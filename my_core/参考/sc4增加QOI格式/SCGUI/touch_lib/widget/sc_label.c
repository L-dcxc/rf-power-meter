
#include "sc_widget.h"
static bool handle_event_label(sc_obj_t *obj, sc_event_t *event)
{
    Label_t *p = (Label_t *)obj;
    if (event->type == SC_EVENT_TYPE_DRAW)
    {
        sc_pfb_t *dest=event->dat.arg;
        dest->parent = &obj->parent->rect; 
        sc_draw_Label(dest, &obj->rect, &p->label,p->color,p->fill);
        dest->parent = NULL;
        return true;
    }
    return false;
}
sc_obj_t *sc_create_label(sc_obj_t *parent, int x, int y, int w, int h, sc_align_t align)
{
    Label_t *p = (Label_t *)sc_obj_init(parent, malloc(sizeof(Label_t)), SC_OBJ_TYPE_LABEL); // 初始化基类
    if (p)
    {
        p->base.handle_event = handle_event_label;                             // 设置事件处理函数
        sc_obj_set_geometry(&p->base, x, y, w, h, align);                      // 初始化基类位置大小
        sc_init_Label(&p->label, 0, 0, gui->font, "label", align); // 初始化label
        p->color= gui->fc;
        p->fill = gui->bkc;
        return &p->base;
    }
    return NULL;
}

void sc_set_label_text(sc_obj_t *obj,  char *text)
{
    if(obj==NULL) return;
    Label_t *p = (Label_t *)obj;
    p->label.text = text;
    sc_obj_dirty(obj, NULL);
}
