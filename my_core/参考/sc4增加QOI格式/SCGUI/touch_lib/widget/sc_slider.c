
#include "sc_widget.h"

static bool handle_event_slider(sc_obj_t *obj, sc_event_t *event)
{
    if (event->type == SC_EVENT_TYPE_DRAW)
    {
        sc_pfb_t *dest = event->dat.arg;
        Slider_t *p = (Slider_t *)obj;
        dest->parent = &obj->parent->rect;
        sc_draw_Bar(dest, &obj->rect, p->r, p->ir, p->color, p->fill, obj->alpha,50);
        dest->parent = NULL;
        return true;
    }
    if (event->type == SC_EVENT_TOUCH_DOWN)
    {
        return true;
    }
    else if (event->type == SC_EVENT_TOUCH_UP)
    {
        return true;
    }
    else if (event->type == SC_EVENT_TYPE_CMD)
    {
    }
    return false;
}
/* 创建slider */
sc_obj_t *sc_create_slider(sc_obj_t *parent, int x, int y, int w, int h, sc_align_t align)
{
    sc_obj_t *obj = sc_obj_init(parent, malloc(sizeof(Slider_t)), SC_OBJ_TYPE_SLIDER); // 初始化基类
    if (obj)
    {
        Slider_t *p = (Slider_t *)obj;
        p->base.handle_event = handle_event_slider;  // 设置事件处理函数
        p->base.attr |= SC_OBJ_ATTR_FOCUS;           // 设置属性
        p->color = gui->fc;                          // 设置属性
        p->fill = gui->bc;                           // 设置属性
        p->r = SC_MIN(w, h) / 2;                     // 设置属性
        p->ir = p->r - 1;                            // 设置属性
        p->volume = 50;                               // 设置属性
        sc_obj_set_geometry(obj, x, y, w, h, align); // 初始化基类位置大小
        sc_obj_dirty(obj, NULL);
    }
    return obj;
}
