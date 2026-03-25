
#ifndef SC_DEMO_SIMPLE_H
#define SC_DEMO_SIMPLE_H
#include "sc_gui.h"

extern const unsigned char img_Lampblank_240x240[];
extern const unsigned char img_BG_240x240[];

extern const sc_image_t tempC_img_48; 
extern const sc_image_t EDA_img_32;      // 图像声明
extern const SC_img_zip logo_160_80_zip; // 压缩图片



///演示代码,文本显示
void sc_demo_text(void);

///演示代码,标签
void sc_demo_label(void);

///演示代码,开关
void sc_demo_Switch(void);

///演示代码,按钮
void sc_demo_button(void);

///演示代码,进度条
void sc_demo_Bar(void);

///演示代码,文本框
void sc_demo_Textbox(void);

///演示代码,图像
void sc_demo_Image(void);

///演示代码,图像
void sc_demo_indexQOI(void);

///演示代码圆弧
void sc_demo_Arc(void);

/// 演示代码性能测试spi_clk= 18单位Mhz
void sc_demo_pfs(int spi_clk);

/////演示代码
void sc_demo_keyboard(void);

#endif 