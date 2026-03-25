
#ifndef SC_ARC_H
#define SC_ARC_H

#include "sc_common.h"
//-------圆弧-----
typedef struct sc_arc_t
{
    int16_t  cx;
    int16_t  cy;
    uint8_t  r;
    uint8_t  ir;
    uint8_t  dot;  //圆点
} sc_arc_t;

void sc_draw_Arc(sc_pfb_t *dest, const sc_arc_t *p, int start_deg, int end_deg, color_t start, color_t end, color_t bkc, uint16_t alpha);

int sc_draw_Eye_blink(sc_pfb_t *dest, int x1, int y1, int x2, int y2, int vtx_y, color_t color);

void DrawEye_Blink_test(sc_pfb_t *dest, int x, int y, int w, int h, int up, int dowm, color_t color);

#endif //SC_ARC_H




