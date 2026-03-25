#ifndef SC_WIDGET_H
#define SC_WIDGET_H

#include "sc_obj.h"
#include "sc_keyboard.h"

// 循环模式（X/Y轴独立控制）
typedef enum
{
    CANVAS_MOVE_DISABLE = 0, // 禁用移动
    CANVAS_MOVE_X = 0x01,
    CANVAS_MOVE_Y = 0x02,
    CANVAS_MOVE_XY = 0x03,
    CANVAS_MOVE_CYCLE = 0x08, // 启用循环
} Canvas_move_t;
/// Canvas控件（虚拟画布）
typedef struct
{
    sc_obj_t base;            // 基类控件
    int16_t virtual_w;        // 虚拟画布宽度
    int16_t virtual_h;        // 虚拟画布高度
    int16_t view_x;           // 虚拟窗口X
    int16_t view_y;           // 虚拟窗口Y
    int16_t mov_x;            // 虚拟窗口X偏移
    int16_t mov_y;            // 虚拟窗口Y偏移
    Canvas_move_t cycle_mode; // 模式共用
} Canvas_t;

/// 矩形控件
typedef struct
{
    sc_obj_t base; // 基类控件（绝对坐标存储）
    color_t color; // 填充色
    color_t fill;  // 边界色
    uint8_t r;
    uint8_t ir;
} Rect_t;
typedef Rect_t Led_t;    /// LED控件
typedef Rect_t Switch_t; /// 开关控件

typedef struct
{
    sc_obj_t base; // 基类控件（绝对坐标存储）
    color_t color; // 填充色
    color_t fill;  // 边界色
    uint8_t r;
    uint8_t ir;
    uint16_t volume;
} Slider_t;
/// 圆弧
typedef struct Arc_t
{
    sc_obj_t base;
    color_t color;
    color_t fill;
    uint8_t r;
    uint8_t ir;
    int16_t start_deg; // 角度
    int16_t end_deg;   // 角度
    uint8_t dot;       // 端点
    color_t bkc;       // 背景色
} Arc_t;

/// 按钮控件
typedef struct Button_t
{
    sc_obj_t base;
    color_t color;
    color_t fill;
    uint8_t r;
    uint8_t ir;
    sc_button_t btn;
} Button_t;

/// 标签控件
typedef struct Label_t
{
    sc_obj_t base;    // 基类控件
    color_t color;
    color_t fill;
    sc_label_t label; // 标签控件
} Label_t;
/// 输入框控件
typedef struct Line_edit_t
{
    sc_obj_t base;
    color_t color;
    color_t fill;
    sc_label_t label;      // 标签控件
    uint16_t editbuf_size; // 编辑框缓冲区大小
    uint8_t focus;         // 输入框焦点状态
} Line_edit_t;
/// 文本框控件
typedef struct Txtbox_t
{
    sc_obj_t base;
    color_t color;
    color_t fill;
    sc_textbox_t tbox; // 文本框控件
} Txtbox_t;

/// 键盘控件
typedef struct Keyboard_t
{
    sc_obj_t base;
    color_t color;        // 填充色
    color_t border_color; // 边界色
    kb_ctx_t kb;
    Line_edit_t *edit;
    uint8_t  value;      // 键盘值
} Keyboard_t;

/// 图片控件
typedef struct
{
    sc_obj_t base;
    const sc_image_t *src;
} Image_t;
/// indexQOI 压缩图片控件
typedef struct
{
    sc_obj_t base;
    const uint8_t *src;
} Image_indexQOI_t;
/// 图片zip
typedef struct
{
    sc_obj_t base;
    const SC_img_zip *src;
    sc_dec_zip dec;
} Imagezip_t;

/// 图片变换
typedef struct
{
    sc_obj_t base;
    Transform_t params;
    int16_t cx;
    int16_t cy;
} Rotate_t;

/// 示波器控件
typedef struct
{
    sc_obj_t base;
    color_t color;
    color_t border_color;
    uint8_t xd;
    uint8_t yd;
    // SC_chart  buf[2];       //波形数量
} Chart_t;

// 菜单项结构体
typedef struct Menulist_t
{
    sc_obj_t base;
    color_t color;
    color_t border_color;
    uint8_t r;
    uint8_t ir;
    const char *text;
    uint8_t draw_st;
    uint8_t draw_cnt;
    // SC_Menu  menu;
} Menulist_t;

/* 设置透明度 */
static inline void sc_set_alpha(void *p, uint8_t alpha)
{
    sc_obj_t *obj = (sc_obj_t *)(p);
    obj->alpha = alpha;
    sc_obj_dirty(obj, NULL);
}
/*设置色彩*/
static inline void sc_set_color(void *p, color_t color, color_t fill)
{
    Rect_t *obj = (Rect_t *)(p);
    if (obj->base.type > SC_OBJ_TYPE_CANVAS && obj->base.type >= SC_OBJ_TYPE_BAES_END)
        return; // 如果没有这个属性，直接返回
    obj->color = color;
    obj->fill = fill;
    sc_obj_dirty(&obj->base, NULL); // 标记刷新
}
/* 设置大小 */
static inline void sc_set_x_y(void *p, int16_t x, int16_t y)
{
    sc_obj_t *obj = (sc_obj_t *)(p);
    sc_rect_t last = obj->rect;
    obj->rect.x = x;
    obj->rect.y = y;
    sc_obj_dirty(obj, &last);
}
/* 设置位置 */
static inline void sc_set_w_h(void *p, int16_t w, int16_t h)
{
    sc_obj_t *obj = (sc_obj_t *)(p);
    sc_rect_t last = obj->rect;
    obj->rect.w = h;
    obj->rect.h = h;
    sc_obj_dirty(obj, &last);
}
// 设置用户事件回调
static inline void sc_set_user_cb(void *p, void (*sc_user_cb)(struct sc_obj_t *obj, sc_event_t *e))
{
    sc_obj_t *obj = (sc_obj_t *)(p);
    obj->sc_user_cb = sc_user_cb;
}

// 创建屏幕
sc_obj_t *sc_create_srceen(sc_obj_t *parent, int x, int y, int w, int h, sc_align_t align);
// 创建Canvas
sc_obj_t *sc_create_canvas(sc_obj_t *parent, int x, int y, int w, int h, int virtual_w, int virtual_h, sc_align_t align);
// 创建矩形
sc_obj_t *sc_create_rect(sc_obj_t *parent, int x, int y, int w, int h, sc_align_t align);
// 创建LED
sc_obj_t *sc_create_led(sc_obj_t *parent, int cx, int cy, int r, int ir, sc_align_t align);
// 创建滑块
sc_obj_t *sc_create_slider(sc_obj_t *parent, int x, int y, int w, int h, sc_align_t align);
// 创建开关
sc_obj_t *sc_create_switch(sc_obj_t *parent, int x, int y, int w, int h, sc_align_t align);
// 创建圆弧
sc_obj_t *sc_create_arc(sc_obj_t *parent, int cx, int cy, int r, int ir, sc_align_t align);
// 设置圆弧角度
void sc_set_arc_deg(sc_obj_t *obj, int start_deg, int end_deg);
// 创建按钮
sc_obj_t *sc_create_button(sc_obj_t *parent, int x, int y, int w, int h, sc_align_t align);
// 设置按钮文本
void sc_set_button_text(sc_obj_t *obj, char *text);
// 创建标签
sc_obj_t *sc_create_label(sc_obj_t *parent, int x, int y, int w, int h, sc_align_t align);
// 设置标签文本
void sc_set_label_text(sc_obj_t *obj, char *text);
// 创建编辑框
sc_obj_t *sc_create_line_edit(sc_obj_t *parent, int x, int y, int w, int h, sc_align_t align);
// 编辑框绑定文本缓存
void sc_set_edit_text(sc_obj_t *obj, char *text, uint16_t editbuf_size);
// 创建文本框
sc_obj_t *sc_create_textbox(sc_obj_t *parent, int x, int y, int w, int h, sc_align_t align);

//
void sc_set_textbox_text(sc_obj_t *obj, char *text);

// 创建图片
sc_obj_t *sc_create_image(sc_obj_t *parent, int x, int y, const sc_image_t *src, sc_align_t align);
// 创建图片压缩
sc_obj_t *sc_create_image_zip(sc_obj_t *parent, int x, int y, const sc_image_zip *src, sc_align_t align);
// 创建 indexQOI 图片
sc_obj_t *sc_create_Image_indexQOI(sc_obj_t *parent, int x, int y, const unsigned char *src, sc_align_t align);
// 创建图片变换
sc_obj_t *sc_create_rotate(sc_obj_t *parent, const sc_image_t *src, int cx, int cy, float src_x, float src_y, sc_align_t align);
// 设置变换角度
void sc_set_Rotate_angle(sc_obj_t *obj, int angle);
// 设置变换缩放
void sc_set_Rotate_scale(sc_obj_t *obj, float scalex,float scaley);




#endif
