#include "ui_menu.h"
#include "sc_port.h"
#include "sc_common.h"
#include "interface_manager.h"
#include "eeprom_24c16.h"
#include "ui_main.h"
#include "stm32f1xx_hal.h"
#include <stdio.h>

/* ---- common colors ---- */
#define MENU_BG    C_BLACK
#define MENU_SEL   C_ROYAL_BLUE
#define MENU_HINT  C_DIM_GRAY

/* ---- utility: draw title bar directly to LCD (called from INIT) ---- */
static void draw_title(const char *text)
{
    sc_draw_Fill(NULL, 0, 0, SC_SCREEN_WIDTH, 17, MENU_SEL, 255);
    sc_rect_t tb = {0, 0, SC_SCREEN_WIDTH, 17};
    sc_draw_str(NULL, 0, 2, &lv_font_12, text, C_WHITE, MENU_SEL, &tb, ALIGN_CENTER);
}

/* ==================================================================
 *  Main menu (4 items) — animated sliding cursor bar
 *
 *  y=0..16    title bar
 *  y=17..40   item 0  (ITEM_H=24, lv_font_20)
 *  y=41..64   item 1
 *  y=65..88   item 2
 *  y=89..112  item 3
 *  y=116..127 hint
 *
 *  Left cursor bar (CURSOR_W px) slides via exponential easing.
 *  Text rects start at x=CURSOR_W+2 so the bar never overlaps text.
 * ================================================================== */
static const char * const MENU_ITEMS[] = {
    "Brightness Set",
    "Calibration",
    "Alarm Setup",
    "About Device",
    "Diagnostics",
    "< Back"
};
#define MENU_COUNT       6
#define VISIBLE_COUNT    4
#define ITEM_Y0          22
#define ITEM_H           24
#define BAR_X            6     /* bar left margin */
#define BAR_PAD          12    /* extra width padding around text */
#define ANIM_MS          300   /* animation duration */
#define SB_W             3     /* scrollbar width px */
#define SB_X             (SC_SCREEN_WIDTH - SB_W - 1)

/* easeOutCubic: fast start, slow finish */
static float ease_out_cubic(float t)
{
    float u = 1.0f - t;
    return 1.0f - u * u * u;
}

/* Measure pixel width of ASCII string in given font */
static uint16_t text_width(lv_font_t *font, const char *s)
{
    lv_font_glyph_dsc_t g;
    uint16_t w = 0;
    while (*s) {
        if (font->get_glyph_dsc(font, &g, (uint32_t)(uint8_t)*s, 0))
            w += g.adv_w;
        s++;
    }
    return w;
}

static int8_t  g_menu_cursor = 0;
static int8_t  g_viewport_start = 0;
static uint16_t g_item_w[MENU_COUNT];  /* text widths, filled in INIT */

/* animation state */
static uint32_t g_anim_start = 0;
static float    g_from_y = ITEM_Y0;
static float    g_from_w = 0;
static float    g_to_y   = ITEM_Y0;
static float    g_to_w   = 0;

static void start_anim(int8_t new_cursor, float cur_y, float cur_w)
{
    if (new_cursor >= g_viewport_start + VISIBLE_COUNT)
        g_viewport_start = new_cursor - VISIBLE_COUNT + 1;
    else if (new_cursor < g_viewport_start)
        g_viewport_start = new_cursor;

    g_from_y     = cur_y;
    g_from_w     = cur_w;
    g_to_y       = (float)(ITEM_Y0 + (new_cursor - g_viewport_start) * ITEM_H);
    g_to_w       = (float)(g_item_w[new_cursor] + BAR_PAD);
    g_menu_cursor = new_cursor;
    g_anim_start  = HAL_GetTick();
}

static void draw_menu_items(sc_pfb_t *pfb, float bar_y, float bar_w)
{
    int i;
    int by = (int)(bar_y + 0.5f);
    int bw = (int)(bar_w + 0.5f);
    int visible_row = g_menu_cursor - g_viewport_start;
    int target_y = ITEM_Y0 + visible_row * ITEM_H;

    /* 1. Visible items — black bg, white text (leave scrollbar column) */
    for (i = 0; i < VISIBLE_COUNT; i++) {
        int item_idx = g_viewport_start + i;
        int iy = ITEM_Y0 + i * ITEM_H;
        sc_draw_Fill(pfb, 0, iy, SB_X, ITEM_H, MENU_BG, 255);
        sc_draw_str(pfb, 10, iy + 2, &lv_font_20, MENU_ITEMS[item_idx],
                    C_WHITE, MENU_BG, NULL, ALIGN_NONE);
    }

    /* 2. Animated white pill bar (variable width) */
    sc_draw_Fill(pfb, BAR_X, by, bw, ITEM_H, C_WHITE, 255);

    /* 3. Target item text in black on top of bar */
    sc_draw_str(pfb, 10, target_y + 2, &lv_font_20, MENU_ITEMS[g_menu_cursor],
                C_BLACK, C_WHITE, NULL, ALIGN_NONE);

    /* 4. Scrollbar (right edge) */
    {
        int track_h = VISIBLE_COUNT * ITEM_H;
        int thumb_h = (track_h * VISIBLE_COUNT) / MENU_COUNT;
        int max_vp  = MENU_COUNT - VISIBLE_COUNT;
        int thumb_y = ITEM_Y0 + (g_viewport_start * (track_h - thumb_h)) / max_vp;
        sc_draw_Fill(pfb, SB_X, ITEM_Y0, SB_W, track_h, C_DIM_GRAY, 255);
        sc_draw_Fill(pfb, SB_X, thumb_y, SB_W, thumb_h, C_WHITE, 255);
    }
}

void ui_menu_task(sc_event_t *e)
{
    switch (e->type)
    {
        case SC_EVENT_TYPE_INIT:
        {
            int j;
            for (j = 0; j < MENU_COUNT; j++)
                g_item_w[j] = text_width(&lv_font_20, MENU_ITEMS[j]);
            g_menu_cursor = 0;
            g_viewport_start = 0;
            g_from_y = g_to_y = (float)ITEM_Y0;
            g_from_w = g_to_w = (float)(g_item_w[0] + BAR_PAD);
            g_anim_start = HAL_GetTick();
            sc_clear(0, 0, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT, MENU_BG);
            draw_title("Menu");
            break;
        }

        case SC_EVENT_TYPE_TIMER:
        {
            uint32_t elapsed = HAL_GetTick() - g_anim_start;
            float t = (float)elapsed / (float)ANIM_MS;
            if (t > 1.0f) t = 1.0f;
            float et = ease_out_cubic(t);
            float cur_y = g_from_y + (g_to_y - g_from_y) * et;
            float cur_w = g_from_w + (g_to_w - g_from_w) * et;

            sc_pfb_t pfb;
            sc_area_t dyn = {0, 17, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT};
            sc_pfb_init_slices(&pfb, &dyn, MENU_BG);
            do {
                draw_menu_items(&pfb, cur_y, cur_w);
            } while (sc_pfb_next_slice(&pfb));
            break;
        }

        case SC_EVENT_TYPE_CMD:
        {
            int cmd = e->dat.cmd;
            {
                uint32_t el = HAL_GetTick() - g_anim_start;
                float t  = (el >= ANIM_MS) ? 1.0f : ease_out_cubic((float)el / ANIM_MS);
                float cy = g_from_y + (g_to_y - g_from_y) * t;
                float cw = g_from_w + (g_to_w - g_from_w) * t;
                if (cmd == CMD_BACK) {
                    sc_create_task(0, ui_main_task, 100);
                } else if (cmd == CMD_UP) {
                    int8_t nc = g_menu_cursor - 1;
                    if (nc < 0) nc = MENU_COUNT - 1;
                    start_anim(nc, cy, cw);
                } else if (cmd == CMD_DOWN) {
                    int8_t nc = (g_menu_cursor + 1) % MENU_COUNT;
                    start_anim(nc, cy, cw);
                } else if (cmd == CMD_ENTER) {
                    switch (g_menu_cursor) {
                        case 0: sc_create_task(0, ui_brightness_task,  20); break;
                        case 3: sc_create_task(0, ui_about_task,      100); break;
                        case 4: sc_create_task(0, ui_debug_task,       50); break;
                        case 5: sc_create_task(0, ui_main_task,       100); break;
                        default: break;
                    }
                }
            }
            break;
        }

        default:
            break;
    }
}

/* ==================================================================
 *  Brightness screen
 *
 *  Level: 1-10 (maps to InterfaceManager_SetBrightness 1-10)
 *  y=48..64   "Level: X / 10"
 *  y=70..79   progress bar (10 cells)
 *  y=116..127 hint
 * ================================================================== */
#define BRIGHT_CELL_W   12
#define BRIGHT_CELL_H   10
#define BRIGHT_CELL_GAP  2
#define BRIGHT_LEVELS   10
/* bar total = 10*12 + 9*2 = 138; start x = (161-138)/2 = 11 */
#define BRIGHT_BAR_X    11
#define BRIGHT_BAR_Y    70

static void draw_brightness(sc_pfb_t *pfb, uint8_t level)
{
    char buf[20];
    int i;

    sprintf(buf, "Level: %u / %u", (unsigned)level, (unsigned)BRIGHT_LEVELS);
    sc_rect_t lbox = {0, 48, SC_SCREEN_WIDTH, 17};
    sc_draw_str(pfb, 0, 0, &lv_font_16, buf, C_WHITE, MENU_BG, &lbox, ALIGN_CENTER);

    for (i = 0; i < BRIGHT_LEVELS; i++) {
        color_t cc = (i < (int)level) ? C_ROYAL_BLUE : C_DIM_GRAY;
        sc_draw_Fill(pfb,
                     BRIGHT_BAR_X + i * (BRIGHT_CELL_W + BRIGHT_CELL_GAP),
                     BRIGHT_BAR_Y,
                     BRIGHT_CELL_W, BRIGHT_CELL_H, cc, 255);
    }

    sc_rect_t hint = {0, 116, SC_SCREEN_WIDTH, 12};
    sc_draw_str(pfb, 0, 0, &lv_font_12, "+   OK:Save/Back  -",
                MENU_HINT, MENU_BG, &hint, ALIGN_CENTER);
}

void ui_brightness_task(sc_event_t *e)
{
    static uint8_t s_level;

    switch (e->type)
    {
        case SC_EVENT_TYPE_INIT:
            s_level = g_interface_manager.brightness_level;
            if (s_level < 1 || s_level > BRIGHT_LEVELS) s_level = 5;
            sc_clear(0, 0, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT, MENU_BG);
            draw_title("Brightness");
            break;

        case SC_EVENT_TYPE_TIMER:
        {
            sc_pfb_t pfb;
            sc_area_t dyn = {0, 17, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT};
            sc_pfb_init_slices(&pfb, &dyn, MENU_BG);
            do {
                draw_brightness(&pfb, s_level);
            } while (sc_pfb_next_slice(&pfb));
            break;
        }

        case SC_EVENT_TYPE_CMD:
        {
            int cmd = e->dat.cmd;
            if (cmd == CMD_UP || cmd == CMD_BACK) {
                if (s_level > 1) s_level--; else s_level = BRIGHT_LEVELS;
                InterfaceManager_SetBrightness(s_level);
            } else if (cmd == CMD_DOWN) {
                if (s_level < BRIGHT_LEVELS) s_level++; else s_level = 1;
                InterfaceManager_SetBrightness(s_level);
            } else if (cmd == CMD_ENTER) {
                g_interface_manager.brightness_level = s_level;
                BL24C16_Write(0x0000, &s_level, 1);
                sc_create_task(0, ui_menu_task, 50);
            }
            break;
        }

        default:
            break;
    }
}

/* ==================================================================
 *  Debug screen (placeholder — any key returns to menu)
 * ================================================================== */
void ui_debug_task(sc_event_t *e)
{
    switch (e->type)
    {
        case SC_EVENT_TYPE_INIT:
        {
            sc_clear(0, 0, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT, MENU_BG);
            draw_title("Debug");
            sc_draw_str(NULL,  5, 22, &lv_font_12, "Debug interface",   C_WHITE, MENU_BG, NULL, ALIGN_NONE);
            sc_draw_str(NULL,  5, 36, &lv_font_12, "(not implemented)",  C_DIM_GRAY, MENU_BG, NULL, ALIGN_NONE);
            sc_rect_t hint = {0, 116, SC_SCREEN_WIDTH, 12};
            sc_draw_str(NULL, 0, 0, &lv_font_12, "Any key to back",
                        MENU_HINT, MENU_BG, &hint, ALIGN_CENTER);
            break;
        }

        case SC_EVENT_TYPE_CMD:
            sc_create_task(0, ui_menu_task, 50);
            break;

        default:
            break;
    }
}

/* ==================================================================
 *  About screen (static, any key returns to menu)
 * ================================================================== */
void ui_about_task(sc_event_t *e)
{
    switch (e->type)
    {
        case SC_EVENT_TYPE_INIT:
        {
            sc_clear(0, 0, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT, MENU_BG);
            draw_title("About");
            sc_draw_str(NULL,  5, 22, &lv_font_12, "RF Power Meter v1.0", C_WHITE, MENU_BG, NULL, ALIGN_NONE);
            sc_draw_str(NULL,  5, 36, &lv_font_12, "MCU: STM32F103",      C_GRAY,  MENU_BG, NULL, ALIGN_NONE);
            sc_draw_str(NULL,  5, 50, &lv_font_12, "LCD: 161x130 SPI",    C_GRAY,  MENU_BG, NULL, ALIGN_NONE);
            sc_draw_str(NULL,  5, 64, &lv_font_12, "ADC: 12-bit  2.5V",   C_GRAY,  MENU_BG, NULL, ALIGN_NONE);
            sc_draw_str(NULL,  5, 78, &lv_font_12, "Freq: TIM x16 Comp",  C_GRAY,  MENU_BG, NULL, ALIGN_NONE);
            sc_draw_str(NULL,  5, 92, &lv_font_12, "EEPROM: 24C16",       C_GRAY,  MENU_BG, NULL, ALIGN_NONE);
            sc_rect_t hint = {0, 116, SC_SCREEN_WIDTH, 12};
            sc_draw_str(NULL, 0, 0, &lv_font_12, "Any key to back",
                        MENU_HINT, MENU_BG, &hint, ALIGN_CENTER);
            break;
        }

        case SC_EVENT_TYPE_CMD:
            sc_create_task(0, ui_menu_task, 50);
            break;

        default:
            break;
    }
}
