#ifndef SC_LVGL_FONT_H
#define SC_LVGL_FONT_H

#include "sc_common.h"


/* 对齐方式枚举 */
typedef uint8_t sc_align_t;
enum
{
    ALIGN_NONE = 0x00,                      // 无对齐
    ALIGN_LEFT = (1 << 0),                  // 左对齐
    ALIGN_RIGHT = (1 << 1),                 // 右对齐
    ALIGN_TOP = (1 << 2),                   // 上对齐
    ALIGN_BOTTOM = (1 << 3),                // 下对齐
    ALIGN_HOR = (ALIGN_LEFT | ALIGN_RIGHT), // 水平对齐
    ALIGN_VER = (ALIGN_TOP | ALIGN_BOTTOM), // 垂直对齐
    ALIGN_CENTER = (ALIGN_HOR | ALIGN_VER), // 居中对齐
    ALIGN_AUTO_BREAK = (1 << 4),            // 自动换行
    ALIGN_REVERSE_MODE = (1 << 5),          // 反色模式
    ALIGN_BORDER_VIS = (1 << 6),            // 边框可见
};

/*矩形对齐计算*/
void sc_rcet_align(sc_rect_t *parent, sc_rect_t *rcet, sc_align_t align);
/*坐标对齐计算*/
static inline void sc_parent_align(sc_rect_t *parent, int *xs, int *ys, int w, int h, sc_align_t align)
{
    sc_rect_t rcet = {*xs, *ys, w, h};
    sc_rcet_align(parent, &rcet, align);
    *xs = rcet.x;
    *ys = rcet.y;
}

// 字体信息类型定义
typedef struct
{
    uint8_t Xspace;             // 字符间距
    uint8_t line_height;        // 字体高度
    uint16_t line_width;        // 字符串总宽
    uint16_t line_cnt;          // 字符串总字符数
    lv_font_t *font;            // 字体
    lv_font_fmt_txt_dsc_t *dsc; // 字体描述符
} font_dsc_t;

/* 设置字体间隙距 */
static inline void sc_set_font_space(lv_font_t *font, uint8_t xspace, uint8_t yspace)
{
    lv_font_fmt_txt_dsc_t *dsc = (lv_font_fmt_txt_dsc_t *)font->dsc;
    dsc->kern_scale = (xspace << 8) | yspace;
}
/* 获取字体点阵 */
static inline uint16_t get_bpp_value(uint32_t offset,const uint8_t *src, const uint8_t bpp)
{
    uint16_t alpha=0;
    switch (bpp)
    {
    case 8:
        return (src[offset]);
    case 4:
        alpha= (src[offset >>1] >> ((1 - (offset & 1))*bpp )) & 0x0F;
        return (alpha << 4) + alpha;  // 替代 alpha *= 17
    case 2:
        alpha= (src[offset >>2] >> ((3 - (offset & 3))*bpp)) & 0x03;
        return alpha*85;
    case 1:
        alpha= (src[offset >>3] >> (7 - (offset & 7))) & 0x01 ? 255 : 0;
        return  alpha;
    }
    return  0;
}
/*label结构体定义*/
typedef struct
{
    lv_font_t *font;
    char *text;
    int32_t xofs : 12;
    int32_t yofs : 12;
    int32_t align : 8;
} sc_label_t;

/*label光标结构体定义 */
typedef struct
{
    int16_t x;
    int16_t y;
    uint16_t w;
    uint16_t h;
    uint16_t mark;
} sc_lpos_t;

// 获取字符串行宽度
font_dsc_t sc_get_line_width(uint16_t line_width_max, lv_font_t *font, const char *text, uint32_t *indx, unicode_t *glyph_id);

// 显示字符串
int sc_draw_str(sc_pfb_t *dest, int tx, int ty, lv_font_t *font, const char *text, uint16_t fc, uint16_t bc, sc_rect_t *parent, sc_align_t align);

// 显示数字
void sc_draw_Num(sc_pfb_t *dest, sc_rect_t *box, lv_font_t *font, int num, int den, color_t tc, color_t bc, sc_align_t align);

/* 初始化标签*/
void sc_init_Label(sc_label_t *label, int tx, int ty, lv_font_t *font, const char *text,  sc_align_t align);
/* 显示标签*/
void sc_draw_Label(sc_pfb_t *dest, sc_rect_t *box, sc_label_t *label,color_t fc, color_t bc);

bool sc_get_Label_pos(sc_rect_t *parent, sc_label_t *label, sc_lpos_t *pos, int16_t x, int16_t y, int cmd);
// 显示文本
static inline void sc_draw_Text(sc_pfb_t *dest, int tx, int ty, lv_font_t *font, const char *text, color_t tc, color_t bc)
{
    sc_draw_str(dest, tx, ty, font, text, tc, bc, NULL, 0);
}
#endif
