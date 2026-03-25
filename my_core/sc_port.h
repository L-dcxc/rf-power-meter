#ifndef SC_PORT_H
#define SC_PORT_H

#include "sc_gui.h"

/* ===== SCGUI 接口初始化 =====
 * 在 System_BootSequence() 或 main() 完成 LCD_Init() 之后调用一次 */
void SC_Port_Init(void);

/* ===== 每帧同步 tick（在主循环每次迭代开始时调用）=====
 * 将 HAL_GetTick() 的毫秒计数同步给 SCGUI 任务调度器 */
void SC_Port_Tick(void);

/* ===== 按键注入 =====
 * 在原有按键处理逻辑里，将按键转发给 SCGUI 事件队列
 * key_id: 0=OK, 1=UP, 2=DOWN */
void SC_Port_InjectKey(uint8_t key_id);

#define SC_KEY_OK   0
#define SC_KEY_UP   1
#define SC_KEY_DOWN 2

/* ===== 字体外部声明（直接使用字体变量）===== */
extern lv_font_t lv_font_12;
extern lv_font_t lv_font_16;
extern lv_font_t lv_font_20;

#endif /* SC_PORT_H */
