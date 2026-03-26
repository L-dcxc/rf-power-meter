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
                        case 2: sc_create_task(0, ui_alarm_task,       50); break;
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
 *  Alarm Setup screen  (no title bar — full height content)
 *
 *  [PFB area y=0..129]
 *  y=5    Row0  "Alarm En:"  ON/OFF  (item=0)
 *  y=22   gray sep
 *  y=28   Row1  "Key Beep:"  ON/OFF  (item=1)
 *  y=45   gray sep
 *  y=51   Row2  "VSWR Limit:"        (item=2..5)
 *  y=68   gray sep
 *  y=74   digit row  H  T  O  .  D
 *  y=88   underline  (item 2-5)
 *  y=93   hint1 / y=108  hint2
 *
 *  Indicator bar (x,y,w,h) animated with easeOutCubic (250ms):
 *    item 0,1  → left vertical bar  (w=3, h=12)
 *    item 2-5  → bottom underline   (w=8, h=3)
 *    1→2 : L-path — slides down through VSWR row, morphs to horizontal,
 *           then slides right to digit H
 *    5→0 : reverse L-path
 *    all others: simple ease interpolation
 *
 *  Keys: DOWN→cycle 0-5; ENTER→toggle/+1; UP→save+return
 * ================================================================== */

#define ALM_ROW0_Y   5
#define ALM_ROW1_Y   28
#define ALM_ROW2_Y   51
#define ALM_ANIM_MS   250
#define ALM_ANIM_L_MS  600   /* L-path (cross-type) animation duration */

/* VSWR digit x-positions */
#define ALM_X_H     46
#define ALM_X_T     59
#define ALM_X_O     72
#define ALM_X_DOT   84
#define ALM_X_D     91
#define ALM_Y_DIG   74
#define ALM_Y_UL    88   /* underline top-y (digit_bottom+3) */

/* Resting indicator geometry per item [x, y, w, h] */
static const int16_t ALM_IX[6] = { 0,  0, 46, 59, 72, 91};
static const int16_t ALM_IY[6] = { 5, 28, 88, 88, 88, 88};
static const int16_t ALM_IW[6] = { 3,  3,  8,  8,  8,  8};
static const int16_t ALM_IH[6] = {12, 12,  3,  3,  3,  3};

/*
 * Compute indicator bar (ox,oy,ow,oh) given transition fi→ti at linear t [0,1].
 * Per-phase easing is applied internally.
 */
static void alarm_get_indicator(uint8_t fi, uint8_t ti, float t,
                                  int16_t *ox, int16_t *oy,
                                  int16_t *ow, int16_t *oh)
{
    float p, cx, cy, cw, ch;

    if (fi == 1 && ti == 2) {
        /* L-path down: left-bar(0,28,3,12) → underline(46,88,8,3)
         *  [0.00-0.35] slide y 28→51 (VSWR row)
         *  [0.35-0.55] slide y 51→74 (digit row)
         *  [0.55-0.75] morph: y 74→88, w 3→8, h 12→3
         *  [0.75-1.00] slide x 0→46  */
        if (t < 0.35f) {
            p  = ease_out_cubic(t / 0.35f);
            cx = 0;  cy = 28 + (51-28)*p;  cw = 3;  ch = 12;
        } else if (t < 0.55f) {
            p  = ease_out_cubic((t - 0.35f) / 0.20f);
            cx = 0;  cy = 51 + (74-51)*p;  cw = 3;  ch = 12;
        } else if (t < 0.75f) {
            p  = ease_out_cubic((t - 0.55f) / 0.20f);
            cx = 0;  cy = 74 + (88-74)*p;  cw = 3 + (8-3)*p;  ch = 12 + (3-12)*p;
        } else {
            p  = ease_out_cubic((t - 0.75f) / 0.25f);
            cx = 46*p;  cy = 88;  cw = 8;  ch = 3;
        }
    } else if (fi == 5 && ti == 0) {
        /* L-path up: underline(91,88,8,3) → left-bar(0,5,3,12)
         *  [0.00-0.25] slide x 91→0
         *  [0.25-0.45] morph: y 88→74, w 8→3, h 3→12
         *  [0.45-0.65] slide y 74→51 (VSWR row)
         *  [0.65-1.00] slide y 51→5  (Alarm En) */
        if (t < 0.25f) {
            p  = ease_out_cubic(t / 0.25f);
            cx = 91*(1-p);  cy = 88;  cw = 8;  ch = 3;
        } else if (t < 0.45f) {
            p  = ease_out_cubic((t - 0.25f) / 0.20f);
            cx = 0;  cy = 88 + (74-88)*p;  cw = 8 + (3-8)*p;  ch = 3 + (12-3)*p;
        } else if (t < 0.65f) {
            p  = ease_out_cubic((t - 0.45f) / 0.20f);
            cx = 0;  cy = 74 + (51-74)*p;  cw = 3;  ch = 12;
        } else {
            p  = ease_out_cubic((t - 0.65f) / 0.35f);
            cx = 0;  cy = 51 + (5-51)*p;  cw = 3;  ch = 12;
        }
    } else {
        /* Simple ease interpolation for all other transitions */
        float et = ease_out_cubic(t);
        cx = ALM_IX[fi] + (ALM_IX[ti] - ALM_IX[fi]) * et;
        cy = ALM_IY[fi] + (ALM_IY[ti] - ALM_IY[fi]) * et;
        cw = ALM_IW[fi] + (ALM_IW[ti] - ALM_IW[fi]) * et;
        ch = ALM_IH[fi] + (ALM_IH[ti] - ALM_IH[fi]) * et;
    }

    *ox = (int16_t)(cx + 0.5f);
    *oy = (int16_t)(cy + 0.5f);
    *ow = (int16_t)(cw + 0.5f);
    *oh = (int16_t)(ch + 0.5f);
}

static void alarm_draw_content(sc_pfb_t *pfb,
                                uint8_t s_item, uint8_t s_en,
                                uint8_t s_buz, float s_vswr,
                                uint8_t from_item, uint8_t to_item, float t)
{
    char    buf[4];
    int     total_tenths, hd, td, od, dd;
    int16_t bx, by, bw, bh;

    alarm_get_indicator(from_item, to_item, t, &bx, &by, &bw, &bh);

    /* Indicator bar (drawn first; separators overdraw where coincident) */
    if (bw > 0 && bh > 0)
        sc_draw_Fill(pfb, bx, by, bw, bh, (uint16_t)0x07E0, 255);

    /* Separators */
    sc_draw_Fill(pfb, 0, 22, SC_SCREEN_WIDTH, 1, C_DIM_GRAY, 255);
    sc_draw_Fill(pfb, 0, 45, SC_SCREEN_WIDTH, 1, C_DIM_GRAY, 255);
    sc_draw_Fill(pfb, 0, 68, SC_SCREEN_WIDTH, 1, C_DIM_GRAY, 255);

    /* Row 0: Alarm En */
    sc_draw_str(pfb, 14, ALM_ROW0_Y, &lv_font_12, "Alarm En:", C_CYAN, MENU_BG, NULL, ALIGN_NONE);
    if (s_en)
        sc_draw_str(pfb, 92, ALM_ROW0_Y, &lv_font_12, "ON ", (uint16_t)0x07E0, MENU_BG, NULL, ALIGN_NONE);
    else
        sc_draw_str(pfb, 92, ALM_ROW0_Y, &lv_font_12, "OFF", (uint16_t)0xF800, MENU_BG, NULL, ALIGN_NONE);

    /* Row 1: Key Beep */
    sc_draw_str(pfb, 14, ALM_ROW1_Y, &lv_font_12, "Key Beep:", C_CYAN, MENU_BG, NULL, ALIGN_NONE);
    if (s_buz)
        sc_draw_str(pfb, 92, ALM_ROW1_Y, &lv_font_12, "ON ", (uint16_t)0x07E0, MENU_BG, NULL, ALIGN_NONE);
    else
        sc_draw_str(pfb, 92, ALM_ROW1_Y, &lv_font_12, "OFF", (uint16_t)0xF800, MENU_BG, NULL, ALIGN_NONE);

    /* Row 2: VSWR Limit */
    sc_draw_str(pfb, 14, ALM_ROW2_Y, &lv_font_12, "VSWR Limit:", C_CYAN, MENU_BG, NULL, ALIGN_NONE);

    /* Digit row (all white — underline is sole cursor) */
    total_tenths = (int)(s_vswr * 10.0f + 0.5f);
    hd = (total_tenths / 1000) % 10;
    td = (total_tenths / 100)  % 10;
    od = (total_tenths / 10)   % 10;
    dd =  total_tenths         % 10;
    sprintf(buf, "%d", hd);
    sc_draw_str(pfb, ALM_X_H,   ALM_Y_DIG, &lv_font_12, buf, C_WHITE, MENU_BG, NULL, ALIGN_NONE);
    sprintf(buf, "%d", td);
    sc_draw_str(pfb, ALM_X_T,   ALM_Y_DIG, &lv_font_12, buf, C_WHITE, MENU_BG, NULL, ALIGN_NONE);
    sprintf(buf, "%d", od);
    sc_draw_str(pfb, ALM_X_O,   ALM_Y_DIG, &lv_font_12, buf, C_WHITE, MENU_BG, NULL, ALIGN_NONE);
    sc_draw_str(pfb, ALM_X_DOT, ALM_Y_DIG, &lv_font_12, ".",  C_WHITE, MENU_BG, NULL, ALIGN_NONE);
    sprintf(buf, "%d", dd);
    sc_draw_str(pfb, ALM_X_D,   ALM_Y_DIG, &lv_font_12, buf, C_WHITE, MENU_BG, NULL, ALIGN_NONE);

    /* Hints */
    sc_draw_str(pfb, 4, 93,  &lv_font_12, "DOWN:Sel   UP:Back", C_DIM_GRAY, MENU_BG, NULL, ALIGN_NONE);
    if (s_item <= 1)
        sc_draw_str(pfb, 4, 108, &lv_font_12, "OK:Toggle ON/OFF", C_DIM_GRAY, MENU_BG, NULL, ALIGN_NONE);
    else
        sc_draw_str(pfb, 4, 108, &lv_font_12, "OK:+1 each digit", C_DIM_GRAY, MENU_BG, NULL, ALIGN_NONE);
}

void ui_alarm_task(sc_event_t *e)
{
    static uint8_t  s_item;
    static uint8_t  s_en;
    static uint8_t  s_buz;
    static float    s_vswr;
    static uint8_t   s_from_item;
    static uint8_t   s_to_item;
    static uint16_t  s_anim_ms;
    static uint32_t  s_anim_start;

    switch (e->type)
    {
        case SC_EVENT_TYPE_INIT:
        {
            s_item       = 0;
            s_en         = g_interface_manager.alarm_enabled;
            s_buz        = g_interface_manager.buzzer_enabled;
            s_vswr       = g_interface_manager.vswr_alarm_threshold;
            s_from_item  = 0;
            s_to_item    = 0;
            s_anim_ms    = ALM_ANIM_MS;
            s_anim_start = HAL_GetTick();
            sc_clear(0, 0, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT, MENU_BG);
            break;
        }

        case SC_EVENT_TYPE_TIMER:
        {
            uint32_t elapsed = HAL_GetTick() - s_anim_start;
            float t = (float)elapsed / (float)s_anim_ms;
            if (t > 1.0f) t = 1.0f;

            sc_pfb_t pfb;
            sc_area_t dyn = {0, 0, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT};
            sc_pfb_init_slices(&pfb, &dyn, MENU_BG);
            do {
                alarm_draw_content(&pfb, s_item, s_en, s_buz, s_vswr,
                                   s_from_item, s_to_item, t);
            } while (sc_pfb_next_slice(&pfb));
            break;
        }

        case SC_EVENT_TYPE_CMD:
        {
            int cmd = e->dat.cmd;
            int total_tenths, hd, td, od, dd, new_tt;

            if (cmd == CMD_UP || cmd == CMD_BACK) {
                g_interface_manager.alarm_enabled        = s_en;
                g_interface_manager.buzzer_enabled       = s_buz;
                g_interface_manager.vswr_alarm_threshold = s_vswr;
                BL24C16_Write(0x0001, &s_en,             1);
                BL24C16_Write(0x0002, (uint8_t*)&s_vswr, sizeof(float));
                BL24C16_Write(0x0006, &s_buz,            1);
                sc_create_task(0, ui_menu_task, 50);
            } else if (cmd == CMD_DOWN) {
                s_from_item  = s_item;
                s_item       = (s_item + 1) % 6;
                s_to_item    = s_item;
                s_anim_ms    = ((s_from_item == 1 && s_to_item == 2) ||
                                (s_from_item == 5 && s_to_item == 0))
                                ? ALM_ANIM_L_MS : ALM_ANIM_MS;
                s_anim_start = HAL_GetTick();
            } else if (cmd == CMD_ENTER) {
                if (s_item == 0) {
                    s_en = !s_en;
                } else if (s_item == 1) {
                    s_buz = !s_buz;
                } else {
                    total_tenths = (int)(s_vswr * 10.0f + 0.5f);
                    hd = (total_tenths / 1000) % 10;
                    td = (total_tenths / 100)  % 10;
                    od = (total_tenths / 10)   % 10;
                    dd =  total_tenths         % 10;
                    switch (s_item) {
                        case 2: hd = (hd + 1) % 10; break;
                        case 3: td = (td + 1) % 10; break;
                        case 4: od = (od + 1) % 10; break;
                        case 5: dd = (dd + 1) % 10; break;
                        default: break;
                    }
                    new_tt = hd * 1000 + td * 100 + od * 10 + dd;
                    s_vswr = (float)new_tt / 10.0f;
                    if (s_vswr < 1.0f)   s_vswr = 1.0f;
                    if (s_vswr > 999.9f) s_vswr = 999.9f;
                }
            }
            break;
        }

        default:
            break;
    }
}

/* ==================================================================
 *  Diagnostics screen — live ADC + calibration readout
 *
 *  y=2~17   title "Diagnostics" (cyan, lv_font_16)
 *  y=19     1px white separator
 *  y=22     Row1  ADC raw F+R         (yellow 0xFFE0)   sep y=37
 *  y=43     Row2  Voltage F+R         (cyan)            sep y=58
 *  y=64     Row3  Offset + Corrected  (white)           sep y=79
 *  y=85     Row4  Cal Y/N  Pts xx/20  (cyan+green/red)  sep y=100
 *  y=106    Row5  Result power        (yellow+white/gray)
 * ================================================================== */

static void dbg_read_adc(uint32_t *fwd_raw, uint32_t *ref_raw,
                          float *fwd_v,  float *ref_v)
{
    extern ADC_HandleTypeDef hadc1;
    ADC_ChannelConfTypeDef sc = {0};
    sc.Rank           = ADC_REGULAR_RANK_1;
    sc.SamplingTime   = ADC_SAMPLETIME_1CYCLE_5;

    sc.Channel = ADC_CHANNEL_2;
    HAL_ADC_ConfigChannel(&hadc1, &sc);
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    *fwd_raw = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    sc.Channel = ADC_CHANNEL_3;
    HAL_ADC_ConfigChannel(&hadc1, &sc);
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    *ref_raw = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    *fwd_v = (float)(*fwd_raw) * 2.5f / 4095.0f;
    *ref_v = (float)(*ref_raw) * 2.5f / 4095.0f;
}

/* Draw static parts: labels + separator lines — called ONCE in INIT */
static void dbg_draw_static(void)
{
    /* Separator lines — 5px below text above, 6px above text below (DY=23) */
    sc_draw_Fill(NULL, 0, 38,  SC_SCREEN_WIDTH, 1, C_DIM_GRAY, 255);
    sc_draw_Fill(NULL, 0, 61,  SC_SCREEN_WIDTH, 1, C_DIM_GRAY, 255);
    sc_draw_Fill(NULL, 0, 84,  SC_SCREEN_WIDTH, 1, C_DIM_GRAY, 255);
    sc_draw_Fill(NULL, 0, 107, SC_SCREEN_WIDTH, 1, C_DIM_GRAY, 255);

    /* Row 1 labels — yellow (y=22) */
    sc_draw_str(NULL,  4, 22, &lv_font_12, "F:",     (uint16_t)0xFFE0, MENU_BG, NULL, ALIGN_NONE);
    sc_draw_str(NULL, 85, 22, &lv_font_12, "R:",     (uint16_t)0xFFE0, MENU_BG, NULL, ALIGN_NONE);
    /* Row 2 labels — cyan (y=45) */
    sc_draw_str(NULL,  4, 45, &lv_font_12, "F:",     C_CYAN,           MENU_BG, NULL, ALIGN_NONE);
    sc_draw_str(NULL, 85, 45, &lv_font_12, "R:",     C_CYAN,           MENU_BG, NULL, ALIGN_NONE);
    /* Row 3 labels — white (y=68) */
    sc_draw_str(NULL,  4, 68, &lv_font_12, "Off:",   C_WHITE,          MENU_BG, NULL, ALIGN_NONE);
    sc_draw_str(NULL, 85, 68, &lv_font_12, "Cor:",   C_WHITE,          MENU_BG, NULL, ALIGN_NONE);
    /* Row 4 labels — cyan (y=91) */
    sc_draw_str(NULL,  4, 91, &lv_font_12, "Cal:",   C_CYAN,           MENU_BG, NULL, ALIGN_NONE);
    sc_draw_str(NULL, 60, 91, &lv_font_12, "Pts:",   C_CYAN,           MENU_BG, NULL, ALIGN_NONE);
    /* Row 5 label — yellow (y=114) */
    sc_draw_str(NULL,  4, 114, &lv_font_12, "Result:", (uint16_t)0xFFE0, MENU_BG, NULL, ALIGN_NONE);
}

/* Redraw only value fields — no full clear, uses fixed-width formats to overwrite old digits */
static void dbg_draw_values(void)
{
    char buf[16];
    uint32_t fwd_raw, ref_raw;
    float fwd_v, ref_v, fwd_cor;
    uint8_t is_cal, fwd_pts;

    dbg_read_adc(&fwd_raw, &ref_raw, &fwd_v, &ref_v);
    fwd_cor = fwd_v - g_calibration_data.forward_offset;
    is_cal  = g_calibration_data.is_calibrated;
    fwd_pts = g_calibration_data.fwd_points;

    /* Row 1 — ADC raw (y=22), value x+4 for breathing room after ':' */
    sprintf(buf, "%4u", (unsigned int)fwd_raw);
    sc_draw_str(NULL, 22, 22, &lv_font_12, buf, (uint16_t)0xFFE0, MENU_BG, NULL, ALIGN_NONE);
    sprintf(buf, "%4u", (unsigned int)ref_raw);
    sc_draw_str(NULL, 103, 22, &lv_font_12, buf, (uint16_t)0xFFE0, MENU_BG, NULL, ALIGN_NONE);

    /* Row 2 — Voltages (y=45, always 6 chars: x.xxxV) */
    sprintf(buf, "%.3fV", fwd_v);
    sc_draw_str(NULL, 22, 45, &lv_font_12, buf, C_CYAN, MENU_BG, NULL, ALIGN_NONE);
    sprintf(buf, "%.3fV", ref_v);
    sc_draw_str(NULL, 103, 45, &lv_font_12, buf, C_CYAN, MENU_BG, NULL, ALIGN_NONE);

    /* Row 3 — Offset + Corrected (y=68, always 5 chars: x.xxx) */
    sprintf(buf, "%.3f", g_calibration_data.forward_offset);
    sc_draw_str(NULL, 36, 68, &lv_font_12, buf, C_WHITE, MENU_BG, NULL, ALIGN_NONE);
    sprintf(buf, "%.3f", fwd_cor);
    sc_draw_str(NULL, 117, 68, &lv_font_12, buf, C_WHITE, MENU_BG, NULL, ALIGN_NONE);

    /* Row 4 — Cal Y/N (y=91); Pts fixed 5 chars */
    if (is_cal)
        sc_draw_str(NULL, 36, 91, &lv_font_12, "Y", (uint16_t)0x07E0, MENU_BG, NULL, ALIGN_NONE);
    else
        sc_draw_str(NULL, 36, 91, &lv_font_12, "N", (uint16_t)0xF800, MENU_BG, NULL, ALIGN_NONE);
    sprintf(buf, "%2d/20", fwd_pts);
    sc_draw_str(NULL, 92, 91, &lv_font_12, buf, C_CYAN, MENU_BG, NULL, ALIGN_NONE);

    /* Row 5 — Result (y=114): clear value area then draw */
    sc_draw_Fill(NULL, 61, 114, 70, 12, MENU_BG, 255);
    if (is_cal && g_power_result.is_valid) {
        sprintf(buf, "%.1fW", g_power_result.forward_power);
        sc_draw_str(NULL, 61, 114, &lv_font_12, buf, C_WHITE, MENU_BG, NULL, ALIGN_NONE);
    } else {
        sc_draw_str(NULL, 61, 114, &lv_font_12, "No Cal", C_DIM_GRAY, MENU_BG, NULL, ALIGN_NONE);
    }
}

void ui_debug_task(sc_event_t *e)
{
    switch (e->type)
    {
        case SC_EVENT_TYPE_INIT:
        {
            sc_clear(0, 0, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT, MENU_BG);
            sc_draw_str(NULL, 4, 2, &lv_font_16, "Diagnostics", C_CYAN, MENU_BG, NULL, ALIGN_NONE);
            sc_draw_Fill(NULL, 0, 19, SC_SCREEN_WIDTH, 1, C_WHITE, 255);
            dbg_draw_static();
            dbg_draw_values();
            break;
        }

        case SC_EVENT_TYPE_TIMER:
        {
            static uint32_t s_dbg_last = 0;
            uint32_t now = HAL_GetTick();
            if (now - s_dbg_last >= 500) {
                s_dbg_last = now;
                dbg_draw_values();
            }
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
            /* 0x07E0 = RGB565 pure green */
            sc_clear(0, 0, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT, MENU_BG);

            /* ── Title (y=3, lv_font_16) + version (y=7, lv_font_12) ── */
            sc_draw_str(NULL,   4,  3, &lv_font_16, "RF Power Meter", (uint16_t)0x07E0, MENU_BG, NULL, ALIGN_NONE);
            sc_draw_str(NULL, 128,  7, &lv_font_12, "V1.0",           (uint16_t)0x07E0, MENU_BG, NULL, ALIGN_NONE);

            /* ── Title separator (1px green, y=22) ── */
            sc_draw_Fill(NULL, 0, 22, SC_SCREEN_WIDTH, 1, (uint16_t)0x07E0, 255);

            /* ── Spec rows: all green, DY=25, text h=12, seps 7px below text ── */
            /* Row 1 Freq   text y=29~40, sep y=47 */
            sc_draw_str(NULL, 4, 29, &lv_font_12, "Freq:  1Hz - 100MHz", (uint16_t)0x07E0, MENU_BG, NULL, ALIGN_NONE);
            sc_draw_Fill(NULL, 0, 47, SC_SCREEN_WIDTH, 1, (uint16_t)0x07E0, 255);

            /* Row 2 Power  text y=54~65, sep y=72 */
            sc_draw_str(NULL, 4, 54, &lv_font_12, "Power: 0W - 2kW",    (uint16_t)0x07E0, MENU_BG, NULL, ALIGN_NONE);
            sc_draw_Fill(NULL, 0, 72, SC_SCREEN_WIDTH, 1, (uint16_t)0x07E0, 255);

            /* Row 3 VSWR   text y=79~90, sep y=97 */
            sc_draw_str(NULL, 4, 79, &lv_font_12, "VSWR:  1.0 - 999.0", (uint16_t)0x07E0, MENU_BG, NULL, ALIGN_NONE);
            sc_draw_Fill(NULL, 0, 97, SC_SCREEN_WIDTH, 1, (uint16_t)0x07E0, 255);

            /* Row 4 Author text y=104~115 (bottom margin 14px to y=129) */
            sc_draw_str(NULL, 4, 104, &lv_font_12, "Author: XUN YU TEK", (uint16_t)0x07E0, MENU_BG, NULL, ALIGN_NONE);
            break;
        }

        case SC_EVENT_TYPE_CMD:
            sc_create_task(0, ui_menu_task, 50);
            break;

        default:
            break;
    }
}
