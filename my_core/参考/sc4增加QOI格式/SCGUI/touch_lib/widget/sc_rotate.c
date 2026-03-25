

#include "sc_widget.h"

// 旋转控件事件处理函数
static bool handle_event_rotate(sc_obj_t *obj, sc_event_t *event)
{
    if (event->type == SC_EVENT_TYPE_DRAW)
    {
        sc_pfb_t *dest = event->dat.arg;
        sc_rect_t *prect = &obj->parent->rect;
        Rotate_t *p = (Rotate_t *)obj;
        dest->parent = prect;
        sc_draw_transform(dest, prect->x + p->cx, prect->y + p->cy, &p->params);
        dest->parent = NULL;
        return true;
    }
    return false;
}
// 设置旋转角度
void sc_set_Rotate_angle(sc_obj_t *obj, int angle)
{
    Rotate_t *p = (Rotate_t *)obj;
    sc_rect_t *prect = &obj->parent->rect;
    if (sc_rect_nor_intersect(prect, &gui->lcd_rect)) // 判断是否在屏幕内
        return;
    sc_set_transform_angle(prect->x + p->cx, prect->y + p->cy, angle, &p->params);
    sc_dirty_mark(NULL, &p->params.drity);
}
// 设置旋转缩放
void sc_set_Rotate_scale(sc_obj_t *obj, float scalex, float scaley)
{
    Rotate_t *p = (Rotate_t *)obj;
    sc_set_transform_scale(&p->params, scalex, scaley);
}
// 设置旋转中心
void sc_set_Rotate_center(sc_obj_t *obj, float scalex, float scaley)
{
    Rotate_t *p = (Rotate_t *)obj;
    sc_set_transform_center(&p->params, scalex, scaley);
}
/// 创建Rotate控件
sc_obj_t *sc_create_rotate(sc_obj_t *parent, const sc_image_t *src, int cx, int cy, float src_x, float src_y, sc_align_t align)
{
    sc_obj_t *obj = sc_obj_init(parent, malloc(sizeof(Rotate_t)), SC_OBJ_TYPE_ROTATE); // 初始化基类
    if (obj)
    {
        sc_rect_t *prect = &obj->parent->rect;
        Rotate_t *Rotate = (Rotate_t *)obj;
        Rotate->base.handle_event = handle_event_rotate; // 设置事件处理函数
        Rotate->base.attr = 0;                           // 设置属性
        sc_init_transform(&obj->rect, (void *)src, &Rotate->params);
        sc_set_Rotate_center(obj, src_x, src_y);
        Rotate->cx = cx;
        Rotate->cy = cy;
        Rotate->params.box = &obj->rect;
    }
    return obj;
}
