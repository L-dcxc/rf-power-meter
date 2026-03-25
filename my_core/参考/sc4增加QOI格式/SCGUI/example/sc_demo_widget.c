
#include "sc_widget.h"

// 旋转控件事件处理函数
void my_watch_cb(sc_delay_t *self)
{
    static int stup = 0;
    stup = (stup + 1) % 360;
    sc_set_Rotate_angle(self->arg, stup); // 设置旋转角度
   // if (stup < 90)
    {
        self->time_out = sc_get_tick() + 1000 / 6; // 重新设置下一次触发时间
    }
}
const char *txt_str = "hello world this is a textbox demo";
/// @brief 演示代码
void sc_demo_obj_watch(void)
{
		extern const unsigned char img_Lampblank_240x240[];
	  extern const unsigned char img_BG_240x240[];
    extern const sc_image_t watch_s_img;
    extern const sc_image_t watch_m_img;
    extern const sc_image_t watch_h_img;
    int cx = 240 / 2; // 获取图片中心点
    int cy = 240 / 2 - 1;

    g_touch.srceen = sc_create_srceen(NULL, 0, 0, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT, ALIGN_CENTER);
    sc_obj_t *canvas = sc_create_canvas(g_touch.srceen, 0, 0, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT, SC_SCREEN_WIDTH * 3, SC_SCREEN_HEIGHT * 1, ALIGN_CENTER);
	  sc_obj_t *watch=sc_create_Image_indexQOI(canvas, 0, 0,img_BG_240x240  , ALIGN_CENTER);
	
    sc_obj_t *label = sc_create_label(watch, -64, 8, 50, 30, ALIGN_CENTER | 0);
    sc_obj_t *labe2 = sc_create_label(watch, 64, 8, 40, 30, ALIGN_CENTER | 0);
    sc_obj_t *labe3 = sc_create_label(watch, 0, 72, 40, 30, ALIGN_CENTER | 0);

    sc_set_label_text(label, "87");
    sc_set_label_text(labe2, "678");
    sc_set_label_text(labe3, "1234");
    sc_set_color(label, C_RED, gui->bkc);
    sc_set_color(labe2, C_YELLOW, gui->bkc);
    sc_set_color(labe3, C_GREEN, gui->bkc);

    sc_obj_t *Rotate_h = sc_create_rotate(watch, &watch_h_img, cx, cy, watch_h_img.w / 2 - 0.5, watch_h_img.h - 10, ALIGN_CENTER);
    sc_obj_t *Rotate_m = sc_create_rotate(watch, &watch_m_img, cx, cy, watch_m_img.w / 2 - 0.5, watch_m_img.h - 10, ALIGN_CENTER);
    sc_obj_t *Rotate_s = sc_create_rotate(watch, &watch_s_img, cx, cy, watch_s_img.w / 2 + 0.5, watch_s_img.h - 23, ALIGN_CENTER);

    sc_set_Rotate_angle(Rotate_m, 90);
    sc_set_Rotate_angle(Rotate_h, 120);
    sc_set_Rotate_angle(Rotate_s, 0);
		sc_set_Rotate_scale(Rotate_s, 0.8, 0.8);
    sc_add_delay(0, my_watch_cb, 1000 / 6, Rotate_s); // 创建一个10ms周期的任务

		sc_obj_t *qoi=sc_create_Image_indexQOI(canvas, SC_SCREEN_WIDTH, 0,img_Lampblank_240x240, ALIGN_CENTER);
		sc_obj_t *tbox= sc_create_textbox(canvas, SC_SCREEN_WIDTH*2, 0, 160, 160, ALIGN_CENTER); // 创建文本框控件
		
		 sc_set_label_text(tbox, (char*)txt_str);
		
		
		g_touch.obj_focus=canvas;
}


/// 用户事件回调示例
void  my_but_cb(struct sc_obj_t *obj, sc_event_t *e)
{
    Button_t *p = (Button_t *)obj;
    if (e->type == SC_EVENT_TOUCH_UP)
    {
        if (obj->flag & SC_OBJ_FLAG_BOOL)
        {
            p->fill = C_RED;
            sc_set_button_text(obj, "ON");
        }
        else
        {
            p->fill = C_YELLOW;
            sc_set_button_text(obj, "OFF");
        }
    }  
}
/// 用户事件回调示例
void  my_but2_cb(struct sc_obj_t *obj, sc_event_t *e)
{
    if (e->type == SC_EVENT_TOUCH_DOWN)
    {
        if (obj->flag & SC_OBJ_FLAG_BOOL)
        {
            sc_set_button_text(obj, "ON2");
        }
        else
        {
            sc_set_button_text(obj, "OFF2");
        }
    }  
}
// 示例
void sc_demo_obj_but(void)
{
    g_touch.srceen = sc_create_srceen(NULL, 0, 0, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT, ALIGN_CENTER);
    sc_obj_t *but = sc_create_button( g_touch.srceen, -40, 0, 50, 30, ALIGN_CENTER);
    sc_obj_t *but2 = sc_create_button( g_touch.srceen, 40, 0, 50, 30, ALIGN_CENTER);
    sc_set_user_cb(but, my_but_cb);
    sc_set_user_cb(but2, my_but2_cb);
}


void sc_demo_obj_edit(void)
{
    static char edit_buf[32] = "scgui\nhello world";
    static char edit_buf2[32] = "sc_edit_demo";
    g_touch.srceen = sc_create_srceen(NULL, 0, 0, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT, ALIGN_CENTER);
    sc_obj_t *rect1 = sc_create_rect(g_touch.srceen , 0, 0, 200, 200, ALIGN_CENTER);
    sc_set_color(rect1, C_WHEAT, gui->bkc);

    sc_obj_t *line_edit = sc_create_line_edit(rect1, 0, -40, 100, 40, ALIGN_CENTER);
    sc_set_edit_text(line_edit, edit_buf, 32);
    
    sc_obj_t *line_edit2 = sc_create_line_edit(rect1, 0, 20, 100, 50, ALIGN_CENTER);
    sc_set_edit_text(line_edit2, edit_buf2, 32);
}