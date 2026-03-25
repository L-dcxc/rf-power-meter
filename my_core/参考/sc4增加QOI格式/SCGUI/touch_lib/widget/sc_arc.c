
#include "sc_widget.h"

static bool handle_event_arc(sc_obj_t *obj, sc_event_t *event)
{
    if (event->type == SC_EVENT_TYPE_DRAW)
    {
        sc_pfb_t *dest = event->dat.arg;
        Arc_t *arc = (Arc_t *)obj;
        sc_arc_t temp; // 临时变量
        temp.cx = obj->rect.x + arc->r;
        temp.cy = obj->rect.y + arc->r;
        temp.r = arc->r;
        temp.ir = arc->ir;
        temp.dot = arc->dot;
        dest->parent = &obj->parent->rect;
        sc_draw_Arc(dest, &temp, arc->start_deg, arc->end_deg, arc->color, arc->fill, arc->bkc, obj->alpha);
        dest->parent = NULL;
        return true;
    }
    return false;
}
/* 创建arc */
sc_obj_t *sc_create_arc(sc_obj_t *parent, int cx, int cy, int r, int ir, sc_align_t align)
{
    sc_obj_t *obj = sc_obj_init(parent, malloc(sizeof(Arc_t)), SC_OBJ_TYPE_ARC); // 初始化基类
    if (obj)
    {
        Arc_t *arc = (Arc_t *)obj;
        arc->base.handle_event = handle_event_arc; // 设置事件处理函数
        arc->color = gui->fc;                      // 设置属性
        arc->fill =  gui->fc;                     // 设置属性
        arc->bkc =   gui->bkc;                      // 设置属性
        arc->r = r;
        arc->ir = ir;
        arc->start_deg = 0;
        arc->end_deg = 360;
        arc->dot = 0; // 默认实心
        sc_obj_set_geometry(obj, cx, cy, 2 * r + 1, 2 * r + 1, align); // 初始化基类位置大小
        sc_obj_dirty(obj, NULL);
    }
    return obj;
}

void sc_set_arc_deg(sc_obj_t *obj, int start_deg, int end_deg)
{
    if (obj == NULL)
        return;
    Arc_t *p = (Arc_t *)obj;
    p->start_deg = start_deg;
    p->end_deg = end_deg;
    sc_obj_dirty(obj, NULL);
}