
#include "sc_widget.h"

static bool handle_event_led(sc_obj_t *obj, sc_event_t *event)
{
    if (event->type == SC_EVENT_TYPE_DRAW)
    {
        sc_pfb_t *dest = event->dat.arg;
        Led_t *p = (Led_t *)obj;
        dest->parent = &obj->parent->rect;
        sc_draw_Rounded_rect(dest, &obj->rect, p->r, p->ir, p->color, p->fill, obj->alpha);
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
/* 创建led */
sc_obj_t *sc_create_led(sc_obj_t *parent, int cx, int cy, int r, int ir, sc_align_t align)
{
    sc_obj_t *obj = sc_obj_init(parent, malloc(sizeof(Led_t)), SC_OBJ_TYPE_SC_LED); // 初始化基类
    if (obj)
    {
        Led_t *p = (Led_t *)obj;
        p->base.handle_event = handle_event_led; // 设置事件处理函数
        p->base.attr |= SC_OBJ_ATTR_FOCUS;       // 设置属性
        p->color = gui->fc;                      // 设置属性
        p->fill =  gui->bc;                     // 设置属性
        p->r = r;
        p->ir = ir;
        sc_obj_set_geometry(obj, cx, cy, 2 * r + 1, 2 * r + 1, align); // 初始化基类位置大小
        sc_obj_dirty(obj, NULL);
    }
    return obj;
}
