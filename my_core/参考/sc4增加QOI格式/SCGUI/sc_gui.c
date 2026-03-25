
#include "sc_gui.h"
sc_gui_t *gui = NULL;

/* 弱定义函数由用户重写*/
__attribute__((weak)) void spiflash_read(uint8_t *buf, uint32_t offset, uint32_t size)
{
    // 支持字体图片读取
}
/* 初始化系统 */
void sc_gui_init(lcd_refresh_cb refresh_cb, color_t bkc, color_t fc, color_t bc, void *font)
{
    static sc_gui_t tft;
    gui = &tft;
    gui->lcd_rect.x = 0;
    gui->lcd_rect.y = 0;
    gui->lcd_rect.w = SC_SCREEN_WIDTH;
    gui->lcd_rect.h = SC_SCREEN_HEIGHT;
    gui->g_mask = NULL;
    gui->refresh_cb = refresh_cb;
    gui->font = font;
    gui->bkc = bkc;
    gui->fc = fc;
    gui->bc = bc;
    gui->alpha = 255;
}

/* 清屏 */
void sc_clear(int x, int y, int w, int h, color_t color)
{
    sc_pfb_t tpfb = {0}; // 临时pfb
    sc_area_t area = {x, y, x + w, y + h};
    sc_pfb_init_slices(&tpfb, &area, color);
    do
    {
    } while (sc_pfb_next_slice(&tpfb));
}
///* 绘制点 */
void sc_draw_point(sc_pfb_t *dest, int x, int y, uint32_t color)
{
    if(sc_rect_touch_ctx((sc_rect_t *)dest,  x,  y))
    {
        x-= dest->x;
        y-= dest->y;
        dest->buf[y * dest->w + x] = color;
    }
}
/* 绘制矩形填充*/
void sc_draw_Fill(sc_pfb_t *dest, int xs, int ys, int w, int h, color_t color, uint16_t alpha)
{
    if (dest == NULL)
    {
        color = alphaBlend(color, gui->bkc, alpha); // 透明度
        sc_clear(xs, ys, w, h, color);
        return;
    }
    sc_area_t intersection;
    sc_area_t area = {xs, ys, xs + w, ys + h};
    if (sc_pfb_intersection(dest, &area, &intersection))
    {
        for (int y = intersection.ys; y < intersection.ye; y++)
        {
            uint16_t *dest_out = &dest->buf[(y - (dest->y)) * dest->w - dest->x + intersection.xs];
            for (int x = intersection.xs; x < intersection.xe; x++)
            {
                alphaBlend_fast(color, dest_out, alpha);
                dest_out++;
            }
        }
    }
}

/* 绘制空芯矩形*/
void sc_draw_Frame(sc_pfb_t *dest, int xs, int ys, int w, int h, int lw, color_t color, uint16_t alpha)
{
    if (w <= lw || h <= lw)
        return;
    sc_draw_Fill(dest, xs, ys, w, lw, color, alpha);                        // 上边框
    sc_draw_Fill(dest, xs, ys + h - lw, w, lw, color, alpha);               // 下边框
    sc_draw_Fill(dest, xs, ys + lw, lw, h - 2 * lw, color, alpha);          // 左边框
    sc_draw_Fill(dest, xs + w - lw, ys + lw, lw, h - 2 * lw, color, alpha); // 右边框
}

/* 绘制反锯齿线*/
void sc_draw_Line(sc_pfb_t *dest, int x1, int y1, int x2, int y2, color_t colour)
{
    if (dest == NULL)
    {
        _sc_draw_Line(x1, y1, x2, y2, colour);
        return;
    }
    sc_area_t intersection;
    int16_t xs = SC_MIN(x1, x2);
    int16_t ys = SC_MIN(y1, y2);
    int16_t dx = x2 - x1;
    int16_t dy = y2 - y1;
    int16_t dxabs = SC_ABS(dx); // 宽度
    int16_t dyabs = SC_ABS(dy); // 高度
    sc_area_t area = {xs, ys, xs + dxabs, ys + dyabs};
    if (!sc_pfb_intersection(dest, &area, &intersection)) // 判断是否在矩形内
    {
        return;
    }
    int16_t sgndx = (dx < 0) ? -1 : 1;                 // 确保x方向正确
    int16_t sgndy = (dy < 0) ? -1 : 1;                 // 确保y方向正确
    int16_t majorStep = dxabs > dyabs ? dxabs : dyabs; // 主方向步长
    int16_t minorStep = dxabs > dyabs ? dyabs : dxabs; // 次方向步长
    int16_t error = majorStep / 2;                     // 初始化误差项
    if (dxabs > dyabs)                                 // 横向线条的插值
    {
        error = minorStep * (intersection.xs - xs) + majorStep / 4; // 0.25
        for (x1 = intersection.xs; x1 < intersection.xe; x1++)
        {
            uint16_t a = (error << 8) / majorStep & 0xff; // 插值计算;
            if ((sgndy * sgndx) > 0)
            {
                y1 = error / majorStep + ys;
            }
            else
            {
                y1 = -error / majorStep + ys + dyabs;
                a = 255 - a; // 插值计算
            }
            int dest_offs = (y1 - dest->y) * dest->w - dest->x + x1;
            uint16_t *dest_out = &dest->buf[dest_offs];
            if (y1 >= intersection.ys && y1 < intersection.ye)
            {
                alphaBlend_fast(colour, dest_out, 255);
            }
            if (y1 + 1 >= intersection.ys && y1 + 1 < intersection.ye)
            {
                alphaBlend_fast(colour, dest_out + dest->w, a);
            }
            if (y1 - 1 >= intersection.ys && y1 - 1 < intersection.ye)
            {
                alphaBlend_fast(colour, dest_out - dest->w, 255 - a);
            }
            error += minorStep;
        }
    }
    else
    {
        error += minorStep * (intersection.ys - ys) + majorStep / 4; // 0.25
        for (y1 = intersection.ys; y1 < intersection.ye; y1++)
        {
            uint16_t a = (error << 8) / majorStep & 0xff; // 插值计算;
            if ((sgndy * sgndx) > 0)
            {
                x1 = error / majorStep + xs;
            }
            else
            {
                x1 = -error / majorStep + xs + dxabs;
                a = 255 - a;
            }
            int dest_offs = (y1 - dest->y) * dest->w - dest->x + x1;
            uint16_t *dest_out = &dest->buf[dest_offs];
            if (x1 >= intersection.xs && x1 < intersection.xe)
            {
                alphaBlend_fast(colour, dest_out, 255);
            }
            if (x1 + 1 >= intersection.xs && x1 + 1 < intersection.xe)
            {
                alphaBlend_fast(colour, dest_out + 1, a);
            }
            if (x1 - 1 >= intersection.xs && x1 - 1 < intersection.xe)
            {
                alphaBlend_fast(colour, dest_out - 1, 255 - a);
            }
            error += minorStep;
        }
    }
}
// 绘制圆角矩形内部函数
bool _draw_Roundrect(sc_pfb_t *dest, int xs, int ys, int w, int h, int r, int ir, color_t color, color_t fill, uint16_t alpha)
{
    sc_pfb_t tpfb; // 临时pfb
    sc_area_t intersection;
    sc_area_t area = {xs, ys, xs + w, ys + h};
    if (!sc_area_intersect_to_parent(&area, gui->g_mask))
        return false;
    if (dest == NULL)
    {
        dest = &tpfb;
        sc_pfb_init_slices(dest, &area, gui->bkc); /// 初始化局部切片
    }
    if (!sc_pfb_intersection(dest, &area, &intersection))
        return false;
    const int ax = xs + r;
    const int bx = xs + w - r - 1;
    const int ay = ys + r;
    const int by = ys + h - r - 1;
    const int left =  ax - ir;
    const int right = bx + ir;
    const bool need_fill = (fill != gui->bkc);
    // 2. 循环外预计算
    int r2 = SC_POW2(r);
    int rmax = SC_POW2(r + 1);
    int ir2 = SC_POW2(ir);
    int rmin = (ir > 0) ? SC_POW2(ir - 1) : 0;
    uint16_t inv_outer = (alpha << 8); // 除法优化
    uint16_t inv_inner = (alpha << 8); // 除法优化
    if (rmax > r2)
    {
        inv_outer = inv_outer / (rmax - r2);
    }
    if (ir2 > rmin)
    {
        inv_inner = inv_inner / (ir2 - rmin);
    }
    do
    {
        uint16_t *dest_buf = dest->buf + (intersection.ys - dest->y) * dest->w - dest->x;
        int x, y;
        for (y = intersection.ys; y < intersection.ye; y++, dest_buf += dest->w)
        {
            uint16_t *dest_out = dest_buf + intersection.xs;
            if (y > ay && y < by) // 中间填充
            {
                for (x = intersection.xs; x < intersection.xe; x++, dest_out++)
                {
                    if (x <= left || x >= right) // 左右垂直线，线宽判断
                    {
                        alphaBlend_fast(color, dest_out, alpha);
                    }
                    else if (need_fill)
                    {
                        alphaBlend_fast(fill, dest_out, alpha);
                    }
                }
                continue;
            }
            int y_rel = y < by ? ay - y : y - by;
            int Ysq = SC_POW2(y_rel); // y_rel^2
            for (x = intersection.xs; x < intersection.xe; x++, dest_out++)
            {
                if (x > ax && x < bx)
                {
                    if (Ysq >= ir2) // 上下水平直线，线宽判断
                    {
                        alphaBlend_fast(color, dest_out, alpha);
                    }
                    else if (need_fill)
                    {
                        alphaBlend_fast(fill, dest_out, alpha);
                    }
                }
                else
                {
                    int x_rel = x < bx ? (ax - x) : (x - bx);
                    int Rsq = SC_POW2(x_rel) + Ysq; // =R*R
                    if (Rsq >= rmax)
                    {
                        x = (x > bx) ? intersection.xe : x;
                    }
                    else if (Rsq > r2)
                    {
                        *dest_out = alphaBlend(color, *dest_out, (rmax - Rsq) * inv_outer >> 8);
                    }
                    else if (Rsq < ir2)
                    {
                        int dist = (Rsq - rmin);
                        if (need_fill)
                        {
                            color_t tbc = fill;
                            if (dist > 0)
                            {
                                tbc = alphaBlend(color, fill, dist * inv_inner >> 8);
                            }
                            *dest_out = alphaBlend(tbc, *dest_out, alpha);
                        }
                        else if (dist > 0)
                        {
                            *dest_out = alphaBlend(color, *dest_out, dist * inv_inner >> 8);
                        }
                    }
                    else
                    {
                        alphaBlend_fast(color, dest_out, alpha);
                    }
                }
            }
        }
    } while ((dest == &tpfb) && sc_pfb_next_slice(dest) && sc_pfb_intersection(dest, &area, &intersection));
    return true;
}
/******绘制圆角矩形**********
 * dest: 目标pfb
 * box: 矩形区域
 * r: 圆角半径
 * ir: 内圆角半径
 * alpha: 透明度
 * color: 边框颜色
 * fill: 填充颜色
 */
bool sc_draw_Rounded_rect(sc_pfb_t *dest, sc_rect_t *box, int r, int ir, color_t color, color_t fill, uint16_t alpha)
{
    return _draw_Roundrect(dest, box->x, box->y, box->w, box->h, r, ir, (ir == 0) ? fill : color, fill, alpha);

}

/*绘制LED*/
void sc_draw_Led(sc_pfb_t *dest, int cx, int cy, int r, color_t color, uint16_t alpha)
{
    sc_rect_t box = {cx - r, cy - r, 2 * r + 1, 2 * r + 1};
    sc_draw_Rounded_rect(dest, &box, r, r, color, color, alpha);
}

/*绘制开关组合绘制*/
void sc_draw_Switch(sc_pfb_t *dest, sc_rect_t *box, int r, int ir, color_t color, color_t fill, uint16_t alpha, uint8_t state)
{
    sc_pfb_t tpfb;
    sc_area_t area = {box->x, box->y, box->x + box->w, box->y + box->h};
    if (dest == NULL)
    {
        dest = &tpfb;
        sc_pfb_init_slices(dest, &area, gui->bkc);
    }
    int lw = r - ir + 1;
    int min = SC_MIN(box->w, box->h);
    sc_rect_t sw = {box->x + lw, box->y + lw, min - 2 * lw, min - 2 * lw};
    uint16_t bc = fill;
    uint16_t bkc = alphaBlend(color, gui->bkc, 128);
    if (state)
    {
        sw.x += box->w - min;
    }
    else
    {
        bc = alphaBlend(fill, 0, 128);
    }
    do
    {
        if (sc_draw_Rounded_rect(dest, box, r, ir, color, bkc, alpha))
        {
            sc_draw_Rounded_rect(dest, &sw, r - lw, 0, bc, bc, alpha);
        }
    } while ((dest == &tpfb) && sc_pfb_next_slice(dest));
}

/**按钮初始化**
 * r: 圆角半径
 * ir: 内圆角半径
 * color: 边框颜色
 * fill: 填充颜色
 * font: 字体
 * text: 文本
 * tc: 文本颜色
 * align: 对齐方式
 */
void sc_init_button(sc_button_t *btn,  lv_font_t *font, const char *text, color_t tc, sc_align_t align)
{
    btn->font = font;
    btn->text = (char *)text;
    btn->tc = tc;
    btn->align = align;
}
// 绘制按钮
void sc_draw_button(sc_pfb_t *dest, sc_rect_t *box, sc_button_t *btn, color_t ac, color_t bc,int r, int ir, uint16_t alpha)
{
    sc_pfb_t tpfb;
    if (dest == NULL)
    {
        dest = &tpfb;
        sc_area_t area = {box->x, box->y, box->x + box->w, box->y + box->h};
        sc_pfb_init_slices(dest, &area, gui->bkc);
    }
    do
    {
        if (sc_draw_Rounded_rect(dest, box, r, ir, ac, bc, alpha))
        {
            sc_draw_str(dest, 0, 0, btn->font, btn->text, btn->tc, gui->bkc, box, btn->align);
        }
    } while ((dest == &tpfb) && sc_pfb_next_slice(dest));
}
/* 绘制圆角进度条****
 * box: 矩形框
 * r: 圆角半径
 * ir: 内圆角半径
 * color: 进度条颜色
 * bkc: 背景颜色
 * alpha: 透明度*/
void sc_draw_Bar(sc_pfb_t *dest, sc_rect_t *box, int r, int ir, color_t color, color_t bkc, uint16_t alpha, int vol)
{
    if (dest)
    {
        if (!sc_rect_nor_intersect((sc_rect_t *)&dest, box))
            return;
    }
    sc_rect_t bar_mask;
    color_t bc = (ir == 0) ? bkc : gui->bkc;
    int rmin = SC_MIN(box->w, box->h) / 2;
    if (r > rmin)
    {
        int lw = rmin - r - ir;
        r = rmin;
        ir = lw > 0 ? lw : r;
    }
    if (box->w >= box->h)
    {
        vol = vol * box->w / 100;
    }
    else
    {
        vol = box->h - vol * box->h / 100;
    }
    gui->g_mask = &bar_mask;
    bar_mask.x = box->x;
    bar_mask.y = box->y;
    if (box->w >= box->h)
    {
        bar_mask.h = box->h;
        bar_mask.w = vol;
        sc_draw_Rounded_rect(dest, box, r, ir, bkc, color, alpha); // 左边
        bar_mask.x = box->x + bar_mask.w;
        bar_mask.w = box->w - bar_mask.w;
        sc_draw_Rounded_rect(dest, box, r, ir, bkc, bc, alpha); // 右边
    }
    else
    {
        bar_mask.w = box->w;
        bar_mask.h = vol;
        sc_draw_Rounded_rect(dest, box, r, ir, bkc, bc, alpha); // 上部
        bar_mask.y = box->y + bar_mask.h;
        bar_mask.h = box->h - bar_mask.h;
        sc_draw_Rounded_rect(dest, box, r, ir, bkc, color, alpha); // 下部
    }
    gui->g_mask = NULL; // 恢复
}

// 文本框进度条
static void sc_text_box_bar(sc_pfb_t *dest, sc_rect_t *box, int r, uint16_t fc, uint16_t bc, uint16_t alpha, int vol)
{
    if (sc_draw_Rounded_rect(dest, box, r, r, bc, bc, alpha)) // 上部
    {
        sc_rect_t bar;
        int max_vol = SC_MAX(box->w, box->h);
        int bar_vol = max_vol >> 1;
        if (vol < 0)
        {
            bar_vol = bar_vol * max_vol / (max_vol - vol); // 进度条压缩
            vol = 0;
        }
        else if (vol + bar_vol >= max_vol)
        {
            bar_vol = bar_vol * max_vol / (vol + bar_vol); // 进度条压缩
            vol = max_vol - bar_vol;
        }
        if (box->w > box->h)
        {
            bar.x = box->x + vol;
            bar.y = box->y;
            bar.w = bar_vol;
            bar.h = box->h;
        }
        else
        {
            bar.x = box->x;
            bar.y = box->y + vol;
            bar.w = box->w;
            bar.h = bar_vol;
        }
        sc_draw_Rounded_rect(dest, &bar, r, r, fc, fc, alpha);
    }
}

/* 初始化文本框***
 *xofs: 文本框x偏移
 *yofs: 文本框y偏移
 *font: 字体
 *text: 文本
 *tc: 文本颜色
 *radius: 圆角半径
 */
void sc_init_Textbox(sc_textbox_t *tbox, int xofs, int yofs, lv_font_t *font, const char *text)
{
    tbox->font = font;
    tbox->text = (char *)text;
    tbox->xofs = xofs;
    tbox->yofs = yofs;
    tbox->align = ALIGN_AUTO_BREAK;
    tbox->r = 6;  
    tbox->bar_w = 6;
}

/* 绘制文本框 */
void sc_draw_Textbox(sc_pfb_t *dest, sc_rect_t *box, sc_textbox_t *tbox,color_t fc, color_t bc)
{
    static int end_y;
    sc_pfb_t tpfb;
    sc_area_t intersection;
    sc_area_t area = {box->x, box->y, box->x + box->w, box->y + box->h};
    if (dest == NULL)
    {
        dest = &tpfb;
        sc_pfb_init_slices(dest, &area, gui->bkc);
    }
    if (dest->y == dest->y_st)
        end_y = dest->y_end;
    if (!sc_pfb_intersection(dest, &area, &intersection))
        return;
    int lw = tbox->r >> 1;
    sc_rect_t text_box = {box->x + lw, box->y + lw, box->w - tbox->bar_w - lw, box->h - 2 * lw}; // 文本框
    sc_rect_t bar = {-1, 0, tbox->bar_w, box->h - tbox->r};                                      // 进度条
    sc_rcet_align(box, &bar, ALIGN_RIGHT | ALIGN_VER);                                           // 进度条右位置
    do
    {
        if (sc_draw_Rounded_rect(dest, box, tbox->r, tbox->r, fc, gui->bkc, 255))
        {
            if (dest->y < end_y)
            {
                end_y = sc_draw_str(dest, tbox->xofs, tbox->yofs, tbox->font, tbox->text, fc, bc, &text_box, ALIGN_AUTO_BREAK);
            }
            if (tbox->bar_w>0)
            {
                sc_text_box_bar(dest, &bar, tbox->bar_w / 2, gui->bc, C_RGB(100, 100, 100), 128, -tbox->yofs);
            }
        }
    } while ((dest == &tpfb) && sc_pfb_next_slice(dest));
}

/* 绘制一个icon字符**
 * dest: 目标pfb
 * xs,ys: 目标位置
 * w,h: 目标大小
 * src: 图像数据
 * bpp: 图像数据位宽
 * fc: 图像颜色
 */
void sc_draw_Icon(sc_pfb_t *dest, int xs, int ys, int w, int h, const uint8_t *src, uint8_t bpp, color_t fc, uint16_t alpha)
{
    if (src == NULL)
        return;
    sc_pfb_t tpfb; // 临时pfb
    sc_area_t intersection;
    sc_area_t area = {xs, ys, xs + w, ys + h};
    if (dest == NULL)
    {
        dest = &tpfb;
        sc_pfb_init_slices(dest, &area, gui->bkc);
    }
    if (!sc_pfb_intersection(dest, &area, &intersection))
        return;
    do
    {
        uint16_t *dest_buf = dest->buf + (intersection.ys - dest->y) * dest->w - dest->x + intersection.xs;
        for (int y = intersection.ys; y < intersection.ye; y++, dest_buf += dest->w)
        {
            int src_offs = (y - ys) * w - xs + intersection.xs;
            uint16_t *dest_out = dest_buf;
            for (int x = intersection.xs; x < intersection.xe; x++, src_offs++)
            {
                if(bpp == 8)
                {
                    alphaBlend_fast(fc, dest_out,src[src_offs]);
                }
                else
                {
                    uint16_t raw= get_bpp_value(src_offs, src, bpp);
                    alphaBlend_fast(fc, dest_out,raw);
                }
                dest_out++;
            }
        }
    } while ((dest == &tpfb) && sc_pfb_next_slice(dest) && sc_pfb_intersection(dest, &area, &intersection));
}

/***绘制图片****
 * dest:  目标pfb
 * xs,ys: 目标位置
 * src: 图像数据
 */
void sc_draw_Image(sc_pfb_t *dest, int xs, int ys, const sc_image_t *src)
{
    sc_pfb_t tpfb; // 临时pfb
    sc_area_t intersection;
    sc_area_t area = {xs, ys, xs + src->w, ys + src->h};
    if (dest == NULL)
    {
        dest = &tpfb;
        sc_pfb_init_slices(dest, &area, gui->bkc);
    }
    if (!sc_pfb_intersection(dest, &area, &intersection))
        return;
    //===========计算相交===============
    do
    {
        uint32_t len = intersection.xe - intersection.xs;
        uint16_t *dest_buf = dest->buf + (intersection.ys - dest->y) * dest->w - dest->x + intersection.xs;
        uint8_t alpha = gui->alpha;
        for (int y = intersection.ys; y < intersection.ye; y++, dest_buf += dest->w)
        {
            int src_offs = (y - ys) * src->w - xs + intersection.xs;
            if (src->exf)
            {
                spiflash_read((uint8_t *)dest_buf, (uintptr_t)src->map + src_offs * 2, len * 2);
                continue;
            }
            //----------源图copy----------
            uint16_t *src_dat = (uint16_t *)src->map + src_offs - intersection.xs;
            uint8_t *mask = (src->mask) ? (uint8_t *)src->mask + src_offs - intersection.xs : NULL;
            uint16_t *dest_out = dest_buf;
            for (int x = intersection.xs; x < intersection.xe; x++)
            {
                *dest_out = (mask) ? alphaBlend(src_dat[x], *dest_out, mask[x]) : src_dat[x];
                dest_out++;
            }
        }
    } while ((dest == &tpfb) && sc_pfb_next_slice(dest) && sc_pfb_intersection(dest, &area, &intersection));
}

// 压缩图片缓存
static inline void zip_map_copy4(uint8_t *dest, const uint8_t *src)
{
    for (int i = 0; i < 4; i++)
    {
        dest[i] = src[i];
    }
}
// 绘制压缩图片,支持外部spi flash
void sc_draw_Image_zip(sc_pfb_t *dest, int xs, int ys, const sc_image_zip *zip, sc_dec_zip *dec)
{
    sc_pfb_t tpfb; // 临时pfb
    sc_area_t intersection;
    sc_area_t area = {xs, ys, xs + zip->w, ys + zip->h};
    if (dest == NULL)
    {
        dest = &tpfb;
        sc_pfb_init_slices(dest, &area, gui->bkc);
    }
    if (!sc_pfb_intersection(dest, &area, &intersection))
    {
        dec->n = 0;
        return;
    }
    if (dest->y == dest->y_st || dec->n == 0) // 首次相交初始化
    {
        dec->x = xs;
        dec->y = ys;
        dec->rep_cnt = 0;
        dec->unzip = 0;
        dec->n = 0;
    }
    do
    {
        uint16_t *dest_buf = dest->buf + (dec->y - dest->y) * dest->w - dest->x;
        uint8_t zip_dat[4];
        while (dec->y < intersection.ye)
        {
            if (!dec->rep_cnt)
            {
                dec->rep_cnt = 1;
                if (zip->exf)
                {
                    spiflash_read(zip_dat, (uintptr_t)zip->map + dec->n, 4); // flash 4字节读取
                }
                else
                {
                    zip_map_copy4(zip_dat, zip->map + (dec->n)); // 4字节读取
                }
                uint16_t b = zip_dat[0];
                if (b & 0x20)
                {
                    b = b | (zip_dat[1] << 8);
                    if (dec->unzip == b)
                    {
                        dec->rep_cnt = (zip_dat[2] << 8) | zip_dat[3]; // 重复的长度
                        dec->n += 2;
                    }
                    else
                    {
                        dec->unzip = b;
                        dec->out = b;
                    }
                    dec->n += 2;
                }
                else // 差值编码rgb232
                {
                    uint16_t r = (b << 5) & 0x1800;
                    uint16_t g = (b << 3) & 0x00e3;
                    dec->out = dec->unzip ^ (r + g + (b & 0x03)); // 差值还原
                    dec->n++;
                }
            }
            while (dec->rep_cnt)
            {
                dec->rep_cnt--;
                if (dec->y >= intersection.ys && dec->x >= intersection.xs && dec->x < intersection.xe)
                {
                    dest_buf[dec->x] = dec->out;
                }
                if (++dec->x >= area.xe)
                {
                    dec->x = xs;
                    dest_buf += dest->w;
                    if (++dec->y >= intersection.ye)
                    {
                        break;
                    }
                }
            }
        }
    } while ((dest == &tpfb) && sc_pfb_next_slice(dest) && sc_pfb_intersection(dest, &area, &intersection));
}

/*波形图数据压入*/
void sc_put_Chart_ch(sc_chart_t *p, int16_t vol, uint16_t scaleX, color_t color)
{
    uint16_t fx = p->indx & 0xff; // 当前下标小数
    p->indx += scaleX;            // 累加坐标
    int wp = p->indx >> 8;        // 当前下标取整
    uint16_t size = sizeof(p->dat_buf) / sizeof(p->dat_buf[0]);
    if (wp >= size)
    {
        wp = 0;
        p->indx = p->indx & 0xff; // 下标保留小数
    }
    int stup = (vol - p->last) << 8;
    for (int i = 1; p->wp != wp; i++)
    {
        p->dat_buf[p->wp] = p->last + stup * (i) / (fx + scaleX); // 插值
        if (++p->wp >= size)
            p->wp = 0;
    }
    p->last = vol;
    p->color = color;
}

// 绘制曲线
void sc_draw_Chart(sc_pfb_t *dest, int xs, int ys, int w, int h, uint16_t gc, int gx, int gy, sc_chart_t *p, int ch)
{
    sc_area_t intersection;
    sc_pfb_t tpfb;
    sc_area_t area = {xs, ys, xs + w, ys + h};
    if (dest == NULL)
    {
        dest = &tpfb;
        sc_pfb_init_slices(dest, &area, gui->bkc);
    }
    if (!sc_pfb_intersection(dest, &area, &intersection))
        return;
    int xe = xs + w - 1;
    int ye = ys + h - 1;
    int base_line_x = w / 2 + xs; // x基准线
    int base_line_y = h / 2 + ys; // y基准线
    int xd = gx;
    int yd = gy;
    uint16_t size = sizeof(p->dat_buf) / sizeof(p->dat_buf[0]);
    do
    {
        int x, y, min, max;
        uint16_t *dest_out;
        for (y = intersection.ys; y < intersection.ye; y++)
        {
            if ((y - base_line_y) % yd == 0 || (y == ys || y == ye))
            {
                bool Solid = (y == base_line_y || y == ys || y == ye) ? 1 : 0; // 实线标志
                dest_out = dest->buf + (y - dest->y) * dest->w + (intersection.xs - dest->x);
                for (x = intersection.xs; x < intersection.xe; x++)
                {
                    *dest_out = (Solid || (x & 0x03) == 0) ? gc : *dest_out; // 水平线
                    dest_out++;
                }
            }
        }
        // 时间轴
        for (x = intersection.xs; x < intersection.xe; x++)
        {
            if ((x - base_line_x) % xd == 0 || x == xs || x == xe)
            {
                bool Solid = (x == base_line_x || x == xs || x == xe) ? 1 : 0; // 实线标志
                dest_out = dest->buf + (intersection.ys - dest->y) * dest->w + x - dest->x;
                for (y = intersection.ys; y < intersection.ye; y++)
                {
                    *dest_out = (Solid || (y & 0x03) == 0) ? gc : *dest_out; ////垂直线
                    dest_out += dest->w;
                }
            }
            if (x >= xe)
                break;
            for (int i = 0; i < ch; i++)
            {
                sc_chart_t *chart = &p[i];
                uint16_t wp = (chart->wp + x - xs) % size;
                uint16_t wp1 = (wp + 1 >= size) ? 0 : wp + 1;
                int16_t v0 = chart->dat_buf[wp];
                int16_t v1 = chart->dat_buf[wp1];
                min = base_line_y - v0;
                max = base_line_y - v1;
                if (min > max)
                {
                    min = max + 1;
                    max = base_line_y - v0;
                }
                else if (min < max)
                {
                    max -= 1;
                }
                min = SC_MAX(min, intersection.ys);
                max = SC_MIN(max, intersection.ye - 1);
                dest_out = dest->buf + (min - dest->y) * dest->w + x - dest->x;
                for (y = min; y <= max; y++)
                {
                    *dest_out = chart->color; // 波形
                    dest_out += dest->w;
                }
            }
        }
    } while ((dest == &tpfb) && sc_pfb_next_slice(dest) && sc_pfb_intersection(dest, &area, &intersection));
}
