#ifndef SC_KEYBOARD_H
#define SC_KEYBOARD_H

/*
 *  键盘驱动偷懒实现，借鉴灵动GUI的常量布局https://gitee.com/gzbkey/LingDongGUI
 *  感谢作者开源，请保留作者版权
 */
#include "sc_gui.h"

//-----------------------
#define LD_CFG_SCREEN_WIDTH (SC_SCREEN_WIDTH)   // 屏幕宽度
#define LD_CFG_SCREEN_HEIGHT (SC_SCREEN_HEIGHT) // 屏幕高度
#define KB_SPACE (SC_SCREEN_WIDTH / 80)

#define KB_VALUE_NONE (0x00)      // 无按键
#define KB_VALUE_BACKSPACE (0x08) // 退格键
#define KB_VALUE_NEWLINE (0x0a)   // 换行键
#define KB_VALUE_ENTER (0x0D)     // 回车键

#define KB_VALUE_DEL (0x7F)         // 删除键
#define KB_VALUE_NUMBER_MODE (0x80) // 数字键盘模式
#define KB_VALUE_QWERTY_MODE (0x81) // 字母键盘模式
#define KB_VALUE_SYMBOL_MODE (0x82) // 符号键盘模式
#define KB_VALUE_SHIFT_MODE (0x83)  // 大写键盘模式

#define KB_VALUE_UP (0x85)    // 上
#define KB_VALUE_DOWN (0x86)  // 下
#define KB_VALUE_LEFT (0x87)  // 左
#define KB_VALUE_RIGHT (0x88) // 右

typedef struct
{
    sc_rect_t region;
    const char *pText;
    uint8_t value;
} kbBtnInfo_t;

typedef enum
{
    KB_TYPE_NUM = 0,    // 数字键盘
    KB_TYPE_ASCII = 1,  // 字母键盘
    KB_TYPE_SYMBOL = 2, // 符号键盘
    KB_TYPE_SHIFT = 3   // 大写键盘
} KB_TYPE_E;

typedef struct
{
    const kbBtnInfo_t *kb_tab; // 当前键盘文本数组指针（兼容4列/10列）
    const kbBtnInfo_t *now;    // 当前按键指针
    uint8_t curr_type;         // 当前键盘类型（数字/字母/符号）
    uint8_t col_num;           // 当前键盘列数（4列/10列）
    uint8_t row_num;           // 当前键盘行数（固定4行）
    uint8_t upper;             // 大写键盘模式
} kb_ctx_t;

uint8_t sc_get_kb_vol(const kbBtnInfo_t *p, uint8_t upper);

void sc_init_keyboard(kb_ctx_t *kb, uint8_t type);

void sc_draw_keyboard(sc_pfb_t *dest,sc_rect_t *box, kb_ctx_t *p, color_t keyc,color_t tc,color_t bkc);

// 获取按键坐标
static inline sc_rect_t sc_get_kb_pos(const kbBtnInfo_t *p, int xs, int ys)
{
    return (sc_rect_t){p->region.x + xs, p->region.y + ys, p->region.w, p->region.h};
}

#endif // KEYBOARD_LAYOUT_H
