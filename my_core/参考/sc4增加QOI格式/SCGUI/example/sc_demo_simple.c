#include "sc_demo_simple.h"
#include "sc_keyboard.h"
// 文本显示
void sc_demo_text(void)
{
    sc_rect_t coord = {10, 10, 60, 60};                                      // 对齐区域
    sc_draw_Text(NULL, 100, 10, gui->font, "hello\nSCGUI", C_RED, gui->bkc); // 简单文本
    //----label---
    sc_draw_Rounded_rect(NULL, &coord, 10, 4, C_BLUE, C_WHITE, 255);
    sc_draw_str(NULL, 0, 0, gui->font, "1234", C_BLUE, C_RED, &coord, ALIGN_CENTER); // 简单文本
    sc_draw_Line(NULL, 10, 10, 50, 40, C_RED);                                       // 简单线段
}

// label示例
void sc_demo_label(void)
{
    sc_rect_t coord = {10, 10, 50, 50};
    sc_label_t label;
    sc_label_t labe2;
    sc_label_t labe3;
    sc_init_Label(&label, 0, 0, gui->font, "SCGUI", ALIGN_CENTER | ALIGN_BORDER_VIS);                           // 居中显示
    sc_init_Label(&labe2, 0, 0, gui->font, "1234567890", ALIGN_CENTER | ALIGN_REVERSE_MODE);                    // 居中反显示
    sc_init_Label(&labe3, 0, -10, gui->font, "ABCDEFGHIJ", ALIGN_CENTER | ALIGN_BORDER_VIS | ALIGN_AUTO_BREAK); // 居中显示，自动换行
    sc_draw_Label(NULL, &coord, &label, C_RED, gui->bkc);
    coord.x += 55;
    sc_draw_Label(NULL, &coord, &labe2, C_RED, gui->bkc);
    coord.x += 55;
    sc_draw_Label(NULL, &coord, &labe3, C_RED, gui->bkc);
}

// Switch示例
void sc_demo_Switch(void)
{
    sc_rect_t coord = {10, 50, 30, 20};
    sc_draw_Switch(NULL, &coord, 10, 8, C_ROYAL_BLUE, C_RED, 255, 0); // 关
    coord.y += 50;
    sc_draw_Switch(NULL, &coord, 10, 8, C_ROYAL_BLUE, C_RED, 255, 1); // 关
}

// button示例
void sc_demo_button(void)
{
    sc_rect_t coord = {20, 20, 50, 30};
    sc_button_t btn1;
    sc_button_t btn2;
    sc_init_button(&btn1,   gui->font, "but1", C_WHITE, ALIGN_CENTER);
    sc_init_button(&btn2,   gui->font, "but2", C_WHITE, ALIGN_CENTER);
    sc_draw_button(NULL, &coord, &btn1,C_ROYAL_BLUE, C_BLUE,8, 6,255);
    coord.x += 55;
    sc_draw_button(NULL, &coord, &btn2,C_RED, C_GRAY,4, 4,255);
}

// 进度条示例
void sc_demo_Bar(void)
{
    sc_rect_t box = {10, 10, 100, 20};
    sc_draw_Bar(NULL, &box, 9, 0, C_RED, C_BLUE, 120, 50);
    box.y += 50;
    sc_draw_Bar(NULL, &box, 9, 8, C_RED, C_BLUE, 120, 50);
}

// 文本框示例
void sc_demo_Textbox(void)
{
    const char *str = "hello world this is a textbox demo";
    sc_textbox_t tbox;
    sc_rect_t coord = {10, 10, SC_SCREEN_WIDTH - 20, SC_SCREEN_HEIGHT - 20};
    sc_init_Textbox(&tbox, 0, 0, gui->font, str);
    sc_draw_Textbox(NULL, &coord, &tbox, C_BLACK, gui->bkc);
}

// 演示压缩图片
void sc_demo_Image(void)
{
    sc_dec_zip dec;
    sc_draw_Image_zip(NULL, 0, 0, &logo_160_80_zip, &dec);
    sc_draw_Image(NULL, 10, 10, &tempC_img_48);
    sc_draw_Image(NULL, 80, 10, &EDA_img_32);
}
// 演示压缩图片
void  sc_demo_indexQOI(void)
{
	sc_draw_Image_indexQOI(NULL, 20,0, img_Lampblank_240x240, 255);
}
/// 演示代码性能测试spi_clk= 18单位Mhz
void sc_demo_pfs(int spi_clk)
{
    char str_buf[20];
    //-------清屏4次---------
    uint32_t t1, t2, t3;
    system_tick = 0;
    color_t colour[4] = {C_RED, C_BLUE, C_GREEN, C_YELLOW};
    for (int i = 0; i < 4; i++)
    {
        sc_clear(0, 0, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT, colour[i]);
    }
    t1 = system_tick;
    //------圆角矩形4次---------
    system_tick = 0;
    for (int i = 0; i < 4; i++)
    {
        sc_draw_Rounded_rect(NULL, &gui->lcd_rect, 30, 10, colour[i], C_WHEAT, 255);
    }
    t2 = system_tick;
#if 1
    //------圆弧4次---------
    system_tick = 0;
    int r = SC_MIN(SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT) / 2;
    sc_arc_t arc = {SC_SCREEN_WIDTH / 2, SC_SCREEN_HEIGHT / 2, r, r - 20, 1};
    for (int i = 0; i < 4; i++)
    {
        sc_draw_Arc(NULL, &arc, 30, 330, C_GREEN, C_GREEN,gui->bkc, 255);
    }
    t3 = system_tick;
#else
    //------长文本---------
    system_tick = 0;
    for (int i = 0; i < 4; i++)
    {
        sc_demo_Textbox();
    }
    t3 = system_tick;
#endif
    //------统计用时---------
    int teim = SC_SCREEN_WIDTH * SC_SCREEN_HEIGHT * 16 / (spi_clk * 1000);
    sprintf(str_buf, "time=%d ms @%dM", teim, spi_clk); // 18M理论用时
    sc_draw_Text(NULL, 20, 5, gui->font, str_buf, gui->fc, gui->bkc);

    sprintf(str_buf, "t1=%d ms @clear", t1 / 4); // 清屏4次用时
    sc_draw_Text(NULL, 20, 25, gui->font, str_buf, gui->fc, gui->bkc);

    sprintf(str_buf, "t2=%d ms @rect", t2 / 4); // 绘制4次用时
    sc_draw_Text(NULL, 20, 45, gui->font, str_buf, gui->fc, gui->bkc);

    sprintf(str_buf, "t3=%d ms @arc", t3 / 4); // 绘制4次用时
    sc_draw_Text(NULL, 20, 65, gui->font, str_buf, gui->fc, gui->bkc);
}

// 演示圆弧
void sc_demo_Arc(void)
{
    sc_arc_t arc = {SC_SCREEN_WIDTH / 3, SC_SCREEN_HEIGHT / 2, 40, 20, 0};
    sc_draw_Arc(NULL, &arc, 60, 280, C_YELLOW, C_YELLOW, C_BLUE, 128);
    arc.cx = SC_SCREEN_WIDTH * 2 / 3;
    arc.dot = 1;
    sc_draw_Arc(NULL, &arc, 60, 280, C_YELLOW, C_RED, gui->bkc, 255);

}

// 绘制组合图形
void sc_draw_compose(sc_pfb_t *dest, sc_rect_t *box, void *arg)
{
    sc_area_t intersection;
    sc_pfb_t tpfb;
    sc_area_t area = {box->x, box->y, box->x + box->w, box->y + box->h};
    if (dest == NULL)
    {
        dest = &tpfb;
        sc_pfb_init_slices(dest, &area, gui->bkc);
    }
    if (!sc_pfb_intersection(dest, &area, &intersection))
        return;
    do
    {
        // 组合图形
    } while ((dest == &tpfb) && sc_pfb_next_slice(dest) && sc_pfb_intersection(dest, &area, &intersection));
}

#include "sc_keyboard.h"
void sc_demo_keyboard(void)
{
    sc_rect_t box = {0, SC_SCREEN_HEIGHT / 2, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT / 2};
    kb_ctx_t kb;
    sc_init_keyboard(&kb, 2);
    sc_draw_keyboard(NULL, &box, &kb, C_RGB(100, 100, 100), C_WHITE, C_WHITE); // 绘制键盘
}