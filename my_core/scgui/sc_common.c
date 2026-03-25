
#include "sc_gui.h"

/* ---------- 全局变量 ---------- */
static SC_ALIGN_ARRAY(color_t, g_pfb_buf, SC_PFB_BUF_SIZE, 4);            // PFB缓存区
static SC_ALIGN_ARRAY(sc_area_t, sc_dirty_buckets, SC_DIRTY_BUCKET_N, 4); // 脏矩阵静态池
static uint8_t sc_dirty_bucket_cnt = 0;                                   // 脏矩阵计数

// 脏矩形阀值合并
static bool _dirty_overlap_merged(const sc_area_t *a, const sc_area_t *b, sc_area_t *merged, int16_t threshold)
{
    // 1. 快速排斥
    if (a->xe <= b->xs || a->xs >= b->xe || a->ye <= b->ys || a->ys >= b->ye)
    {
        return false;
    }
    // 2. 计算相交区域判断是否满足合并条件
    const int16_t intersect_xs = SC_MAX(a->xs, b->xs);
    const int16_t intersect_ys = SC_MAX(a->ys, b->ys);
    const int16_t intersect_xe = SC_MIN(a->xe, b->xe);
    const int16_t intersect_ye = SC_MIN(a->ye, b->ye);
    const int16_t overlap_area = (intersect_xe - intersect_xs) * (intersect_ye - intersect_ys);
    // 条件A: 重叠区域超过阈值
    // 条件B: 一个矩形完全包含另一个
    bool is_merged = (overlap_area >= threshold) ||
                     (a->xs <= b->xs && a->ys <= b->ys && a->xe >= b->xe && a->ye >= b->ye) ||
                     (b->xs <= a->xs && b->ys <= a->ys && b->xe >= a->xe && b->ye >= a->ye);
    if (is_merged)
    {
        merged->xs = SC_MIN(a->xs, b->xs);
        merged->ys = SC_MIN(a->ys, b->ys);
        merged->xe = SC_MAX(a->xe, b->xe);
        merged->ye = SC_MAX(a->ye, b->ye);
    }
    return is_merged;
}

// 脏矩形入队列
void sc_dirty_mark(const sc_rect_t *parent, const sc_rect_t *dirty)
{
    sc_area_t area;
    // 裁剪到全屏+父级
    if (!sc_rect_intersect_to_area(&gui->lcd_rect, dirty, &area) || !sc_area_intersect_to_parent(&area, parent)) 
        return;
    // 动态调整合并阈值：当桶池使用超过一半时，降低合并门槛以防止溢出
    int16_t threshold = (sc_dirty_bucket_cnt >= SC_DIRTY_BUCKET_N / 2) ? 0 : SC_DIRTY_AREA_THRESHOLD;
    // 倒序遍历，从最近添加的矩形开始检查
    for (int i = sc_dirty_bucket_cnt - 1; i >= 0; i--)
    {
        if (_dirty_overlap_merged(&sc_dirty_buckets[i], &area, &sc_dirty_buckets[i], threshold))
        {
            return; // 合并成功
        }
    }
    if (sc_dirty_bucket_cnt < SC_DIRTY_BUCKET_N)
    {
        sc_dirty_buckets[sc_dirty_bucket_cnt++] = area; // 将新矩形添加到新桶中
    }
    else
    {
        sc_area_t *merged = &sc_dirty_buckets[0]; // 桶满强制合并到第一个
        merged->xs = SC_MIN(merged->xs, area.xs);
        merged->ys = SC_MIN(merged->ys, area.ys);
        merged->xe = SC_MAX(merged->xe, area.xe);
        merged->ye = SC_MAX(merged->ye, area.ye);
    }

}

// 脏矩形全局合并。返回实际有效数量
sc_area_t *sc_dirty_merge_out(uint8_t *valid_cnt)
{
    if (sc_dirty_bucket_cnt == 0)
        return NULL;
    sc_area_t *out = sc_dirty_buckets;
    *valid_cnt = sc_dirty_bucket_cnt;
    if (sc_dirty_bucket_cnt > 3)
    {
        /* 全局合并：滚动缓冲区 + 置空已合并 */
        uint8_t write = 0;
        for (uint8_t i = 0; i < sc_dirty_bucket_cnt; ++i)
        {
            bool is_merged = false;              // 合并标记，
            sc_area_t cur = sc_dirty_buckets[i]; // 当前待合并矩形
            for (uint8_t j = 0; j < write; ++j)
            {
                if (_dirty_overlap_merged(&cur, &sc_dirty_buckets[j], &sc_dirty_buckets[j], SC_DIRTY_AREA_THRESHOLD))
                {
                    is_merged = true; // 标记“已合并”
                    break;
                }
            }
            if (!is_merged)
            {
                sc_dirty_buckets[write++] = cur; // 向前移动
            }
        }
        *valid_cnt = write;
    }
    sc_dirty_bucket_cnt = 0; // 清空桶计数
    return out;
}

/*=====================分帧缓管理=================================*/

/*刷新当前切片到屏幕*/
static void sc_pfb_refresh(uint16_t xs, uint16_t ys, uint16_t w, uint16_t h, color_t *color)
{
#if SC_LCD_DMA_WAP
    uint32_t len = w * h;
    uint32_t n32 = len >> 1; // 对半数量
    uint32_t *p32 = (uint32_t *)color;
    while (n32--)
    {
        uint32_t v = *p32;
        v = ((v & 0x00FF00FF) << 8) | ((v & 0xFF00FF00) >> 8);
        *p32++ = v;
    }
    /* 2. 奇数长度？剩最后一个 u16 */
    if (len & 1)
    { // 最低位 1 表示奇数
        uint16_t v = *(uint16_t *)p32;
        *(uint16_t *)p32 = (v << 8) | (v >> 8);
    }
#endif
    if (gui->refresh_cb)
    {
        gui->refresh_cb(xs, ys, w, h, color);
    }
}

/*当前切片填充颜色*/
static void sc_pfb_memset(sc_pfb_t *dest)
{

#if SC_LCD_DMA_2BUF
    static uint8_t addr = 0; // 双缓切换
    dest->buf = (addr & 0x01) ? &g_pfb_buf[SC_PFB_BUF_SIZE >> 1] : g_pfb_buf;
    addr ^= 0x01;
#else
    dest->buf = g_pfb_buf; // 指向全局PFB缓存
#endif
    uint32_t total_pixels = dest->w * dest->h;
    uint32_t batch_color = ((uint32_t)dest->color << 16) | dest->color; // 预计算批量颜色
    color_t *p = dest->buf;
    for (uint32_t i = 0; i < total_pixels / 2; i++)
    {
        *((uint32_t *)p) = batch_color;
        p += 2; // 指针跳过 2 个像素
    }
    if (total_pixels & 0x01)
    {
        *p = dest->color;
    }
}

/*初始化第一个切片*/
bool sc_pfb_init_slices(sc_pfb_t *dest, sc_area_t *dirty, color_t color)
{
    dest->x = SC_MAX(0, dirty->xs);
    dest->y = SC_MAX(0, dirty->ys);
    int xe = SC_MIN(SC_SCREEN_WIDTH, dirty->xe);
    int ye = SC_MIN(SC_SCREEN_HEIGHT, dirty->ye);
    if (dest->x >= xe || dest->y >= ye)
    {
        dest->w = 0;
        return false;
    }
    int width = xe - dest->x;
    int height = ye - dest->y;
    // 切片起始Y坐标
    dest->y_st = dest->y;           // 切片起始Y坐标
    dest->y_end = dest->y + height; // 切片结束Y坐标
    dest->w = width;
    dest->h = height;
    // 计算核心切片行高
    height = (SC_PFB_BUF_SIZE / width) >> SC_LCD_DMA_2BUF;
    dest->h = SC_MIN(dest->h, height); //
    dest->color = color;
    dest->parent = NULL;
    sc_pfb_memset(dest);
    return true;
}

// pfb相交判断
bool sc_pfb_intersection(const sc_pfb_t *dest, const sc_area_t *area, sc_area_t *out)
{
    sc_area_t pfb = {dest->x, dest->y, dest->x + dest->w, dest->y + dest->h};
    if (sc_area_nor_intersect(&pfb, area))
    {
        return false;
    }
    //------------相交于PFB----------
    out->xs = SC_MAX(area->xs, pfb.xs);
    out->ys = SC_MAX(area->ys, pfb.ys);
    out->xe = SC_MIN(area->xe, pfb.xe);
    out->ye = SC_MIN(area->ye, pfb.ye);
    return sc_area_intersect_to_parent(out, dest->parent);
}

/*切换下一个切片*/
bool sc_pfb_next_slice(sc_pfb_t *dest)
{
    if (dest->w == 0)
    {
        return false;
    }
    sc_pfb_refresh(dest->x, dest->y, dest->w, dest->h, dest->buf);
    dest->y += dest->h;
    if (dest->y >= dest->y_end) // 检查是否还有剩余区域
    {
        return false; // 没有下一个切片了
    }
    uint32_t remaining_height = dest->y_end - dest->y;
    dest->h = SC_MIN(remaining_height, dest->h);
    sc_pfb_memset(dest); // 填充下一个切片
    return true;
}

// 快速运算alpha算法32位MCU更快
uint16_t alphaBlend(uint16_t fc, uint16_t bc, uint16_t alpha)
{
    if (alpha > 251)
    {
        return fc;
    }
    else if (alpha < 4)
    {
        return bc;
    }
    uint32_t bc_Alpha = (bc | (bc << 16)) & 0x7E0F81F;
    uint32_t fc_Alpha = ((fc | (fc << 16)) & 0x7E0F81F) - bc_Alpha;
    uint32_t result = (bc_Alpha + (fc_Alpha * (alpha >> 3) >> 5)) & 0x7E0F81F;
    return (result & 0xFFFF) | (result >> 16);
}

///**fun:打点画线*/
void _sc_draw_Line(int x1, int y1, int x2, int y2, uint16_t colour)
{
    int dx = x2 - x1;
    int dy = y2 - y1;
    int sgndx = (dx < 0) ? -1 : 1;                // 确保x方向正确
    int sgndy = (dy < 0) ? -1 : 1;                // 确保y方向正确
    int dxabs = SC_ABS(dx);                       // 宽度
    int dyabs = SC_ABS(dy);                       // 高度
    uint8_t isHorizontal = dxabs > dyabs;         // 判断主方向
    int majorStep = isHorizontal ? dxabs : dyabs; // 主方向步长
    int minorStep = isHorizontal ? dyabs : dxabs; // 次方向步长
    int error = 0;                                // majorStep/2;  // 初始化误差项
    uint16_t abuf[3];
    for (int i = 0; i <= majorStep; i++)
    {
        int a = (error << 8) / majorStep; // 插值计算
        abuf[0] = alphaBlend(colour, gui->bkc, 255 - a);
        abuf[1] = colour;
        abuf[2] = alphaBlend(colour, gui->bkc, a);
        if (isHorizontal) // 横向线条的插值
        {
            if (sgndy < 0)
            {
                uint16_t temp = abuf[2];
                abuf[2] = abuf[0];
                abuf[0] = temp;
            }
            sc_pfb_refresh(x1, y1 - 1, 1, 3, abuf);
            x1 += sgndx; // 更新x坐标
        }
        else // 纵向线条的插值
        {
            if (sgndx < 0)
            {
                uint16_t temp = abuf[2];
                abuf[2] = abuf[0];
                abuf[0] = temp;
            }
            sc_pfb_refresh(x1 - 1, y1, 3, 1, abuf);
            y1 += sgndy; // 更新y坐标
        }
        // 更新误差值
        error += minorStep;
        if (error >= majorStep)
        {
            error -= majorStep;
            // 更新次方向坐标
            if (isHorizontal)
            {
                y1 += sgndy; // 横向时调整y
            }
            else
            {
                x1 += sgndx; // 纵向时调整x
            }
        }
    }
}

static const int16_t sin0_90_table[] = {
    0, 572, 1144, 1715, 2286, 2856, 3425, 3993, 4560, 5126, 5690, 6252, 6813, 7371, 7927, 8481,
    9032, 9580, 10126, 10668, 11207, 11743, 12275, 12803, 13328, 13848, 14364, 14876, 15383, 15886, 16383, 16876,
    17364, 17846, 18323, 18794, 19260, 19720, 20173, 20621, 21062, 21497, 21925, 22347, 22762, 23170, 23571, 23964,
    24351, 24730, 25101, 25465, 25821, 26169, 26509, 26841, 27165, 27481, 27788, 28087, 28377, 28659, 28932, 29196,
    29451, 29697, 29934, 30162, 30381, 30591, 30791, 30982, 31163, 31335, 31498, 31650, 31794, 31927, 32051, 32165,
    32269, 32364, 32448, 32523, 32587, 32642, 32687, 32722, 32747, 32762, 32767};
/*******************************************
 * Return with sinus of an angle
 * @param angle
 * @return sinus of 'angle'. sin(-90) = -32767, sin(90) = 32767
 */
int32_t sc_sin(int16_t angle)
{
    angle %= 360;
    if (angle < 0) angle += 360;
    if (angle < 90)  return sin0_90_table[angle];
    if (angle < 180) return sin0_90_table[180 - angle];
    if (angle < 270) return -sin0_90_table[angle - 180];
    return -sin0_90_table[360 - angle];
}
/*******************************************
 * Return with cos of an angle
 * @param angle
 * @return cos of 'angle'. sin(-90) = -32767, sin(90) = 32767
 */
int32_t sc_cos(int16_t angle)
{
    return sc_sin(angle + 90);
}

// Compute the fixed point square root of an integer and
// return the 8 MS bits of fractional part.
// Quicker than sqrt() for processors that do not have an FPU (e.g. RP2040)
uint8_t sc_sqrt(uint32_t num)
{
    if (num > (0x40000000))
        return 0;
    uint32_t bsh = 0x00004000;
    uint32_t fpr = 0;
    uint32_t osh = 0;
    uint32_t bod;
    // Auto adjust from U8:8 up to U15:16
    while (num > bsh)
    {
        bsh <<= 2;
        osh++;
    }
    do
    {
        bod = bsh + fpr;
        if (num >= bod)
        {
            num -= bod;
            fpr = bsh + bod;
        }
        num <<= 1;
    } while (bsh >>= 1);
    return fpr >> osh;
}
