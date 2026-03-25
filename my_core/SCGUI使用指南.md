# SCGUI 移植使用指南

> 适用于本项目：STM32F103 + ST7735S 161×130 TFT LCD

---

## 一、Keil 工程配置

### 1.1 添加源文件到工程
在 Keil **Project → Manage Project Items** 中新建 Group `my_core/scgui`，添加以下文件：

| 文件 | 路径 |
|------|------|
| `sc_common.c` | `my_core/scgui/` |
| `sc_gui.c` | `my_core/scgui/` |
| `sc_arc.c` | `my_core/scgui/` |
| `sc_event_task.c` | `my_core/scgui/` |
| `sc_lvgl_font.c` | `my_core/scgui/` |
| `sc_menu.c` | `my_core/scgui/` |
| `sc_transform.c` | `my_core/scgui/` |
| `sc_indexQOI.c` | `my_core/scgui/` |
| `lv_font_12.c` | `my_core/scgui/` |
| `lv_font_16.c` | `my_core/scgui/` |
| `sc_port.c` | `my_core/` |

### 1.2 添加头文件包含路径
**Options for Target → C/C++ → Include Paths** 中添加：
```
.\my_core\scgui
.\my_core
```

### 1.3 检查 Heap Size
**startup_stm32f103xx.s** 中确认 Heap_Size 不小于 `0x200`（512字节）。
SCGUI 核心 PFB 用静态数组，heap 主要用于菜单等动态控件。

---

## 二、初始化（修改 main.c）

在 `System_BootSequence()` 完成或 `main()` 里 LCD 初始化后，在 `USER CODE BEGIN` 块中加入：

```c
/* USER CODE BEGIN 2 */
#include "sc_port.h"

SC_Port_Init();   /* 初始化 SCGUI，只调用一次 */
/* USER CODE END 2 */
```

主循环：
```c
/* USER CODE BEGIN WHILE */
while (1)
{
    SC_Port_Tick();         /* 同步 ms 计数给任务调度器 */
    sc_task_loop(NULL);     /* SCGUI 任务调度（驱动所有 create_task 注册的界面） */
    /* USER CODE END WHILE */
```

---

## 三、按键集成

在原有按键扫描处理里，把按键转发给 SCGUI：

```c
#include "sc_port.h"

/* 原有按键检测代码，在判断到按键后调用 */
if (key == KEY_UP)   SC_Port_InjectKey(SC_KEY_UP);
if (key == KEY_DOWN) SC_Port_InjectKey(SC_KEY_DOWN);
if (key == KEY_OK)   SC_Port_InjectKey(SC_KEY_OK);
```

---

## 四、SCGUI 核心绘图 API

> **注意**：所有 `dest` 参数传 `NULL` 表示"独立刷新此区域到屏幕"。
> 在任务回调（`sc_draw_event`）里传实际 `pfb` 可做局部刷新。

### 4.1 坐标与颜色体系

```c
/* 颜色常量（RGB565）*/
C_BLACK, C_WHITE, C_RED, C_GREEN, C_BLUE, C_YELLOW, C_CYAN
C_ROYAL_BLUE, C_ORANGE, C_GRAY, C_DARK_GRAY, C_LIME
C_RGB(r, g, b)   /* 自定义颜色：r/g/b 范围 0-255 */

/* 矩形结构体 */
sc_rect_t box = {x, y, w, h};  /* 左上角坐标 + 宽高 */
```

### 4.2 清屏 / 填充

```c
/* 清整屏 */
sc_clear(0, 0, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT, C_BLACK);

/* 填充矩形区域，alpha: 0=透明 255=不透明 */
sc_draw_Fill(NULL, x, y, w, h, C_BLUE, 255);
sc_draw_Fill(NULL, x, y, w, h, C_BLUE, 128);  /* 半透明 */
```

### 4.3 文字

```c
#include "sc_port.h"  /* 包含字体声明 */

/* 简单文字（最常用）*/
sc_draw_Text(NULL, x, y, &lv_font_12, "Hello!", C_WHITE, C_BLACK);
sc_draw_Text(NULL, x, y, &lv_font_16, "RF Power", C_CYAN, C_BLACK);

/* 格式化文字（先 sprintf 再绘制）*/
char buf[32];
sprintf(buf, "%.1f W", power);
sc_draw_Text(NULL, 10, 50, &lv_font_16, buf, C_GREEN, C_BLACK);

/* 对齐文字（在指定矩形内居中）*/
sc_rect_t box = {0, 0, 161, 20};
sc_draw_str(NULL, 0, 0, &lv_font_16, "VSWR", C_YELLOW, C_BLACK, &box, ALIGN_CENTER);

/* 对齐枚举 */
ALIGN_CENTER        /* 水平+垂直居中 */
ALIGN_LEFT          /* 左对齐 */
ALIGN_RIGHT         /* 右对齐 */
ALIGN_AUTO_BREAK    /* 自动换行 */
```

### 4.4 基本图形

```c
/* 直线 */
sc_draw_Line(NULL, x1, y1, x2, y2, C_WHITE);

/* 空心矩形边框，lw=线宽 */
sc_draw_Frame(NULL, x, y, w, h, lw, C_WHITE, 255);

/* 圆角矩形（r=外圆角半径，ir=内填充半径，0=无填充）*/
sc_rect_t box = {10, 10, 141, 40};
sc_draw_Rounded_rect(NULL, &box, 8, 6, C_ROYAL_BLUE, C_BLUE, 255);
```

### 4.5 进度条

```c
/* 进度条：r=圆角，ir=内圆角，vol=进度 0-100 */
sc_rect_t bar = {10, 60, 141, 16};
sc_draw_Bar(NULL, &bar, 7, 5, C_GREEN, C_DARK_GRAY, 255, vol);  /* vol: 0~100 */
```

### 4.6 圆弧（仪表盘指针）

```c
sc_arc_t arc = {
    .cx = 80,   /* 圆心 x */
    .cy = 65,   /* 圆心 y */
    .r  = 50,   /* 外半径 */
    .ir = 35,   /* 内半径（环形宽度 = r - ir）*/
    .dot = 0    /* 0=实心弧，1=点状弧 */
};
/* start_angle=30°, end_angle=330°（0°=右，顺时针）*/
sc_draw_Arc(NULL, &arc, 30, 330, C_GREEN, C_GREEN, C_BLACK, 255);
```

### 4.7 开关按钮

```c
sc_rect_t sw = {10, 50, 36, 20};
sc_draw_Switch(NULL, &sw, 10, 8, C_ROYAL_BLUE, C_GREEN, 255, state); /* state=0关/1开 */
```

### 4.8 Label（带对齐的标签）

```c
sc_label_t lbl;
sc_rect_t box = {0, 5, 161, 20};
sc_init_Label(&lbl, 0, 0, &lv_font_16, "RF Power Meter", ALIGN_CENTER);
sc_draw_Label(NULL, &box, &lbl, C_WHITE, C_BLACK);
```

### 4.9 Button（带圆角的按钮控件）

```c
sc_button_t btn;
sc_rect_t box = {20, 100, 120, 24};
sc_init_button(&btn, &lv_font_12, "Confirm", C_WHITE, ALIGN_CENTER);
sc_draw_button(NULL, &box, &btn, C_ROYAL_BLUE, C_BLUE, 6, 4, 255);
```

### 4.10 波形图

```c
sc_chart_t chart = {0};  /* 全局或静态 */

/* 写入新数据点（scaleX=水平缩放，1=每点1像素宽）*/
sc_put_Chart_ch(&chart, (int16_t)power_value, 1, C_GREEN);

/* 绘制（gc=网格颜色，gx/gy=网格步进像素，0=不画网格）*/
sc_draw_Chart(NULL, 5, 30, 151, 80, C_DARK_GRAY, 20, 20, &chart, 1);
```

---

## 五、任务系统（界面框架）

SCGUI 用轻量协作式任务驱动界面，每个界面对应一个**任务回调函数**。

### 5.1 任务结构

```c
void my_screen_task(sc_event_t *e)
{
    switch (e->type)
    {
        case SC_EVENT_TYPE_INIT:
            /* 首次进入时执行一次：清屏、画静态元素 */
            sc_clear(0, 0, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT, C_BLACK);
            /* 画标题 */
            sc_draw_Text(NULL, 40, 5, &lv_font_16, "RF Power Meter", C_WHITE, C_BLACK);
            break;

        case SC_EVENT_TYPE_TIMER:
            /* 定时刷新（每隔 ms 毫秒调一次）：只更新动态区域 */
            {
                char buf[16];
                sprintf(buf, "%6.1f W", g_power_result.forward_w);
                sc_draw_Text(NULL, 10, 40, &lv_font_16, buf, C_GREEN, C_BLACK);
            }
            break;

        case SC_EVENT_TYPE_KEY:
            /* 按键事件 */
            if (e->dat.key == CMD_ENTER)
            {
                /* 切换到菜单界面 */
                sc_create_task(0, menu_task, 50);
            }
            break;
    }
}
```

### 5.2 启动与切换

```c
/* 启动一个界面任务（id=0~3，ms=刷新间隔毫秒）*/
sc_create_task(0, my_screen_task, 100);  /* 100ms 刷新 */

/* 切换界面：直接用新回调覆盖同 id 的任务 */
sc_create_task(0, another_task, 50);

/* 主循环驱动 */
sc_task_loop(NULL);
```

### 5.3 多任务（例：数据处理 + 界面分离）

```c
sc_create_task(0, ui_main_task, 100);   /* id=0: 主界面，100ms */
sc_create_task(1, data_task, 50);       /* id=1: 数据采集，50ms */
```

---

## 六、局部刷新（进阶，减少闪烁）

不传 `NULL` 而是注册 `SC_EVENT_TYPE_DRAW` 回调，实现脏区局部刷新：

```c
/* 在 INIT 事件里注册绘制回调 */
case SC_EVENT_TYPE_INIT:
    sc_dirty_mark(NULL, &power_box);  /* 标记需要刷新的区域 */
    break;

/* 绘制事件，dest 是对应脏区的 PFB 切片 */
case SC_EVENT_TYPE_DRAW:
    {
        sc_pfb_t *dest = (sc_pfb_t *)e->dat.arg;
        sc_draw_Text(dest, 10, 40, &lv_font_16, buf, C_GREEN, C_BLACK);
    }
    break;
```

---

## 七、常用颜色速查

| 宏名 | 效果 |
|------|------|
| `C_BLACK` | 黑色背景 |
| `C_WHITE` | 白色文字 |
| `C_CYAN` | 青色标签 |
| `C_GREEN` | 绿色正常值 |
| `C_YELLOW` | 黄色警告 |
| `C_RED` | 红色错误/高值 |
| `C_ROYAL_BLUE` | 皇家蓝按钮底色 |
| `C_GRAY` | 灰色提示文字 |
| `C_DARK_GRAY` | 深灰分割线 |
| `C_ORANGE` | 橙色次要警告 |

---

## 八、整合现有系统的注意事项

1. **不要删除 `interface_manager.c`** - 保留作参考，新界面稳定后再移除旧代码。
2. **`system_tick` 已在 `sc_event_task.c` 中定义**，每次调用 `SC_Port_Tick()` 自动同步。
3. **旧的 `Show_Str` / `LCD_Fill` 等函数仍然可以继续用**，SCGUI 和原有绘图函数可以共存（但不能混用在同一界面里，否则会相互覆盖）。
4. **字体文件较大**（`lv_font_12.c` = 76KB）—— 确认 STM32F103 FLASH 够用（通常 256KB~512KB）。
5. **PFB 静态缓冲**：`4行 × 161px × 2B = 1288 字节`，消耗 SRAM。全部 SCGUI 运行时静态内存约 **3~4KB**。

---

*文档版本：2026-03-24*
