

#include "sc_demo_test.h"

//@breif 演示代码,波形图表显示
void sc_demo_chart_task(sc_event_t *event)
{
    static sc_chart_t chart[2];
    static uint16_t angle0, vol;
    if (event->type == SC_EVENT_TYPE_INIT) // 创键时初始化
    {
    }
    else if (event->type == SC_EVENT_TYPE_TIMER) // 10ms周期
    {
        vol++;
        angle0 = (angle0 + 2) % 360;
        int16_t adc0 = sc_sin(angle0) >> 9;
        sc_put_Chart_ch(&chart[0], adc0, 128, C_BLUE);
        sc_put_Chart_ch(&chart[1], adc0 / 2, 256, C_YELLOW);
        sc_draw_Chart(NULL, 40, 40, 240, 160, 0, 30, 30, chart, 2);
    }
}

//@brief 演示代码,动画显示
void sc_demo_gif_task(sc_event_t *event)
{
    // extern const SC_img_zip kof97b_buf[86]; // 外部
    extern const SC_img_zip bashen_buf[21]; // 内部
    static int buf_i = 0;
    sc_dec_zip dec;
    int buf_max = sizeof(bashen_buf) / sizeof(bashen_buf[0]);
    if (event->type == SC_EVENT_TYPE_TIMER) // 10ms周期
    {
        sc_area_t area = {0, 0, bashen_buf[0].w, bashen_buf[0].h};
        sc_pfb_t tpfb;
        sc_pfb_init_slices(&tpfb, &area, gui->bkc); // 初始化切片
        gui->alpha = 120;
        do
        {
            sc_draw_Image_zip(&tpfb, 0, 0, &bashen_buf[buf_i], &dec);
        } while (sc_pfb_next_slice(&tpfb));
        if (++buf_i >= buf_max)
        {
            buf_i = 0;
        }
    }
}





/// 示例脏矩移动
static void sc_move_rect(sc_rect_t *p, int x, int y)
{
    int w = (SC_SCREEN_WIDTH + p->w);
    int h = (SC_SCREEN_HEIGHT + p->h);
    sc_dirty_mark(NULL, p);
    p->x += x;
    p->y += y;
    p->x += (p->x > SC_SCREEN_WIDTH) ? -w : ((p->x < -(int)p->w) ? w : 0);  // X轴折返
    p->y += (p->y > SC_SCREEN_HEIGHT) ? -h : ((p->y < -(int)p->h) ? h : 0); // Y轴折返
    sc_dirty_mark(NULL, p);
}
static u16 pfs_cnt = 0;
static u32 pfs_time = 0;
/// 示例脏矩形驱动任务
void sc_demo_drity_task(sc_event_t *event)
{
    static int angle = 0;
    static char str_buf[20] = "PFS:0 ";
    static sc_rect_t rect1 = {10, 50, 50, 60};
    static sc_rect_t rect2 = {20, 60, 40, 50};
    static sc_rect_t rect3 = {30, 70, 50, 60};
    static sc_rect_t rect4 = {40, 80, 40, 50};
    static sc_rect_t rect5 = {200, 200, 60, 20};
    sc_rect_t Slider = {10, 160, 120, 16};              // 滑块常量
    sc_rect_t Slider1 = {270, 60, 18, 120};             // 滑块常量
    int16_t cx = SC_SCREEN_WIDTH / 2;                   // X中心
    int16_t cy = SC_SCREEN_HEIGHT / 2;                  // Y中心
    sc_arc_t arc1 = {cx - 50, cy - 20, 60, 60 - 20, 1}; // 圆弧常量
    sc_arc_t arc2 = {cx + 50, cy + 20, 60, 60 - 20, 1}; // 圆弧常量
    sc_rect_t arc_rect = {arc1.cx - arc1.r, arc1.cy - arc1.r, arc1.r * 2 + 1, arc1.r * 2 + 1};
    if (event->type == SC_EVENT_TYPE_INIT)
    {
        sc_dirty_mark(NULL, &gui->lcd_rect); // 标记脏矩形
        pfs_time = system_tick + 500;
    }
    else if (event->type == SC_EVENT_TYPE_TIMER) // 10ms周期
    {
        sc_dirty_mark(NULL, &Slider);  // 标记脏矩形
        sc_dirty_mark(NULL, &Slider1); // 标记脏矩形
        //   sc_dirty_mark(NULL, &arc_rect); // 标记脏矩形
        //-------移动矩形标记脏矩形----------------
        sc_move_rect(&rect1, 2, 3);
        sc_move_rect(&rect2, -2, 4);
        sc_move_rect(&rect3, 2, 3);
        sc_move_rect(&rect4, -2, 5);
        angle = (angle + 2) % 360;
        sc_dirty_mark(NULL, &rect5);
    }
    sc_pfb_t tpfb;
    uint8_t dirty_cnt = 0;
    sc_area_t *p_dirty = sc_dirty_merge_out(&dirty_cnt); // 获取脏矩形
    for (int i = 0; i < dirty_cnt; i++)
    {
        sc_pfb_init_slices(&tpfb, &p_dirty[i], gui->bkc); // 初始化切片
        do
        {
            sc_draw_Fill(&tpfb, rect1.x, rect1.y, rect1.w, rect1.h, C_GREEN, 255);
            sc_draw_Fill(&tpfb, rect2.x, rect2.y, rect2.w, rect2.h, C_RED, 255);
            sc_draw_Rounded_rect(&tpfb, &rect3, 18, 15, gui->fc, gui->bc, 255);
            sc_draw_Rounded_rect(&tpfb, &rect4, 8, 5, gui->fc, gui->bc, 255);
            sc_draw_Arc(&tpfb, &arc1, 50, 310, C_RED, C_RED, gui->bkc, 255);
            // sc_draw_Arc(&tpfb, &arc2, 50, 310, C_YELLOW, C_YELLOW, 255);
            sc_draw_Image(&tpfb, 200, 120, &tempC_img_48);
            sc_draw_Image(&tpfb, 100, 120, &EDA_img_32);
            sc_draw_Bar(&tpfb, &Slider, 8, 6, C_RED, C_GREEN, 255, angle * 100 / 360);
            sc_draw_Bar(&tpfb, &Slider1, 8, 6, C_RED, C_GREEN, 255, angle * 100 / 360);

        } while (sc_pfb_next_slice(&tpfb));
    }
    pfs_cnt++;
    if ((int)pfs_time - (int)system_tick <= 0)
    {
        pfs_time = system_tick + 1000;
        sprintf(str_buf, "PFS:%d  ", pfs_cnt);
        pfs_cnt = 0;
    }
    sc_draw_Text(NULL, 10, 10, gui->font, str_buf, C_WHEAT, gui->bc);
}

// 文本框示例
void sc_demo_Textbox_task(sc_event_t *event)
{
    static const char *str = "hello world this is a textbox demo";
    static sc_textbox_t tbox;
    sc_rect_t coord = {SC_SCREEN_WIDTH / 4, SC_SCREEN_HEIGHT / 4, SC_SCREEN_WIDTH / 2, SC_SCREEN_HEIGHT / 2};
    switch (event->type)
    {
    case SC_EVENT_TYPE_INIT:
        sc_init_Textbox(&tbox, 0, 0, gui->font, str);
        sc_draw_Textbox(NULL, &coord, &tbox, C_BLACK, gui->bkc);
        break;
    case SC_EVENT_TYPE_CMD:
        if (event->dat.cmd == CMD_UP)
        {
            tbox.yofs += 5;
            sc_draw_Textbox(NULL, &coord, &tbox, C_BLACK, gui->bkc);
        }
        else if (event->dat.cmd == CMD_DOWN)
        {
            tbox.yofs -= 5;
            sc_draw_Textbox(NULL, &coord, &tbox, C_BLACK, gui->bkc);
        }
        break;
    default:
        break;
    }
}

// 示例菜单任务
void sc_demo_menu_task(sc_event_t *event)
{
    static sc_menu_t test_menu;
    // 接收所有事件
    switch (event->type)
    {
    case SC_EVENT_TYPE_INIT:
        sc_menu_init(&test_menu, 100, 50, 100, 25);
        sc_menu_loop_key(&test_menu, CMD_ENTER);
        break;
    case SC_EVENT_TYPE_CMD:
        sc_menu_loop_key(&test_menu, event->dat.cmd);
        break;
    default:
        break;
    }
}

void sc_demo_DrawEye_task(sc_event_t *event)
{
    sc_rect_t rect1 = {10, 10, 120, 120};
    sc_rect_t rect2 = {160, 10, 120, 120};
    static int Eye = 0;
    static int stup = 2;
    if (event->type == SC_EVENT_TYPE_INIT)
    {
        DrawEye_Blink_test(NULL, rect1.x, rect1.y, rect1.w, rect1.h, 0, 0, 0);
    }
    else if (event->type == SC_EVENT_TYPE_TIMER) // 10ms周期
    {
        Eye += stup;
        if (Eye < -10 || Eye > 40)
        {
            stup = -stup;
        }
        DrawEye_Blink_test(NULL, rect1.x, rect1.y, rect1.w, rect1.h, Eye, Eye, C_WHITE);
        //  DrawEye_Blink_test(NULL, rect2.x, rect2.y, rect2.w, rect2.h, Eye, Eye, C_WHITE);
    }
}

// 旋转示例
typedef struct
{
    Transform_t T; // 旋转参数
    sc_rect_t box; // 旋转参数
} my_rotate_t;

extern const unsigned char img_BG_240x240[];
extern const sc_image_t watch_s_img;
static my_rotate_t rotate;
static int angle = 0;

void sc_watch_demo_task(sc_event_t *event)
{
    static int16_t stup = 1;
    sc_rect_t bg_box = {20, 0, 240, 240};
    int cx = bg_box.x + bg_box.w / 2;     // X中心
    int cy = bg_box.y + bg_box.h / 2 - 1; // Y中心
    if (event->type == SC_EVENT_TYPE_INIT)
    {
        angle = 0;
        sc_init_transform(&rotate.box, (void *)&watch_s_img, &rotate.T);           // 设置图像
        sc_set_transform_center(&rotate.T, watch_s_img.w / 2, watch_s_img.h - 23); // 设置旋转中心
        sc_dirty_mark(NULL, &bg_box);                                              // 标记脏矩形
    }
    else if (event->type == SC_EVENT_TYPE_TIMER) // 10ms周期
    {
        angle += stup;
        if (angle >= 90 ||angle <= -90)
            stup = -stup;
        sc_set_transform_angle(cx, cy, angle, &rotate.T); // 设置旋转角度
        sc_dirty_mark(NULL, &rotate.T.drity);                    // 标记脏矩形
    }
    //============所有事件都会执行===========
    sc_pfb_t tpfb;
    uint8_t dirty_cnt = 0;
    sc_area_t *p_dirty = sc_dirty_merge_out(&dirty_cnt); // 获取脏矩形
    for (int i = 0; i < dirty_cnt; i++)
    {
        sc_pfb_init_slices(&tpfb, &p_dirty[i], gui->bkc); // 初始化切片
        do
        {
				    sc_draw_Image_indexQOI(&tpfb, 20,0, img_BG_240x240, 255);
            sc_draw_transform(&tpfb, cx, cy, &rotate.T);
        } while (sc_pfb_next_slice(&tpfb));
    }
}




//@brief 演示代码,图像旋转缩放
void sc_demo_trans_task(sc_event_t *event)
{
	  static sc_image_t label_image; // 图像缓存
  
	  static my_rotate_t label;
    int16_t cx = SC_SCREEN_WIDTH / 3;      // X中心
    int16_t cy = SC_SCREEN_HEIGHT / 2;     // Y中心
    if (event->type == SC_EVENT_TYPE_INIT) // 创键时初始化
    {
        // 图像旋转缩放
        sc_init_transform(&rotate.box, (void *)&EDA_img_32, &rotate.T);
			  sc_init_transform_text(&label.box, gui->font, "1234", &label_image, &label.T);
        sc_dec_zip dec;
        sc_draw_Image_zip(NULL, 0, 0, &logo_160_80_zip, &dec);
			
			  sc_set_transform_angle(cx, cy, angle, &rotate.T);
			  sc_set_transform_angle(cx<<1, cy, angle, &label.T);
    }
    else if (event->type == SC_EVENT_TYPE_TIMER) // 10ms周期
    {
        angle = (angle + 1) % 360;
        sc_set_transform_angle(cx, cy, angle, &rotate.T);
        sc_draw_transform(NULL, cx, cy, &rotate.T);
			
				float scale= (float)angle/180+0.5;
			  sc_set_transform_scale( &label.T, scale,scale);
				sc_set_transform_angle(cx<<1, cy, angle, &label.T);
				sc_draw_transform(NULL, cx<<1, cy, &label.T);
    }
}
