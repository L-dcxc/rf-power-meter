#include "sc_keyboard.h"

/*
 *  键盘驱动偷懒实现，借鉴灵动GUI的常量布局https://gitee.com/gzbkey/LingDongGUI
 *  感谢作者开源，请保留作者版权
 */

// 数字键盘按钮信息
#define NUM_COL_NUM (5)
#define NUM_BTN_W_SPACE ((LD_CFG_SCREEN_WIDTH - KB_SPACE) / NUM_COL_NUM)
#define NUM_START ((LD_CFG_SCREEN_WIDTH - NUM_BTN_W_SPACE * NUM_COL_NUM - KB_SPACE) / 2)
#define NUM_BTN_W (NUM_BTN_W_SPACE - KB_SPACE)
#define NUM_BTN_H (((LD_CFG_SCREEN_HEIGHT >> 1) - KB_SPACE) / 4 - KB_SPACE)
#define NUM_OFFSET_W(num) (KB_SPACE + (NUM_BTN_W + KB_SPACE) * num)
#define NUM_OFFSET_H(num) (KB_SPACE + (NUM_BTN_H + KB_SPACE) * num )

const kbBtnInfo_t numBtnInfo[4][5] = {
    {
        {NUM_START + NUM_OFFSET_W(0), NUM_OFFSET_H(0), NUM_BTN_W, NUM_BTN_H, 0, '1'},
        {NUM_START + NUM_OFFSET_W(1), NUM_OFFSET_H(0), NUM_BTN_W, NUM_BTN_H, 0, '2'},
        {NUM_START + NUM_OFFSET_W(2), NUM_OFFSET_H(0), NUM_BTN_W, NUM_BTN_H, 0, '3'},
        {NUM_START + NUM_OFFSET_W(3), NUM_OFFSET_H(0), NUM_BTN_W, NUM_BTN_H, 0, '+'},
        {NUM_START + NUM_OFFSET_W(4), NUM_OFFSET_H(0), NUM_BTN_W, NUM_BTN_H, "<-", KB_VALUE_BACKSPACE},
    },
    {
        {NUM_START + NUM_OFFSET_W(0), NUM_OFFSET_H(1), NUM_BTN_W, NUM_BTN_H, 0, '4'},
        {NUM_START + NUM_OFFSET_W(1), NUM_OFFSET_H(1), NUM_BTN_W, NUM_BTN_H, 0, '5'},
        {NUM_START + NUM_OFFSET_W(2), NUM_OFFSET_H(1), NUM_BTN_W, NUM_BTN_H, 0, '6'},
        {NUM_START + NUM_OFFSET_W(3), NUM_OFFSET_H(1), NUM_BTN_W, NUM_BTN_H, 0, '-'},
        {NUM_START + NUM_OFFSET_W(4), NUM_OFFSET_H(1), NUM_BTN_W, NUM_BTN_H, "!@", KB_VALUE_SYMBOL_MODE},
    },
    {
        {NUM_START + NUM_OFFSET_W(0), NUM_OFFSET_H(2), NUM_BTN_W, NUM_BTN_H, 0, '7'},
        {NUM_START + NUM_OFFSET_W(1), NUM_OFFSET_H(2), NUM_BTN_W, NUM_BTN_H, 0, '8'},
        {NUM_START + NUM_OFFSET_W(2), NUM_OFFSET_H(2), NUM_BTN_W, NUM_BTN_H, 0, '9'},
        {NUM_START + NUM_OFFSET_W(3), NUM_OFFSET_H(2), NUM_BTN_W, NUM_BTN_H, 0, '*'},
        {0, 0, 0, 0, 0, 0},
    },
    {
        {NUM_START + NUM_OFFSET_W(0), NUM_OFFSET_H(3), NUM_BTN_W, NUM_BTN_H, "ABC", KB_VALUE_QWERTY_MODE},
        {NUM_START + NUM_OFFSET_W(1), NUM_OFFSET_H(3), NUM_BTN_W, NUM_BTN_H, 0, '0'},
        {NUM_START + NUM_OFFSET_W(2), NUM_OFFSET_H(3), NUM_BTN_W, NUM_BTN_H, 0, '.'},
        {NUM_START + NUM_OFFSET_W(3), NUM_OFFSET_H(3), NUM_BTN_W, NUM_BTN_H, 0, '/'},
        {NUM_START + NUM_OFFSET_W(4), NUM_OFFSET_H(2), NUM_BTN_W, (NUM_BTN_H << 1) + KB_SPACE, "Enter", KB_VALUE_ENTER},
    }};

// 字母键盘按钮信息
#define QWERT_COL_NUM (10)

#define QWERT_BTN_W_SPACE ((LD_CFG_SCREEN_WIDTH - KB_SPACE) / QWERT_COL_NUM)
#define QWERT_START ((LD_CFG_SCREEN_WIDTH - QWERT_BTN_W_SPACE * QWERT_COL_NUM - KB_SPACE) / 2)
#define QWERT_BTN_W (QWERT_BTN_W_SPACE - KB_SPACE)
#define QWERT_BTN_H (((LD_CFG_SCREEN_HEIGHT >> 1) - KB_SPACE) / 4 - KB_SPACE)
#define QWERT_OFFSET_W(num) (QWERT_BTN_W_SPACE * (num / 2) + KB_SPACE + QWERT_BTN_W * (num % 2) / 2) // (KB_SPACE+((QWERT_BTN_W+KB_SPACE)>>1)*num)
#define QWERT_OFFSET_H(num) (KB_SPACE + (QWERT_BTN_H + KB_SPACE) * num )

const kbBtnInfo_t asciiBtnInfo[4][10] = {
    {
        {QWERT_START + QWERT_OFFSET_W(0), QWERT_OFFSET_H(0), QWERT_BTN_W, QWERT_BTN_H, 0, 'q'},
        {QWERT_START + QWERT_OFFSET_W(2), QWERT_OFFSET_H(0), QWERT_BTN_W, QWERT_BTN_H, 0, 'w'},
        {QWERT_START + QWERT_OFFSET_W(4), QWERT_OFFSET_H(0), QWERT_BTN_W, QWERT_BTN_H, 0, 'e'},
        {QWERT_START + QWERT_OFFSET_W(6), QWERT_OFFSET_H(0), QWERT_BTN_W, QWERT_BTN_H, 0, 'r'},
        {QWERT_START + QWERT_OFFSET_W(8), QWERT_OFFSET_H(0), QWERT_BTN_W, QWERT_BTN_H, 0, 't'},
        {QWERT_START + QWERT_OFFSET_W(10), QWERT_OFFSET_H(0), QWERT_BTN_W, QWERT_BTN_H, 0, 'y'},
        {QWERT_START + QWERT_OFFSET_W(12), QWERT_OFFSET_H(0), QWERT_BTN_W, QWERT_BTN_H, 0, 'u'},
        {QWERT_START + QWERT_OFFSET_W(14), QWERT_OFFSET_H(0), QWERT_BTN_W, QWERT_BTN_H, 0, 'i'},
        {QWERT_START + QWERT_OFFSET_W(16), QWERT_OFFSET_H(0), QWERT_BTN_W, QWERT_BTN_H, 0, 'o'},
        {QWERT_START + QWERT_OFFSET_W(18), QWERT_OFFSET_H(0), QWERT_BTN_W, QWERT_BTN_H, 0, 'p'},
    },
    {
        {QWERT_START + QWERT_OFFSET_W(0), QWERT_OFFSET_H(1), QWERT_BTN_W, QWERT_BTN_H, 0, 'a'},
        {QWERT_START + QWERT_OFFSET_W(2), QWERT_OFFSET_H(1), QWERT_BTN_W, QWERT_BTN_H, 0, 's'},
        {QWERT_START + QWERT_OFFSET_W(4), QWERT_OFFSET_H(1), QWERT_BTN_W, QWERT_BTN_H, 0, 'd'},
        {QWERT_START + QWERT_OFFSET_W(6), QWERT_OFFSET_H(1), QWERT_BTN_W, QWERT_BTN_H, 0, 'f'},
        {QWERT_START + QWERT_OFFSET_W(8), QWERT_OFFSET_H(1), QWERT_BTN_W, QWERT_BTN_H, 0, 'g'},
        {QWERT_START + QWERT_OFFSET_W(10), QWERT_OFFSET_H(1), QWERT_BTN_W, QWERT_BTN_H, 0, 'h'},
        {QWERT_START + QWERT_OFFSET_W(12), QWERT_OFFSET_H(1), QWERT_BTN_W, QWERT_BTN_H, 0, 'j'},
        {QWERT_START + QWERT_OFFSET_W(14), QWERT_OFFSET_H(1), QWERT_BTN_W, QWERT_BTN_H, 0, 'k'},
        {QWERT_START + QWERT_OFFSET_W(16), QWERT_OFFSET_H(1), QWERT_BTN_W, QWERT_BTN_H, 0, 'l'},
        {QWERT_START + QWERT_OFFSET_W(18), QWERT_OFFSET_H(1), QWERT_BTN_W, QWERT_BTN_H, "<-", KB_VALUE_BACKSPACE},
    },
    {

        {QWERT_START + QWERT_OFFSET_W(0), QWERT_OFFSET_H(2), QWERT_BTN_W, QWERT_BTN_H, "!@", KB_VALUE_SYMBOL_MODE},
        {QWERT_START + QWERT_OFFSET_W(2), QWERT_OFFSET_H(2), QWERT_BTN_W, QWERT_BTN_H, 0, 'z'},
        {QWERT_START + QWERT_OFFSET_W(4), QWERT_OFFSET_H(2), QWERT_BTN_W, QWERT_BTN_H, 0, 'x'},
        {QWERT_START + QWERT_OFFSET_W(6), QWERT_OFFSET_H(2), QWERT_BTN_W, QWERT_BTN_H, 0, 'c'},
        {QWERT_START + QWERT_OFFSET_W(8), QWERT_OFFSET_H(2), QWERT_BTN_W, QWERT_BTN_H, 0, 'v'},
        {QWERT_START + QWERT_OFFSET_W(10), QWERT_OFFSET_H(2), QWERT_BTN_W, QWERT_BTN_H, 0, 'b'},
        {QWERT_START + QWERT_OFFSET_W(12), QWERT_OFFSET_H(2), QWERT_BTN_W, QWERT_BTN_H, 0, 'n'},
        {QWERT_START + QWERT_OFFSET_W(14), QWERT_OFFSET_H(2), QWERT_BTN_W, QWERT_BTN_H, 0, 'm'},
        {QWERT_START + QWERT_OFFSET_W(16), QWERT_OFFSET_H(2), QWERT_BTN_W, QWERT_BTN_H, "^", KB_VALUE_UP},
        {QWERT_START + QWERT_OFFSET_W(18), QWERT_OFFSET_H(2), QWERT_BTN_W, QWERT_BTN_H, "\\n", KB_VALUE_NEWLINE},

    },
    {
        {QWERT_START + QWERT_OFFSET_W(0), QWERT_OFFSET_H(3), QWERT_BTN_W, QWERT_BTN_H, "123", KB_VALUE_NUMBER_MODE},
        {QWERT_START + QWERT_OFFSET_W(2), QWERT_OFFSET_H(3), QWERT_BTN_W / 2 + QWERT_BTN_W, QWERT_BTN_H, "shift", KB_VALUE_SHIFT_MODE},
        {QWERT_START + QWERT_OFFSET_W(5), QWERT_OFFSET_H(3), QWERT_BTN_W_SPACE * 5 / 2, QWERT_BTN_H, 0, ' '},
        {QWERT_START + QWERT_OFFSET_W(10), QWERT_OFFSET_H(3), QWERT_BTN_W * 2 + KB_SPACE, QWERT_BTN_H, "Enter", KB_VALUE_ENTER},
        {QWERT_START + QWERT_OFFSET_W(14), QWERT_OFFSET_H(3), QWERT_BTN_W, QWERT_BTN_H, "<", KB_VALUE_LEFT},
        {QWERT_START + QWERT_OFFSET_W(16), QWERT_OFFSET_H(3), QWERT_BTN_W, QWERT_BTN_H, "v", KB_VALUE_DOWN},
        {QWERT_START + QWERT_OFFSET_W(18), QWERT_OFFSET_H(3), QWERT_BTN_W, QWERT_BTN_H, ">", KB_VALUE_RIGHT},

        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
    },
};

// 符号键盘按钮信息
const kbBtnInfo_t symbolBtnInfo[4][10] = {
    {
        {QWERT_START + QWERT_OFFSET_W(0), QWERT_OFFSET_H(0), QWERT_BTN_W, QWERT_BTN_H, 0, '-'},
        {QWERT_START + QWERT_OFFSET_W(2), QWERT_OFFSET_H(0), QWERT_BTN_W, QWERT_BTN_H, 0, '/'},
        {QWERT_START + QWERT_OFFSET_W(4), QWERT_OFFSET_H(0), QWERT_BTN_W, QWERT_BTN_H, 0, ':'},
        {QWERT_START + QWERT_OFFSET_W(6), QWERT_OFFSET_H(0), QWERT_BTN_W, QWERT_BTN_H, 0, ';'},
        {QWERT_START + QWERT_OFFSET_W(8), QWERT_OFFSET_H(0), QWERT_BTN_W, QWERT_BTN_H, 0, '('},
        {QWERT_START + QWERT_OFFSET_W(10), QWERT_OFFSET_H(0), QWERT_BTN_W, QWERT_BTN_H, 0, ')'},
        {QWERT_START + QWERT_OFFSET_W(12), QWERT_OFFSET_H(0), QWERT_BTN_W, QWERT_BTN_H, 0, '_'},
        {QWERT_START + QWERT_OFFSET_W(14), QWERT_OFFSET_H(0), QWERT_BTN_W, QWERT_BTN_H, 0, '$'},
        {QWERT_START + QWERT_OFFSET_W(16), QWERT_OFFSET_H(0), QWERT_BTN_W, QWERT_BTN_H, 0, '&'},
        {QWERT_START + QWERT_OFFSET_W(18), QWERT_OFFSET_H(0), QWERT_BTN_W, QWERT_BTN_H, 0, '"'},
    },
    {
        {QWERT_START + QWERT_OFFSET_W(0), QWERT_OFFSET_H(1), QWERT_BTN_W, QWERT_BTN_H, 0, '['},
        {QWERT_START + QWERT_OFFSET_W(2), QWERT_OFFSET_H(1), QWERT_BTN_W, QWERT_BTN_H, 0, ']'},
        {QWERT_START + QWERT_OFFSET_W(4), QWERT_OFFSET_H(1), QWERT_BTN_W, QWERT_BTN_H, 0, '{'},
        {QWERT_START + QWERT_OFFSET_W(6), QWERT_OFFSET_H(1), QWERT_BTN_W, QWERT_BTN_H, 0, '}'},
        {QWERT_START + QWERT_OFFSET_W(8), QWERT_OFFSET_H(1), QWERT_BTN_W, QWERT_BTN_H, 0, '#'},
        {QWERT_START + QWERT_OFFSET_W(10), QWERT_OFFSET_H(1), QWERT_BTN_W, QWERT_BTN_H, 0, '%'},
        {QWERT_START + QWERT_OFFSET_W(12), QWERT_OFFSET_H(1), QWERT_BTN_W, QWERT_BTN_H, 0, '^'},
        {QWERT_START + QWERT_OFFSET_W(14), QWERT_OFFSET_H(1), QWERT_BTN_W, QWERT_BTN_H, 0, '*'},
        {QWERT_START + QWERT_OFFSET_W(16), QWERT_OFFSET_H(1), QWERT_BTN_W, QWERT_BTN_H, 0, '+'},
        {QWERT_START + QWERT_OFFSET_W(18), QWERT_OFFSET_H(1), QWERT_BTN_W, QWERT_BTN_H, 0, '='},
    },
    {
        {QWERT_START + QWERT_OFFSET_W(0), QWERT_OFFSET_H(2), QWERT_BTN_W, QWERT_BTN_H, 0, '\\'},
        {QWERT_START + QWERT_OFFSET_W(2), QWERT_OFFSET_H(2), QWERT_BTN_W, QWERT_BTN_H, 0, '|'},
        {QWERT_START + QWERT_OFFSET_W(4), QWERT_OFFSET_H(2), QWERT_BTN_W, QWERT_BTN_H, 0, '<'},
        {QWERT_START + QWERT_OFFSET_W(6), QWERT_OFFSET_H(2), QWERT_BTN_W, QWERT_BTN_H, 0, '>'},
        {QWERT_START + QWERT_OFFSET_W(8), QWERT_OFFSET_H(2), QWERT_BTN_W, QWERT_BTN_H, 0, '~'},
        {QWERT_START + QWERT_OFFSET_W(10), QWERT_OFFSET_H(2), QWERT_BTN_W, QWERT_BTN_H, 0, ','},
        {QWERT_START + QWERT_OFFSET_W(12), QWERT_OFFSET_H(2), QWERT_BTN_W, QWERT_BTN_H, 0, '@'},
        {QWERT_START + QWERT_OFFSET_W(14), QWERT_OFFSET_H(2), QWERT_BTN_W, QWERT_BTN_H, 0, '!'},
        {QWERT_START + QWERT_OFFSET_W(16), QWERT_OFFSET_H(2), QWERT_BTN_W, QWERT_BTN_H, 0, '`'},
        {QWERT_START + QWERT_OFFSET_W(18), QWERT_OFFSET_H(2), QWERT_BTN_W, QWERT_BTN_H, 0, '\''},
    },
    {
        {QWERT_START + QWERT_OFFSET_W(0), QWERT_OFFSET_H(3), QWERT_BTN_W * 2 + KB_SPACE, QWERT_BTN_H, "ABC", KB_VALUE_QWERTY_MODE},
        {QWERT_START + QWERT_OFFSET_W(4), QWERT_OFFSET_H(3), QWERT_BTN_W, QWERT_BTN_H, 0, '.'},
        {QWERT_START + QWERT_OFFSET_W(6), QWERT_OFFSET_H(3), QWERT_BTN_W_SPACE * 3 + QWERT_BTN_W, QWERT_BTN_H, 0, ' '},
        {QWERT_START + QWERT_OFFSET_W(14), QWERT_OFFSET_H(3), QWERT_BTN_W, QWERT_BTN_H, 0, '?'},
        {QWERT_START + QWERT_OFFSET_W(16), QWERT_OFFSET_H(3), QWERT_BTN_W * 2 + KB_SPACE, QWERT_BTN_H, "<-", KB_VALUE_BACKSPACE},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0},
    },
};

/******************************************************************************************
 * 初始化键盘类型
 ******************************************************************************************/
void sc_init_keyboard(kb_ctx_t *kb, uint8_t type)
{
    kb->curr_type = type;
    kb->row_num = 4; // 所有键盘固定4行
    kb->now = NULL;  // 当前选中的按键
    switch (type)
    {
    case KB_TYPE_NUM:
        // 数字键盘：5列
        kb->kb_tab = (kbBtnInfo_t *)numBtnInfo;
        kb->col_num = 5;
        break;
    case KB_TYPE_SHIFT:
        kb->upper = 1 - kb->upper;
    case KB_TYPE_ASCII:
        // 字母键盘：10列
        kb->kb_tab = (kbBtnInfo_t *)asciiBtnInfo;
        kb->col_num = 10;
        break;

    case KB_TYPE_SYMBOL:
        // 符号键盘：10列
        kb->kb_tab = (kbBtnInfo_t *)symbolBtnInfo;
        kb->col_num = 10;
        break;
    default:
        break;
    }
}

#define to_upper(c) ((c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c) // 大小写转换
// 获取按键值
uint8_t sc_get_kb_vol(const kbBtnInfo_t *p, uint8_t upper)
{
    return upper ? to_upper(p->value) : p->value; // 大写
}
// 绘制单个按键
void sc_draw_kb_btn(sc_pfb_t *dest, int xs, int ys, const kbBtnInfo_t *p, color_t tc, color_t kc, uint8_t upper)
{
    if (p->region.w == 0 || p->region.h == 0)
        return;
    sc_rect_t coord =p->region;
    coord.x+=xs;
    coord.y+=ys;
    sc_button_t btn;
    sc_init_button(&btn ,  gui->font,p->pText, tc, ALIGN_CENTER);
    if (p->pText==NULL)
    {
        char vol[2] = {0};
        vol[0] = upper ? to_upper(p->value) : p->value; // 大写
        btn.text = vol;
    }
    sc_draw_button(dest, &coord, &btn,kc, kc, 4,4,255);
}

// 绘制键盘
void sc_draw_keyboard(sc_pfb_t *dest,sc_rect_t *box, kb_ctx_t *p, color_t keyc,color_t tc,color_t bkc)
{
    int xs = box->x;
    int ys = box->y;
    sc_draw_Rounded_rect(dest, box, 5, 5, keyc, bkc, 255);      // 绘制键盘背景
    // 3. 遍历当前键盘的坐标+文本数组，绘制所有按键
    for (int y = 0; y < p->row_num; y++)
    {
        for (int x = 0; x < p->col_num; x++)
        {
            const kbBtnInfo_t *kb = &p->kb_tab[y * p->col_num + x];
            color_t bc = (kb == p->now) ? C_RED : keyc;
            sc_draw_kb_btn(dest, xs, ys, kb, tc, bc, p->upper);
        }
    }
}


