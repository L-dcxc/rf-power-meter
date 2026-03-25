#ifndef SC_GUI_H
#define SC_GUI_H

#include "sc_arc.h"
#include "sc_event_task.h"
#include "sc_lvgl_font.h"
#include "sc_transform.h"
#include "sc_menu.h"

void spiflash_read(uint8_t *buf, uint32_t offset, uint32_t size);

typedef void (*lcd_refresh_cb)(uint16_t x, uint16_t y, uint16_t w, uint16_t h, color_t *color); // 点绘制函数指针
/* SC_GUI结构体 */
typedef struct sc_gui_t
{
    sc_rect_t lcd_rect;        // 屏幕矩形
    sc_rect_t *g_mask;         // 屏幕矩形
    lcd_refresh_cb refresh_cb; // 底层刷新函数
    void *font;                // 主字体
    uint16_t bkc;              // 主题底色
    uint16_t fc;               // 前影色
    uint16_t bc;               // 背影色
    uint16_t alpha;            // 透明度
} sc_gui_t;

extern sc_gui_t *gui;          // 全局gui
/* 图片结构体 */
typedef struct
{
    const uint8_t *map;
    const uint8_t *mask;
    uint32_t w : 12;
    uint32_t h : 12;
    uint32_t bpp : 7;
    uint32_t exf : 1;
}SC_img_t ;
typedef SC_img_t sc_image_t ; // 上位机命名不规范，这里统一命名

/* 压缩图结构体 */
typedef struct
{
    const uint8_t *map;
    uint32_t len;
    uint16_t w;
    uint16_t h : 15;
    uint16_t exf : 1;
} SC_img_zip;
typedef SC_img_zip sc_image_zip ;// 上位机命名不规范，这里统一命名

typedef struct
{
    uint32_t n;
    int16_t x;
    int16_t y;
    uint16_t rep_cnt;
    uint16_t out;
    uint16_t unzip;
    const uint8_t *map;
} sc_dec_zip;

/*波形图结构体定义*/
typedef struct
{
    int16_t dat_buf[240];
    int16_t last;
    uint16_t wp;   // 队列
    uint32_t indx; // 当前显示位置
    color_t color;
} sc_chart_t;

/*按键结构体定义*/
typedef struct
{
    color_t tc;      // 文字颜色
    lv_font_t *font; // 字体
    char *text;      // 文字内容
    uint8_t align;   // 文字对齐
} sc_button_t;

/*文本框结构体定义*/
typedef struct
{
    lv_font_t *font;
    char *text;
    int32_t xofs : 12;
    int32_t yofs : 12;
    int32_t align : 8;
    uint8_t r;     // 圆角
    uint8_t bar_w; // 进度条宽
} sc_textbox_t;

/* 高性能写入 */ 
static inline void alphaBlend_fast(uint16_t fc, uint16_t *dest, uint16_t alpha)
{
    if (alpha > 251)
        *dest = fc;
    else
        *dest = alphaBlend(fc, *dest, alpha);
}
/* 初始化gui */
void sc_gui_init(lcd_refresh_cb refresh_cb, color_t bkc, color_t fc, color_t bc, void *font); // 初始化

/* 清屏 */
void sc_clear(int x, int y, int w, int h, color_t color); // 清屏

/*绘制点*/ 
void sc_draw_point(sc_pfb_t *dest, int x, int y, uint32_t color);

/*绘制色块*/
void sc_draw_Fill(sc_pfb_t *dest, int xs, int ys, int w, int h, color_t color, uint16_t alpha);

/* 空心矩形 */
void sc_draw_Frame(sc_pfb_t *dest, int xs, int ys, int w, int h, int lw, color_t color, uint16_t alpha);

/* 绘制线 */
void sc_draw_Line(sc_pfb_t *dest, int x1, int y1, int x2, int y2, color_t colour);

/* 绘制圆角矩形*/
bool sc_draw_Rounded_rect(sc_pfb_t *dest, sc_rect_t *box, int r, int ir, color_t color, color_t fill, uint16_t alpha);

/* 绘制圆形LED*/
void sc_draw_Led(sc_pfb_t *dest, int cx, int cy, int r, color_t color, uint16_t alpha);

/* 绘制开关*/
void sc_draw_Switch(sc_pfb_t *dest, sc_rect_t *box, int r, int ir, color_t color, color_t fill, uint16_t alpha, uint8_t state);

/* 按钮初始化*/
void sc_init_button(sc_button_t *btn,   lv_font_t *font, const char *text, color_t tc, sc_align_t align);

/* 绘制按钮*/
void sc_draw_button(sc_pfb_t *dest, sc_rect_t *box, sc_button_t *btn,color_t ac, color_t bc,int r, int ir,uint16_t alpha);

/* 绘制进度条 */ 
void sc_draw_Bar(sc_pfb_t *dest, sc_rect_t *box, int r, int ir, color_t color, color_t bkc, uint16_t alpha, int vol);

/* 初始文本框*/ 
void sc_init_Textbox(sc_textbox_t *tbox, int xofs, int yofs, lv_font_t *font, const char *text); 

/* 绘制文本框 */ 
void sc_draw_Textbox(sc_pfb_t *dest, sc_rect_t *box, sc_textbox_t *tbox,color_t fc, color_t bc);

/* 绘制icon字符*/
void sc_draw_Icon(sc_pfb_t *dest, int xs, int ys, int w, int h, const uint8_t *src, uint8_t bpp, color_t fc, uint16_t alpha);

/* 绘制图片*/
void sc_draw_Image(sc_pfb_t *dest, int xs, int ys, const sc_image_t *src);

void sc_draw_Image_indexQOI(sc_pfb_t *dest, int xs, int ys, const uint8_t *src, uint8_t alpha);
/* 绘制压缩图片*/
void sc_draw_Image_zip(sc_pfb_t *dest, int xs, int ys, const sc_image_zip *zip, sc_dec_zip *dec);

/* 波形图数据写入*/
void sc_put_Chart_ch(sc_chart_t *p, int16_t vol, uint16_t scaleX, color_t color);

/* 绘制波形图*/
void sc_draw_Chart(sc_pfb_t *dest, int xs, int ys, int w, int h, uint16_t gc, int gx, int gy, sc_chart_t *p, int size);




#endif
