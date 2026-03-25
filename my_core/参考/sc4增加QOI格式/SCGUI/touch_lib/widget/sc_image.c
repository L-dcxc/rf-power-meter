
#include "sc_widget.h"
/// 图片控件事件处理函数
static bool handle_event_image(sc_obj_t *obj, sc_event_t *event)
{
    if (event->type == SC_EVENT_TYPE_DRAW)
    {
        sc_pfb_t *dest = event->dat.arg;
        dest->parent = &obj->parent->rect;
        if (obj->type == SC_OBJ_TYPE_IMAGEZIP)
        {
            Imagezip_t *p = (Imagezip_t *)obj;
            sc_draw_Image_zip(dest, obj->rect.x, obj->rect.y, p->src, &p->dec);
        }
        else if (obj->type == SC_OBJ_TYPE_IMAGE_INDEXQOI)
        {
            Image_indexQOI_t *p = (Image_indexQOI_t *)obj;
            sc_draw_Image_indexQOI(dest, obj->rect.x, obj->rect.y, p->src, obj->alpha);
        }
        else
        {
            Image_t *p = (Image_t *)obj;
            sc_draw_Image(dest, obj->rect.x, obj->rect.y, p->src);
        }
        dest->parent = NULL;
        return true;
    }
    return false;
}

/// 创建普通图片控件
sc_obj_t *sc_create_image(sc_obj_t *parent, int x, int y, const sc_image_t *src, sc_align_t align)
{
    sc_obj_t *obj = sc_obj_init(parent, malloc(sizeof(Image_t)), SC_OBJ_TYPE_IMAGE); // 初始化基类
    if (obj)
    {
        Image_t *p = (Image_t *)obj;
        p->base.handle_event = handle_event_image;                  // 设置事件处理函数
        p->base.attr = 0;                                           // 设置属性
        sc_obj_set_geometry(&p->base, x, y, src->w, src->h, align); // 初始化基类位置大小
        p->src = src;                                               // 设置图片数据
        return &p->base;
    }
    return obj;
}
// 创建 indexQOI 图片控件
sc_obj_t *sc_create_Image_indexQOI(sc_obj_t *parent, int x, int y, const unsigned char *src, sc_align_t align)
{
    sc_obj_t *obj = sc_obj_init(parent, malloc(sizeof(Image_indexQOI_t)), SC_OBJ_TYPE_IMAGE_INDEXQOI);
    if (obj)
    {
        Image_indexQOI_t *p = (Image_indexQOI_t *)obj;
        p->base.handle_event = handle_event_image;
        p->base.attr = 0;
        // 从 indexQOI 头部剥离宽高
        uint16_t w = (src[1] << 8) | src[2];
        uint16_t h = (src[3] << 8) | src[4];
        sc_obj_set_geometry(&p->base, x, y, w, h, align);
        p->src = src;
        return &p->base;
    }
    return obj;
}
/// 创建压缩图片控件
sc_obj_t *sc_create_image_zip(sc_obj_t *parent, int x, int y, const sc_image_zip *src, sc_align_t align)
{
    sc_obj_t *obj = sc_obj_init(parent, malloc(sizeof(Imagezip_t)), SC_OBJ_TYPE_IMAGEZIP); // 初始化基类
    if (obj)
    {
        Imagezip_t *p = (Imagezip_t *)obj;
        p->base.handle_event = handle_event_image;                  // 设置事件处理函数
        p->base.attr = 0;                                           // 设置属性
        sc_obj_set_geometry(&p->base, x, y, src->w, src->h, align); // 初始化基类位置大小
        p->src = src;                                               // 设置图片数据
        return &p->base;
    }
    return obj;
}
