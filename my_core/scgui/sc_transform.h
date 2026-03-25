#ifndef SC_TRANSFORM_H
#define SC_TRANSFORM_H

#include "sc_common.h"
#include "sc_lvgl_font.h"

typedef struct Transform_t
{
    void *src;         // 源图
    sc_rect_t *box;    // 包围盒
    sc_rect_t drity;   // 需要刷新的区域
    int16_t center_x;  // 源图中心点
    int16_t center_y;  // 源图中心点
    int16_t scaleX;    // X缩放
    int16_t scaleY;    // Y缩放
    int16_t sinA;      // 角度sin
    int16_t cosA;      // 角度cos
} Transform_t;


//// 设置缩放
 void sc_set_transform_scale( Transform_t *p,float scaleX, float scaleY);

/// 设置旋转中心
 void sc_set_transform_center( Transform_t *p,float centerX, float centerY);
/// 初始化旋转参数
void sc_init_transform(sc_rect_t *box, void *src, Transform_t *p);

void sc_set_transform_angle(int move_x, int move_y, int Angle, Transform_t *p);

void sc_init_transform_text(sc_rect_t *rect,lv_font_t *font, const char *text,  void *imge, Transform_t *p);

void sc_draw_transform(sc_pfb_t *dest,int move_x, int move_y, Transform_t *p);

#endif
