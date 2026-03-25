

#include "sc_gui.h"
// 对齐计算
void sc_rcet_align(sc_rect_t *parent, sc_rect_t *rcet, sc_align_t align)
{
    if (parent == NULL)
        return;
    if (align & ALIGN_RIGHT)
    { // 右对齐
        int w = (parent->w - rcet->w);
        rcet->x += (align & ALIGN_LEFT) ? w >> 1 : w;
    }
    if (align & ALIGN_BOTTOM)
    { // 底对齐
        int h = (parent->h - rcet->h);
        rcet->y += (align & ALIGN_TOP) ? h >> 1 : h;
    }
    rcet->x += parent->x;
    rcet->y += parent->y;
}
// 将utf-8编码转为unicode编码
static uint32_t lv_txt_utf8_next(const char *txt, uint32_t *i)
{
    uint32_t result = 0;
    if (txt == NULL)
        return 0;
    uint8_t curr = (uint8_t)txt[*i]; // 统一转uint8_t，避免符号位干扰
    // 1字节ASCII
    if ((curr & 0x80) == 0)
    {
        result = curr;
        (*i)++;
    }
    // 多字节UTF-8解析
    else if ((curr & 0xE0) == 0xC0) // 2字节
    {
        result = (uint32_t)(curr & 0x1F) << 6;
        (*i)++;
        if ((txt[*i] & 0xC0) != 0x80)
            return 0;
        result += (txt[*i] & 0x3F);
        (*i)++;
    }
    else if ((curr & 0xF0) == 0xE0) // 3字节
    {
        result = (uint32_t)(curr & 0x0F) << 12;
        (*i)++;
        if ((txt[*i] & 0xC0) != 0x80)
            return 0;
        result += (uint32_t)(txt[*i] & 0x3F) << 6;
        (*i)++;

        if ((txt[*i] & 0xC0) != 0x80)
            return 0;
        result += (txt[*i] & 0x3F);
        (*i)++;
    }
    else if ((curr & 0xF8) == 0xF0) // 4字节
    {
        result = (uint32_t)(curr & 0x07) << 18;
        (*i)++;
        if ((txt[*i] & 0xC0) != 0x80)
            return 0;
        result += (uint32_t)(txt[*i] & 0x3F) << 12;
        (*i)++;

        if ((txt[*i] & 0xC0) != 0x80)
            return 0;
        result += (uint32_t)(txt[*i] & 0x3F) << 6;
        (*i)++;

        if ((txt[*i] & 0xC0) != 0x80)
            return 0;
        result += txt[*i] & 0x3F;
        (*i)++;
    }
    else // 非法UTF-8编码，跳过当前字节
    {
        (*i)++;
        result = 0;
    }
    return result;
}
/*显示一个lvgl字符*/
void sc_draw_lv_letter(sc_pfb_t *dest, sc_rect_t *box, const lv_font_fmt_txt_glyph_dsc_t *dsc, font_dsc_t *fdsc, color_t tc, color_t bc, sc_rect_t *parent)
{
    sc_pfb_t tpfb;          // 临时pfb
    sc_area_t intersection; // 交集
    sc_area_t area = {box->x, box->y, box->x + box->w, box->y + box->h};
    if (!sc_area_intersect_to_parent(&area, parent)) // 裁剪蒙板区域
        return;
    if (dest == NULL)
    {
        dest = &tpfb;
        sc_pfb_init_slices(dest, &area, gui->bkc);
    }
    if (!sc_pfb_intersection(dest, &area, &intersection))
        return;
    lv_font_t *font = fdsc->font;
    const uint8_t *src = &fdsc->dsc->glyph_bitmap[dsc->bitmap_index];
    const uint8_t bpp = fdsc->dsc->bpp;
    uint16_t alpha = gui->alpha;
    int offs_y = box->y + (font->line_height - dsc->box_h - dsc->ofs_y - font->base_line);
    int offs_x = box->x + dsc->ofs_x;
    int x, y, src_x, src_y;
    do
    {
        uint16_t *dest_out = dest->buf + (intersection.ys - dest->y) * dest->w - dest->x;
        for (y = intersection.ys; y < intersection.ye; y++, dest_out += dest->w)
        {
            src_y = y - offs_y;
            int offset = -1;
            if (src_y >= 0 && src_y < dsc->box_h)
            {
                offset = src_y * dsc->box_w;
            }
            for (x = intersection.xs; x < intersection.xe; x++)
            {
                src_x = x - offs_x;
                if (bc != gui->bkc)
                {
                    dest_out[x] = bc;
                }
                if (offset >= 0 && src_x >= 0 && src_x < dsc->box_w)
                {
                    uint16_t a = get_bpp_value(offset + src_x, src, bpp);
                    alphaBlend_fast(tc, &dest_out[x], a );
                }
            }
        }
    } while ((dest == &tpfb) && sc_pfb_next_slice(dest) && sc_pfb_intersection(dest, &area, &intersection));
}

/* 取得字符串行宽度
glyph_id：记录每个字符的ID
*/
font_dsc_t sc_get_line_width(uint16_t line_width_max, lv_font_t *font, const char *text, uint32_t *indx, unicode_t *glyph_id)
{
    lv_font_glyph_dsc_t gdsc;
    lv_font_fmt_txt_dsc_t *dsc = (lv_font_fmt_txt_dsc_t *)font->dsc;
    font_dsc_t fdsc = {
        .Xspace = dsc->kern_scale >> 8,
        .line_height = font->line_height + (dsc->kern_scale & 0xff),
        .line_width = 0,
        .line_cnt = 0,
        .font = font,
        .dsc = dsc,
    };
    while (fdsc.line_cnt < SC_UINCODE_SIZE)
    {
        unicode_t id = UINCODE_MAX;
        uint32_t prv = *indx;
        uint32_t code = lv_txt_utf8_next(text, indx); // txt转unicode
        if (code == '\n' || code == '\r' || code == 0)
        {
            glyph_id[fdsc.line_cnt++] = id;
            break;
        }
        if (font->get_glyph_dsc(font, &gdsc, code, 0) == 0)
        {
            code = ' '; // 未知字符空格替代
            font->get_glyph_dsc(font, &gdsc, code, 0);
        }
        uint16_t adv_w = gdsc.adv_w + fdsc.Xspace;
        if (fdsc.line_width + adv_w >= line_width_max)
        {
            *indx = prv; // 回退一个字符
            break;
        }
        fdsc.line_width += adv_w;
        glyph_id[fdsc.line_cnt++] = fdsc.dsc->last_glyph_id; // 记录字符ID
    }
    return fdsc;
}
/// 显示字字符串
int sc_draw_str(sc_pfb_t *dest, int tx, int ty, lv_font_t *font, const char *text, uint16_t fc, uint16_t bc, sc_rect_t *parent, sc_align_t align)
{
    if (font == NULL || text == NULL)
        return 0;
    unicode_t glyph_id[SC_UINCODE_SIZE];
    uint32_t indx = 0;
    uint16_t total_height = 0; // 累计行高
    uint16_t line_width_max = (parent && (align & ALIGN_AUTO_BREAK)) ? parent->w : SC_INT16_MAX;
    int end_y = 0;
    do
    {
        font_dsc_t fdsc = sc_get_line_width(line_width_max - tx, font, text, &indx, glyph_id); // 计算行宽度
        sc_rect_t box = (sc_rect_t){.x = tx, .y = ty + total_height, .w = fdsc.line_width, .h = font->line_height};
        sc_rcet_align(parent, &box, align);
        end_y=box.y+box.h;
        if (dest != NULL)
        {
            if (sc_rect_nor_intersect((sc_rect_t *)dest, &box))
            {
                total_height += fdsc.line_height; // 累计总高度+字符统
                continue;
            }
        }
        for (int k = 0; k < fdsc.line_cnt; k++)
        {
            if (glyph_id[k] != UINCODE_MAX)
            {
                lv_font_fmt_txt_glyph_dsc_t gdsc = fdsc.dsc->glyph_dsc[glyph_id[k]]; // 通过字符ID获取字符信息
                box.w = gdsc.adv_w + fdsc.Xspace;
                sc_draw_lv_letter(dest, &box, &gdsc, &fdsc, fc, bc, parent);
                box.x += box.w;
            }
        }
        total_height += fdsc.line_height;
    } while (text[indx - 1] != 0); // 0结束符
    return end_y;
}

/* 显示数字，分子分母形式显示小数
 * dest: 目标pfb
 * tx: tx坐标
 * ty: ty坐标
 * font: 字体
 * num: 分子
 * den: 分母 1,10,100,1000,10000,100000,1000000
 * tc: 字体颜色
 * bc: 边框颜色
 */
void sc_draw_Num(sc_pfb_t *dest, sc_rect_t *box, lv_font_t *font, int num, int den, color_t tc, color_t bc, sc_align_t align)
{
    sc_pfb_t tpfb;
    sc_area_t intersection;
    sc_area_t area = {box->x, box->y, box->x + box->w, box->y + box->h};
    if (dest == NULL)
    {
        dest = &tpfb;
        sc_pfb_init_slices(dest, &area, gui->bkc);
    }
    if (!sc_pfb_intersection(dest, &area, &intersection))
        return;
    char num_str[10];
    char *p = num_str;
    if (num < 0)
    {
        num *= -1;
        *p++ = '-'; //-号
    }
    for (int32_t temp = 100000; temp; temp /= 10)
    {
        if (num >= temp || den >= temp)
        {
            *p++ = num / temp % 10 + '0'; //
        }
        else if (temp < 10)
        {
            *p++ = '0';
        }
        if (den >= 10 && temp == den)
        {
            *p++ = '.'; // 小数点
        }
    }
    *p = '\0';
    do
    {
        sc_draw_str(dest, 0, 0, font, num_str, tc, bc, box, ALIGN_CENTER); // 显示数字
    } while ((dest == &tpfb) && sc_pfb_next_slice(dest) && sc_pfb_intersection(dest, &area, &intersection));
}

/* 标签初始化*****
xofs: 文本X坐标滚动
yofs: 文本Y坐标滚动
align: 对齐方式+扩展功能
align:ALIGN_REVERSE_MODE反色
align:ALIGN_AUTO_BREAK 自动换行
align:ALIGN_BORDER_VISIBLE边框
*/
void sc_init_Label(sc_label_t *label, int xofs, int yofs, lv_font_t *font, const char *text, sc_align_t align)
{

    label->font = font;
    label->text = (char *)text;
    label->xofs = xofs;
    label->yofs = yofs;
    label->align = align;
}
// 绘制标签
void sc_draw_Label(sc_pfb_t *dest, sc_rect_t *box, sc_label_t *label, color_t fc, color_t bc)
{
    sc_pfb_t tpfb;
    sc_area_t intersection;
    sc_area_t area = {box->x, box->y, box->x + box->w, box->y + box->h};
    if (dest == NULL)
    {
        dest = &tpfb;
        sc_pfb_init_slices(dest, &area, gui->bkc);
    }
    if (!sc_pfb_intersection(dest, &area, &intersection))
        return;
    uint16_t tfc = fc;
    uint16_t tbc = bc;
    if (label->align & ALIGN_REVERSE_MODE)
    {
        tbc = fc;
        tfc = bc;
    }
    do
    {
        sc_draw_str(dest, label->xofs, label->yofs, label->font, label->text, tfc, tbc, box, label->align); // 显示数字
        if (label->align & ALIGN_BORDER_VIS)
        {
            sc_draw_Frame(dest, box->x, box->y, box->w, box->h, 1, fc, 255);
        }
    } while ((dest == &tpfb) && sc_pfb_next_slice(dest) && sc_pfb_intersection(dest, &area, &intersection));
}

// 获取字符串光标
bool sc_get_Label_pos(sc_rect_t *parent, sc_label_t *label, sc_lpos_t *pos, int16_t x, int16_t y, int cmd)
{
    if (label->font == NULL || label->text == NULL)
        return false;
    sc_rect_t *ret = (sc_rect_t *)pos; // 返回值
    lv_font_t *font = label->font;
    unicode_t glyph_id[SC_UINCODE_SIZE];
    uint32_t indx = 0;
    uint16_t pos_mark = 0; // 光标位置
    uint16_t total_height = 0;
    uint16_t line_width_max = (parent && (label->align & ALIGN_AUTO_BREAK)) ? parent->w : SC_INT16_MAX;
    do
    {
        font_dsc_t fdsc = sc_get_line_width(line_width_max - label->xofs, font, label->text, &indx, glyph_id);
        int xs = label->xofs;
        int ys = label->yofs;
        sc_parent_align(parent, &xs, &ys, fdsc.line_width, fdsc.line_height, label->align);
        sc_rect_t box = {.x = parent->x, .y = ys + total_height, .w = parent->w, .h = fdsc.line_height};
        if (cmd != 0)
        {
            box.x = xs;
            for (int k = 0; k < fdsc.line_cnt; k++, pos_mark++) // 通过pos_mark查找光标
            {
                if (pos_mark >= pos->mark)
                {
                    box.w = 2;  // 光标宽度2
                    *ret = box; // 找到位置返回
                    return true;
                }
                if (glyph_id[k] != UINCODE_MAX)
                {
                    const lv_font_fmt_txt_glyph_dsc_t *gdsc = &fdsc.dsc->glyph_dsc[glyph_id[k]];
                    box.w = gdsc->adv_w + fdsc.Xspace;
                    box.x += box.w;
                }
            }
            if (label->text[indx - 1] == 0)
            {
                box.w = 2;  // 光标宽度2
                *ret = box; // 找不到返回最后一个
                if (pos->mark >= pos_mark)
                {
                    pos->mark = pos_mark - 1;
                    return false;
                }
                return true;
            }
        }
        else if (sc_rect_touch_ctx(&box, x, y)) // 通过坐标查找光标
        {
            box.x = xs;
            for (int k = 0; k < fdsc.line_cnt; k++)
            {
                if (glyph_id[k] == UINCODE_MAX)
                    continue;
                const lv_font_fmt_txt_glyph_dsc_t *gdsc = &fdsc.dsc->glyph_dsc[glyph_id[k]];
                box.w = gdsc->adv_w + fdsc.Xspace;
                if (x < xs)
                {
                    break; // 如果点在字符外左边返回第一个
                }
                if (sc_rect_touch_ctx(&box, x, y)) // 字符内
                {
                    int right_half = box.x + box.w / 2; // 字符中心
                    if ((x > right_half))
                    {
                        box.x += box.w; // 下一个
                        pos_mark++;
                    }
                    break;
                }
                box.x += box.w;
                pos_mark++;
            }
            pos->mark = pos_mark; // 记录mark下标
            box.w = 2;            // 修改为2
            *ret = box;           // 记录位置
            return true;
        }
        else
        {
            pos_mark += fdsc.line_cnt;
        }
        total_height += fdsc.line_height;
    } while (label->text[indx - 1] != 0); // 判断是否结束符
    return false;
}