
#include "sc_widget.h"

static bool handle_event_switch(sc_obj_t *obj, sc_event_t *event)
{
    if (event->type == SC_EVENT_TYPE_DRAW)
    {
        sc_pfb_t *dest = event->dat.arg;
        Switch_t *p = (Switch_t *)obj;
        dest->parent = &obj->parent->rect;
        uint8_t state = obj->flag & SC_OBJ_FLAG_PRESSED ? 1 : 0;
        sc_draw_Switch(dest, &obj->rect, p->r, p->ir, p->color, p->fill, obj->alpha, state);
        dest->parent = NULL;
        return true;
    }
    if (event->type == SC_EVENT_TOUCH_DOWN)
    {
        obj->flag ^= SC_OBJ_FLAG_PRESSED;
        sc_obj_dirty(obj, NULL);
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
/* 创建开关 */
sc_obj_t *sc_create_switch(sc_obj_t *parent, int x, int y, int w, int h, sc_align_t align)
{
    sc_obj_t *obj = sc_obj_init(parent, malloc(sizeof(Switch_t)), SC_OBJ_TYPE_SWITCH); // 初始化基类
    if (obj)
    {
        Switch_t *p = (Switch_t *)obj;
        p->base.handle_event = handle_event_switch; // 设置事件处理函数
        p->base.attr |= SC_OBJ_ATTR_FOCUS;                // 设置属性
        p->color = gui->fc;                         // 设置属性
        p->fill = gui->bc;                          // 设置属性
        p->r = SC_MIN(w, h) / 2;                     // 设置属性
        p->ir = p->r-1;                                // 设置属性
        sc_obj_set_geometry(obj, x, y, w, h, align); // 初始化基类位置大小
        sc_obj_dirty(obj, NULL);
    }
    return obj;
}
