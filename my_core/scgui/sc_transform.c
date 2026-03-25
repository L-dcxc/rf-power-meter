
#include "sc_gui.h"

// 回调函数指针
typedef void (*Bilinear_cb)(Transform_t *p, int32_t x, int32_t y, const uint8_t fx, const uint8_t fy, uint16_t *dst);
// 图片插值算法
static inline void _Bilinear_Image(Transform_t *p, int32_t x, int32_t y, const uint8_t fx, const uint8_t fy, uint16_t *dst)
{
    sc_image_t *image = (sc_image_t *)p->src;
    uint16_t *map = (uint16_t *)image->map;
    uint8_t *mask = (uint8_t *)image->mask;
    uint16_t bg = *dst;
    uint16_t w = image->w;
    int16_t xe = w - 1;
    int16_t ye = image->h - 1;
    int32_t offs = y * w + x;

    uint16_t p00 = (x >= 0 && y >= 0) ? map[offs] : bg;
    uint16_t p01 = (x < xe && y >= 0) ? map[offs + 1] : bg;
    uint16_t p10 = (y < ye && x >= 0) ? map[offs + w] : bg;
    uint16_t p11 = (y < ye && x < xe) ? map[offs + w + 1] : bg;
    if (mask)
    {
        if(p00!=bg) {p00 =alphaBlend(p00, bg, mask[offs]);}          // 左上角像素
        if(p01!=bg) {p01 =alphaBlend(p01, bg, mask[offs + 1]);}      // 右上角像素
        if(p10!=bg) {p10 =alphaBlend(p10, bg, mask[offs + w]); }     // 左下角像素
        if(p11!=bg) {p11 =alphaBlend(p11, bg, mask[offs + w + 1]);}  // 右下角像素
    }
    else
    {
        p00 = (p00) ? p00 : bg; // 左上角像素
        p01 = (p01) ? p01 : bg; // 右上角像素
        p10 = (p10) ? p10 : bg; // 左下角像素
        p11 = (p11) ? p11 : bg; // 右下角像素
    }
    // if (p->sinA == 0 || p->cosA == 32767)
    // {
    //     *dst = alphaBlend(p10, p00, fy); // 在Y方向进行权重插值，返回最终结果;
    //     return;
    // }
    // 在X方向进行权重插值
    uint16_t y0 = alphaBlend(p01, p00, fx); // 左上和右上的插值
    uint16_t y1 = alphaBlend(p11, p10, fx); // 左下和右下的插值
    *dst = alphaBlend(y1, y0, fy);          // 在Y方向进行权重插值，返回最终结果
}

// 字体插值算法
static inline void _Bilinear_font(Transform_t *p, int32_t x, int32_t y, const uint8_t fx, const uint8_t fy, uint16_t *dst)
{
    sc_image_t *image = (sc_image_t *)p->src;
    uint8_t *mask = (uint8_t *)image->mask;
    const uint8_t bpp = image->bpp;
    int32_t offs = y * image->w + x;
    const int16_t xe = image->w - 2;
    const int16_t ye = image->h - 2;
    uint8_t p00 = (x < 0 || y < 0) ? 0 : get_bpp_value(offs, mask, bpp);                  // 左上角像素
    uint8_t p01 = (x > xe || y < 0) ? 0 : get_bpp_value(offs + 1, mask, bpp);             // 右上角像素
    uint8_t p10 = (y > ye || x < 0) ? 0 : get_bpp_value(offs + image->w, mask, bpp);      // 左下角像素
    uint8_t p11 = (y > ye || x > xe) ? 0 : get_bpp_value(offs + image->w + 1, mask, bpp); // 右下角像素

    // 在X方向进行权重插值
    const uint16_t y0 = (uint32_t)p00 * (255U - fx) + (uint32_t)p01 * fx;
    const uint16_t y1 = (uint32_t)p10 * (255U - fx) + (uint32_t)p11 * fx;
    const uint16_t ret = (y0 * (255U - fy) + y1 * fy) >> 16;
    *dst = alphaBlend(gui->fc, *dst, ret);
}

/*初始化源图，默认为源图中心点
box:包围盒
src:源图
p:结构体指针
*/
void sc_init_transform(sc_rect_t *box, void *src, Transform_t *p)
{
    sc_image_t *image = (sc_image_t *)src;
    p->box = box;               // 包围盒
    p->src = src;               // 源图
    p->center_x = image->w * 4; // 源图中心放大8倍*0.5 
    p->center_y = image->h * 4; // 源图中心放大8倍*0.5 
    p->scaleX = 256;            // X缩放放大256倍
    p->scaleY = 256;            // Y缩放放大256倍
}

//// 设置缩放
 void sc_set_transform_scale( Transform_t *p,float scaleX, float scaleY)
{
    int16_t sx = scaleX*256;
    int16_t sy = scaleY*256;
    p->scaleX = (sx>0) ? sx: 1; // 不能为0
    p->scaleY = (sy>0) ? sy : 1; // 不能为0;  
}
/// 设置旋转中心
 void sc_set_transform_center( Transform_t *p,float centerX, float centerY)
{
    p->center_x = centerX*8;
    p->center_y = centerY*8;
}

/* 设置旋转角度
move_x 画布中心点X
move_Y 画布中心点X
Angle：旋转角度 0~360
*/
void sc_set_transform_angle(int move_x, int move_y, int Angle, Transform_t *p)
{
    if (p == NULL || p->src == NULL)
        return;
    const sc_image_t *image = (sc_image_t *)p->src;
    const int16_t w = image->w << 3;
    const int16_t h = image->h << 3;
    // 源图4顶点（左包坐标系：0~w, 0~h）
    const int16_t v[4][2] = {{0, 0}, {w, 0}, {0, h}, {w, h}};
    // 统一矩阵符号：通过sinA符号控制旋转方向
    p->sinA = sc_sin(Angle);
    p->cosA = sc_cos(Angle);
    int32_t m00 = (p->cosA * p->scaleX) >> 8;  // x轴：cos(θ)*Kx（Q15）
    int32_t m01 = (-p->sinA * p->scaleY) >> 8; // y轴：-sin(θ)*Ky（Q15）
    int32_t m10 = (p->sinA * p->scaleX) >> 8;  // x轴： sin(θ)*Kx（Q15）
    int32_t m11 = (p->cosA * p->scaleY) >> 8;  // y轴：cos(θ)*Ky（Q15）
    //-----------旋转后的窗口------------------
    int32_t pivot_x = move_x << 15;
    int32_t pivot_y = move_y << 15;
    int32_t min_x, max_x, min_y, max_y;
    for (int i = 0; i < 4; i++)
    {
        int32_t x = v[i][0] - p->center_x;
        int32_t y = v[i][1] - p->center_y;
        int32_t dx = ((x * m00 + y * m01) >> 3) + pivot_x;
        int32_t dy = ((x * m10 + y * m11) >> 3) + pivot_y;
        if (i == 0)
        {
            min_x = dx;
            max_x = dx;
            min_y = dy;
            max_y = dy;
        }
        else
        {
            min_x = SC_MIN(min_x, dx);
            min_y = SC_MIN(min_y, dy);
            max_x = SC_MAX(max_x, dx);
            max_y = SC_MAX(max_y, dy);
        }
    }
    min_x = min_x >> 15;
    min_y = min_y >> 15;
    max_x = (max_x >> 15) + 1; 
    max_y = (max_y >> 15) + 1; 
    // 合并包围盒
    p->drity.x = SC_MIN(min_x, p->box->x);
    p->drity.y = SC_MIN(min_y, p->box->y);
    p->drity.w = SC_MAX(max_x, p->box->x + p->box->w) - p->drity.x;
    p->drity.h = SC_MAX(max_y, p->box->y + p->box->h) - p->drity.y;

    p->box->x = min_x;
    p->box->y = min_y;
    p->box->w = max_x - min_x;
    p->box->h = max_y - min_y;
}
/* 旋转缩放绘制*/
void sc_draw_transform(sc_pfb_t *dest, int move_x, int move_y, Transform_t *p)
{
    sc_image_t *image = (sc_image_t *)p->src;
    sc_pfb_t tpfb;          // 临时pfb
    sc_area_t intersection; // 交集
    sc_area_t area = sc_rect_to_area(p->box);
    if (dest == NULL)
    {
        dest = &tpfb;
        sc_area_t drity = sc_rect_to_area(&p->drity);
        sc_pfb_init_slices(dest, &drity, gui->bkc);
    }
    do
    {
        if (sc_pfb_intersection(dest, &area, &intersection)) // 判断是否有交集
        {
            int32_t center_x = (int32_t)p->center_x << 12; // 中心点
            int32_t center_y = (int32_t)p->center_y << 12; // 中心点
            int32_t sinA = (int32_t)p->sinA << 8;
            int32_t cosA = (int32_t)p->cosA << 8;
            // 反映射缩放系数是除
            int32_t m00 = (cosA / p->scaleX);
            int32_t m01 = (sinA / p->scaleX);
            int32_t m10 = -(sinA / p->scaleY);
            int32_t m11 = (cosA / p->scaleY);
            //----------逆旋转公式------
            int32_t Cx = m01 * (intersection.ys - move_y) + center_x;
            int32_t Cy = m11 * (intersection.ys - move_y) + center_y;
            // 目标图像包围盒遍历
            uint16_t *dest_out = dest->buf + (intersection.ys - dest->y) * dest->w - dest->x;
            Bilinear_cb Bilinear_fun = (image->map != NULL) ? _Bilinear_Image : _Bilinear_font;
            for (int y = intersection.ys; y < intersection.ye; y++, dest_out += dest->w)
            {
                //----------逆旋转公式------
                int32_t xd = intersection.xs - move_x;
                int32_t rotatedX = m00 * xd + Cx; // 初始x项（仅计算一次乘法）
                int32_t rotatedY = m10 * xd + Cy; // 初始y项（仅计算一次乘法）
                bool _end = 0;
                for (int x = intersection.xs; x < intersection.xe; x++)
                {
                    int32_t sx = rotatedX >> 15;
                    int32_t sy = rotatedY >> 15;
                    if (sx >= -1 && sx < image->w && sy >= -1 && sy < image->h) // 边界钳位+采样
                    {
                        uint8_t fx = (rotatedX & 0x7FFF) >> 7;
                        uint8_t fy = (rotatedY & 0x7FFF) >> 7;
                        Bilinear_fun(p, sx, sy, fx, fy, &dest_out[x]);
                        _end = 1;
                    }
                    else if (_end)
                    {
                        break;
                    }
                    rotatedX += m00;
                    rotatedY += m10;
                }
                Cx += m01;
                Cy += m11;
            }
#if 0
            sc_draw_Frame(dest, p->box->x,p->box->y,p->box->w,p->box->h, 1, C_RED, 255); // 调试边框
            sc_draw_point(dest, move_x, move_y, C_RED);
#endif
        }
    } while ((dest == &tpfb) && sc_pfb_next_slice(dest));
}

// copy字体像素值（解析LVGL）
static inline void copy_bpp_value(uint8_t *dst, uint32_t dst_offs,
                                  const uint8_t *src, uint32_t src_offs,
                                  uint8_t bpp)
{
    if (bpp >= 8)
    { // 快速路径：8bpp 直接复制
        ((uint8_t *)dst)[dst_offs] = src[src_offs];
        return;
    }
    const uint8_t pix_per_byte = 8 / bpp;
    const uint8_t pix_mask = pix_per_byte - 1;
    const uint8_t bpp_mask = (1U << bpp) - 1;
    const uint8_t bpp_shift = 3 - (bpp >> 1);
    // 读取源像素
    const uint32_t src_byte_idx = src_offs >> bpp_shift;
    const uint8_t src_pix_idx = src_offs & pix_mask;
    const uint8_t src_bit_offset = (pix_per_byte - 1 - src_pix_idx) * bpp;
    const uint8_t src_byte = src[src_byte_idx];
    const uint8_t raw_pix = (src_byte >> src_bit_offset) & bpp_mask;
    // 写入目标像素
    uint8_t *dst_writable = (uint8_t *)dst;
    const uint32_t dst_byte_idx = dst_offs >> bpp_shift;
    const uint8_t dst_pix_idx = dst_offs & pix_mask;
    const uint8_t dst_bit_offset = (pix_per_byte - 1 - dst_pix_idx) * bpp;
    dst_writable[dst_byte_idx] &= ~(bpp_mask << dst_bit_offset);
    dst_writable[dst_byte_idx] |= (raw_pix << dst_bit_offset);
}
/*内部函数lvgl字符转mask用于字符串缩放*/
static void _label_to_mask(sc_rect_t *box, const lv_font_fmt_txt_glyph_dsc_t *dsc, const font_dsc_t *fdsc, uint8_t *mask)
{
    lv_font_t *font = fdsc->font;
    int16_t offs_y = box->y + (font->line_height - dsc->box_h - dsc->ofs_y - font->base_line);
    int16_t offs_x = box->x + dsc->ofs_x;
    int16_t x, y, src_x, src_y;
    uint32_t dest_offs = box->y * fdsc->line_width;
    const uint8_t *src = &fdsc->dsc->glyph_bitmap[dsc->bitmap_index];
    for (y = box->y; y < box->y + box->h; y++, dest_offs += fdsc->line_width)
    {
        src_y = y - offs_y;
        for (x = box->x; x < box->x + box->w; x++)
        {
            src_x = x - offs_x;
            if (src_x >= 0 && src_x < dsc->box_w && src_y >= 0 && src_y < dsc->box_h)
            {
                uint32_t src_offs = src_y * dsc->box_w + src_x;
                copy_bpp_value(mask, dest_offs + x, src, src_offs, fdsc->dsc->bpp);
            }
        }
    }
}

/* 设置label图像用于缩放
merge: 合并后的包围盒
label: 标签
cx,cy: 中心点
imge:  label生成的图像
p: 参数
*/
void sc_init_transform_text(sc_rect_t *rect, lv_font_t *font, const char *text, void *imge, Transform_t *p)
{
    if (font == NULL || text == NULL || imge == NULL)
        return;
    sc_image_t *label_imge = (sc_image_t *)imge;
    // ----------初始化参数----------------
    unicode_t fid[SC_UINCODE_SIZE]; // 字体信息
    uint32_t indx = 0;
    font_dsc_t fdsc = sc_get_line_width(0xffff, font, text, &indx, fid);
    size_t size = (fdsc.line_width * font->line_height) * fdsc.dsc->bpp / 8; // 内存大小
    void *mask = (void *)label_imge->mask;
    mask = sc_malloc(size);
    if (mask == NULL)
        return;
    sc_memset(mask, 0, size);
    // ----------生成labelmask----------------
    sc_rect_t box = {.x = 0, .y = 0, .w = 0, .h = fdsc.line_height}; // 字体包围盒
    for (int k = 0; k < fdsc.line_cnt; k++)
    {
        if (fid[k] == UINCODE_MAX)
            continue;
        lv_font_fmt_txt_glyph_dsc_t gdsc = fdsc.dsc->glyph_dsc[fid[k]];
        box.w = gdsc.adv_w + fdsc.Xspace;
        _label_to_mask(&box, &gdsc, &fdsc, mask);
        box.x += box.w;
    }
    label_imge->map = NULL; // 清空map
    label_imge->mask = mask;
    label_imge->w = fdsc.line_width;   // 设置宽度
    label_imge->h = font->line_height; // 设置高度
    label_imge->bpp = fdsc.dsc->bpp;   // 设置bpp
    sc_init_transform(rect, imge, p);
}
