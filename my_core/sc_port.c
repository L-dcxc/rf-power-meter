#include "sc_port.h"
#include "ST7567A.h"
#include "stm32f1xx_hal.h"

/* ----------------------------------------------------------------
 * LCD 刷新回调
 * SCGUI 每完成一个 PFB 切片就调用此函数，将像素数据送到屏幕
 * 参数:
 *   x, y  : 左上角坐标
 *   w, h  : 宽高（像素）
 *   color : 像素数组，格式 RGB565 (uint16_t)
 * ---------------------------------------------------------------- */
static void sc_lcd_refresh(uint16_t x, uint16_t y, uint16_t w, uint16_t h, color_t *color)
{
    uint32_t n = (uint32_t)w * h;
    LCD_SetWindows(x, y, x + w - 1, y + h - 1);
    LCD_A0(1);

    /* ARM little-endian → LCD big-endian: swap each pixel's bytes in-place */
    uint8_t *p = (uint8_t *)color;
    for (uint32_t i = 0; i < n; i++) {
        uint8_t t  = p[2 * i];
        p[2 * i]   = p[2 * i + 1];
        p[2 * i + 1] = t;
    }

    /* Single bulk SPI transfer — ~70x faster than per-pixel calls */
    HAL_SPI_Transmit(&hspi1, p, n * 2, HAL_MAX_DELAY);
}

/* ----------------------------------------------------------------
 * SCGUI 初始化
 * 在 LCD_Init() 完成之后调用一次
 * ---------------------------------------------------------------- */
void SC_Port_Init(void)
{
    sc_gui_init(sc_lcd_refresh,
                C_BLACK,        /* 背景色 */
                C_WHITE,        /* 前景色 */
                C_ROYAL_BLUE,   /* 辅助色（按钮等默认底色）*/
                &lv_font_12);   /* 默认字体 */

    sc_clear(0, 0, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT, C_BLACK);
}

/* ----------------------------------------------------------------
 * Tick 同步（在主循环每次迭代开头调用）
 * 将 HAL ms 计数映射给 SCGUI 任务调度器
 * ---------------------------------------------------------------- */
void SC_Port_Tick(void)
{
    extern uint32_t system_tick;
    system_tick = HAL_GetTick();
}

/* ----------------------------------------------------------------
 * 按键注入
 * key_id: SC_KEY_OK / SC_KEY_UP / SC_KEY_DOWN
 * ---------------------------------------------------------------- */
void SC_Port_InjectKey(uint8_t key_id)
{
    switch (key_id)
    {
        case SC_KEY_UP:   sc_send_cmd_event(CMD_UP);    break;
        case SC_KEY_DOWN: sc_send_cmd_event(CMD_DOWN);  break;
        case SC_KEY_OK:   sc_send_cmd_event(CMD_ENTER); break;
        default: break;
    }
}
