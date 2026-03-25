#include "ui_main.h"
#include "ui_menu.h"
#include "sc_port.h"
#include "sc_common.h"
#include "interface_manager.h"
#include "frequency_counter.h"
#include <stdio.h>

/* ===== 布局常量 =====
 *
 *  y=  0~16 : 标题栏 (深蓝底)
 *  x=  0~72 : 左面板 — "FWD" 标签 + 圆弧仪表盘 + 功率值(弧下)
 *  x= 73    : 垂直分割线
 *  x= 74~160: 右面板 — SWR / R / G / Eff / 频率（5行等距）
 *
 *  圆弧：cx=37, cy=62, r=29, ir=17
 *        0%=210°(左下), 100%=150°(右下), 顺时针300°
 */

/* ── 左面板圆弧 ── */
#define ARC_CX      37
#define ARC_CY      62
#define ARC_R       29
#define ARC_IR      17
#define ARC_START   210
#define ARC_END     150

/* ── 右面板参数列 ── */
#define RPANEL_X    77      /* 右面板文字起始 x */
#define RPANEL_Y0   22      /* 第1行 y */
#define RPANEL_DY   22      /* 行间距（等分显示区域）*/

/* ── 量程 ── */
#define GAUGE_FULL_SCALE_W  1000.0f

/* ── 背景色（深海军蓝，比纯黑更柔和）── */
#define C_BG    ((color_t)0x0000)

/* ---- helpers ---- */

static color_t vswr_to_color(float vswr)
{
    if (vswr < 1.5f) return C_GREEN;
    if (vswr < 2.0f) return C_YELLOW;
    return C_RED;
}

static int power_to_arc_angle(float power_w)
{
    float ratio = power_w / GAUGE_FULL_SCALE_W;
    if (ratio > 1.0f) ratio = 1.0f;
    if (ratio < 0.0f) ratio = 0.0f;
    int end = ARC_START + (int)(ratio * 300.0f);
    if (end >= 360) end -= 360;
    return end;
}

/* ---- 将所有动态内容绘入同一个 pfb（消除闪烁）---- */

static void draw_content(sc_pfb_t *pfb,
                          float fwd_w, float ref_w,
                          float vswr,  float gamma, float eta,
                          uint32_t freq_hz)
{
    char buf[24];
    color_t ac = vswr_to_color(vswr);

    /* ── 垂直分割线 ── */
    sc_draw_Fill(pfb, 73, 17, 1, SC_SCREEN_HEIGHT - 17, C_DARK_GRAY, 255);

    /* ── 左面板顶部：入射功率缩写标签 ── */
    {
        sc_rect_t lbl = {0, 18, 73, 12};
        sc_draw_str(pfb, 0, 0, &lv_font_12, "FWD",
                    C_GRAY, C_BG, &lbl, ALIGN_CENTER);
    }

    /* ─────── 左面板：圆弧仪表盘 ─────── */
    sc_arc_t arc = {ARC_CX, ARC_CY, ARC_R, ARC_IR, 0};
    int ae = power_to_arc_angle(fwd_w);

    if (fwd_w < 0.5f || ae == ARC_START) {
        sc_draw_Arc(pfb, &arc, ARC_START, ARC_END,
                    C_DARK_GRAY, C_DARK_GRAY, C_BG, 255);
    } else if (ae == ARC_END) {
        sc_draw_Arc(pfb, &arc, ARC_START, ARC_END,
                    ac, ac, C_BG, 255);
    } else {
        /* 已激活段（彩色）*/
        sc_draw_Arc(pfb, &arc, ARC_START, ae,
                    ac, ac, C_BG, 255);
        /* 未激活段（灰色）*/
        sc_draw_Arc(pfb, &arc, ae, ARC_END,
                    C_DARK_GRAY, C_DARK_GRAY, C_BG, 255);
    }

    /* ── 圆弧下方功率值（16px，左面板居中）── */
    sc_rect_t pw_box = {0, 94, 73, 21};
    if (fwd_w >= 1000.0f)
        sprintf(buf, "%.2fkW", fwd_w / 1000.0f);
    else
        sprintf(buf, "%.1fW", fwd_w);
    sc_draw_str(pfb, 0, 0, &lv_font_20, buf,
                C_WHITE, C_BG, &pw_box, ALIGN_CENTER);

    /* ── 左面板底部：量程标注 ── */
    {
        sc_rect_t rng = {0, 116, 73, 12};
        sc_draw_str(pfb, 0, 0, &lv_font_12, "(0-2kW)",
                    C_DARK_GRAY, C_BG, &rng, ALIGN_CENTER);
    }

    /* ─────── 右面板：参数列表 ─────── */
    int ry = RPANEL_Y0;

    /* 驻波比 SWR */
    if (vswr > 99.9f)
        sprintf(buf, "SWR:>99");
    else
        sprintf(buf, "SWR:%.2f", vswr);
    sc_draw_str(pfb, RPANEL_X, ry, &lv_font_12, buf,
                ac, C_BG, NULL, ALIGN_NONE);
    ry += RPANEL_DY;

    /* 反射功率 R */
    if (ref_w >= 1000.0f)
        sprintf(buf, "R:%.2fkW", ref_w / 1000.0f);
    else
        sprintf(buf, "R:%.1fW", ref_w);
    sc_draw_str(pfb, RPANEL_X, ry, &lv_font_12, buf,
                ac, C_BG, NULL, ALIGN_NONE);
    ry += RPANEL_DY;

    /* 反射系数 G (Gamma) */
    sprintf(buf, "G:%.3f", gamma);
    sc_draw_str(pfb, RPANEL_X, ry, &lv_font_12, buf,
                C_CYAN, C_BG, NULL, ALIGN_NONE);
    ry += RPANEL_DY;

    /* 传输效率 Eff */
    sprintf(buf, "Eff:%.1f%%", eta);
    sc_draw_str(pfb, RPANEL_X, ry, &lv_font_12, buf,
                C_CYAN, C_BG, NULL, ALIGN_NONE);
    ry += RPANEL_DY;

    /* 频率 */
    if (g_freq_result.is_valid && freq_hz > 0) {
        if (freq_hz >= 1000000UL)
            sprintf(buf, "%.3fMHz", freq_hz / 1000000.0f);
        else if (freq_hz >= 1000UL)
            sprintf(buf, "%.2fkHz", freq_hz / 1000.0f);
        else
            sprintf(buf, "%uHz", (unsigned int)freq_hz);
    } else {
        sprintf(buf, "No Signal");
    }
    sc_draw_str(pfb, RPANEL_X, ry, &lv_font_12, buf,
                C_GRAY, C_BG, NULL, ALIGN_NONE);
}

/* ===== 主界面任务入口 ===== */

void ui_main_task(sc_event_t *e)
{
    switch (e->type)
    {
        case SC_EVENT_TYPE_INIT:
        {
            sc_clear(0, 0, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT, C_BG);
            /* 标题栏（静态，只画一次）*/
            sc_draw_Fill(NULL, 0, 0, SC_SCREEN_WIDTH, 17, C_ROYAL_BLUE, 255);
            sc_rect_t tb = {0, 0, SC_SCREEN_WIDTH, 17};
            sc_draw_str(NULL, 0, 2, &lv_font_12, "RF Power Meter",
                        C_WHITE, C_ROYAL_BLUE, &tb, ALIGN_CENTER);
            break;
        }

        case SC_EVENT_TYPE_TIMER:
        {
            float fwd_w = g_power_result.is_valid ? g_power_result.forward_power   : 0.0f;
            float ref_w = g_power_result.is_valid ? g_power_result.reflected_power : 0.0f;
            float vswr  = g_rf_params.is_valid    ? g_rf_params.vswr               : 1.0f;
            float gamma = g_rf_params.is_valid    ? g_rf_params.reflection_coeff   : 0.0f;
            float eta   = g_rf_params.is_valid    ? g_rf_params.transmission_eff   : 100.0f;
            uint32_t freq = FreqCounter_GetFrequencyHz();

            /* 整个动态区域在同一 PFB 批次渲染，消除闪烁 */
            sc_pfb_t pfb;
            sc_area_t dyn = {0, 17, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT};
            sc_pfb_init_slices(&pfb, &dyn, C_BG);
            do {
                draw_content(&pfb, fwd_w, ref_w, vswr, gamma, eta, freq);
            } while (sc_pfb_next_slice(&pfb));
            break;
        }

        case SC_EVENT_TYPE_CMD:
            if (e->dat.cmd == CMD_ENTER) {
                sc_create_task(0, ui_menu_task, 20);
            }
            break;
    }
}
