#ifndef SC_DEMO_TEST_H
#define SC_DEMO_TEST_H

#include "sc_gui.h"
#include "sc_demo_simple.h"

//演示代码,按钮显示
void sc_demo_gif_task(sc_event_t *event);
/// 示例图像旋转缩放任务
void sc_demo_trans_task(sc_event_t *event);

/// 示例脏矩形驱动任务
void sc_demo_drity_task(sc_event_t *event);

///// 示例图表任务
void sc_demo_chart_task(sc_event_t *event);

/// 示例文本任务
void sc_demo_Textbox_task(sc_event_t *event);
/// 示例菜单任务
void sc_demo_menu_task(sc_event_t *event);

/// 示例图像任务
void sc_demo_DrawEye_task(sc_event_t *event);

// 示例图像任务
void sc_watch_demo_task(sc_event_t *event);


extern void sc_demo_obj_watch(void);

extern void sc_demo_obj_but(void);

extern void sc_demo_obj_edit(void);
#endif 
// SC_DEMO_TEST_H
