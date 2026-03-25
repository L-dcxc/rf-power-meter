
#include "sc_widget.h"

static bool handle_event_rect(sc_obj_t *obj, sc_event_t *event)
{
    if (event->type == SC_EVENT_TYPE_DRAW)
    {
        if (obj->type == SC_OBJ_TYPE_RECT)
        {
            sc_pfb_t *dest = event->dat.arg;
            Rect_t *p = (Rect_t *)obj;
            dest->parent = &obj->parent->rect;
            sc_draw_Fill(dest, obj->rect.x, obj->rect.y, obj->rect.w, obj->rect.h, p->color, obj->alpha); // 绘制矩形
            dest->parent = NULL;
            return true;
        }
    }
    return false;
}

/* 创建矩形：从静态池分配，初始化默认样式 */
sc_obj_t *sc_create_rect(sc_obj_t *parent, int x, int y, int w, int h, sc_align_t align)
{
    sc_obj_t *obj = sc_obj_init(parent, malloc(sizeof(Rect_t)), SC_OBJ_TYPE_RECT); // 初始化基类
    if (obj)
    {
        Rect_t *p = (Rect_t *)obj;
        p->base.handle_event = handle_event_rect;    // 设置事件处理函数
        p->color = gui->fc;                          // 设置属性
        p->fill = gui->bc;                           // 设置属性
        sc_obj_set_geometry(obj, x, y, w, h, align); // 初始化基类位置大小
    }
    return obj;
}

sc_obj_t *sc_create_srceen(sc_obj_t *parent, int x, int y, int w, int h, sc_align_t align)
{
    sc_obj_t *obj = sc_obj_init(parent, malloc(sizeof(sc_obj_t)), SC_OBJ_TYPE_SCREEN); // 初始化基类
    if (obj)
    {
        obj->handle_event = handle_event_rect;       // 设置事件处理函数
        obj->attr |= SC_OBJ_ATTR_VIRTUAL;            // 设置属性
        sc_obj_set_geometry(obj, x, y, w, h, align); // 初始化基类位置大小
    }
    return obj;
}

