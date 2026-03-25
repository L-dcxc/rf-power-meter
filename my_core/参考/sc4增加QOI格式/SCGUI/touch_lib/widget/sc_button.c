

#include "sc_widget.h"

static bool handle_event_button(sc_obj_t *obj, sc_event_t *event)
{
    Button_t *p = (Button_t *)obj;
    if (event->type == SC_EVENT_TYPE_DRAW)
    {
        sc_pfb_t *dest = event->dat.arg;
        dest->parent = &obj->parent->rect;
        sc_draw_button(dest, &obj->rect, &p->btn, p->color, p->fill,p->r,p->ir, obj->alpha);
        dest->parent = NULL;
        return true;
    }
    if (event->type == SC_EVENT_TOUCH_DOWN)
    {
        obj->flag |= SC_OBJ_FLAG_PRESSED;
        obj->flag ^= SC_OBJ_FLAG_BOOL;
        obj->rect.x -= 1;
        obj->rect.y -= 1;
        obj->rect.w += 2;
        obj->rect.h += 2;
        sc_obj_dirty(obj, NULL);
        return true;
    }
    else if (event->type == SC_EVENT_TOUCH_UP)
    {
        if (obj->flag & SC_OBJ_FLAG_PRESSED)
        {
            obj->flag &= ~SC_OBJ_FLAG_PRESSED;
            sc_obj_dirty(obj, NULL);
            obj->rect.x += 1;
            obj->rect.y += 1;
            obj->rect.w -= 2;
            obj->rect.h -= 2;
        }
        return true;
    }
    else if (event->type == SC_EVENT_TYPE_CMD)
    {
  
    }
    return false;
}

sc_obj_t *sc_create_button(sc_obj_t *parent, int x, int y, int w, int h, sc_align_t align)
{
    Button_t *p = (Button_t *)sc_obj_init(parent, malloc(sizeof(Button_t)), SC_OBJ_TYPE_BUTTON); // 初始化基类
    if (p)
    {
        p->base.handle_event = handle_event_button;       // 设置事件处理函数
        p->base.attr |= SC_OBJ_ATTR_FOCUS;                // 设置属性
        sc_obj_set_geometry(&p->base, x, y, w, h, align); // 初始化基类位置大小
        p->color = gui->fc;                               // 设置属性
        p->fill = gui->bc;                                // 设置属性
        p->r =  5;                                     // 设置属性
        p->ir = 4;                                     // 设置属性
        sc_init_button(&p->btn,  gui->font, "but", gui->bkc, ALIGN_CENTER);
        return &p->base;
    }
    return NULL;
}

void sc_set_button_text(sc_obj_t *obj, char *text)
{
    if (obj == NULL)
        return;
    Button_t *p = (Button_t *)obj;
    p->btn.text = text;
    sc_obj_dirty(obj, NULL);
}
