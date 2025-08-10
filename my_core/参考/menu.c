#include <stdio.h>  //用于printf
#include <stdarg.h> //用于vsprintf函数原型
#include <string.h>
#include <uart.h>
#include "stc8a.h"
#include "GPIO.h"
#include "ST7567A.h"
#include "intrins.h"
#include "delay.h"
#include "eeprom.h"
#include "dog.h"
#include "menu.h"
#include "control.h"
#include "MQTT.h"
#include "adc.h"
#include "oledfont.h"
#include "stdlib.h"
#include "control.h"
#include "ds1302.h"
#include "key.h"
#include "audio.h"
#include "interface.h"
extern u8 Save[255];
extern u16 runing_timer;

// 显示气体化学式
void display_name(u8 x, u8 y, u8 type, u8 size, u16 color) {

  switch (type) {
  case 0:
    Show_Str(x, y, color, BLACK, "可燃气  ", size, 0);
    break;
  case 1:
    Show_Str(x, y, color, BLACK, "氧气    ", size, 0);
    break;
  case 2:
    Show_Str(x, y, color, BLACK, "一氧化碳", size, 0);
    break;
  case 3:
    Show_Str(x, y, color, BLACK, "硫化氢  ", size, 0);
    break;
  default:
    break;
  }
}
// 显示气体单位
void dispaly_unit(u8 x, u8 y, u8 unit, u16 color) {
  switch (unit) {
  case 0:
    Show_Str(x, y, color, BLACK, "%LEL", 16, 0);
    break;
  case 1:
    Show_Str(x, y, color, BLACK, "%VOL", 16, 0);
    break;
  case 2:
    Show_Str(x, y, color, BLACK, "PPM", 16, 0);
    break;
  case 3:
    Show_Str(x, y, color, BLACK, "PPM", 16, 0);
    break;
  default:
    break;
  }
}

// 指定位置显示气体浓度  坐标 浓度 小数位 状态 类型----用于主界面
void disp_concentration_2(u8 x, u8 y, float con, u8 poin, u16 color,
                          u16 color2) {
  u8 i = 0, j = 0;
  char str[10] = "";
  int integer_part = (int)con;             // 1
  float decimal_part = con - integer_part; // 0.688
  float con_dec = 0.1;
  for (j = 0; j < poin; j++) {
    con_dec = con_dec / 10.0; // 补偿C语言浮点型数据最后一位自动削减特性
  }
  decimal_part = decimal_part + con_dec;

  if (poin == 0) {
    sprintf(str, "%d", integer_part); // 直接取整
  } else {
    // 手动拼接整数和小数部分
    sprintf(str, "%d.", integer_part);
    i = strlen(str);
    for (j = 0; j < poin; j++) {
      decimal_part *= 10;
      str[j + i] = '0' + (int)decimal_part;
      decimal_part -= (int)decimal_part;
    }
    str[i + poin] = '\0';
  }
  Show_Str(x, y, color, color2, str, 16, 0);
}

// 指定位置显示气体浓度  坐标 浓度 小数位 状态 类型----用于主界面
void disp_concentration(u8 x, u8 y, float con, u8 poin, u8 status, u8 type) {
  static u8 flag = 0, flag_count = 0;
  u8 i = 0, j = 0;
  char str[10] = "";
  int integer_part = (int)con;             // 1
  float decimal_part = con - integer_part; // 0.688
  float con_dec = 0.1;
  for (j = 0; j < poin; j++) {
    con_dec = con_dec / 10.0; // 补偿C语言浮点型数据最后一位自动削减特性
  }
  decimal_part = decimal_part + con_dec;
  decimal_part = decimal_part >= 1 ? 0.9 : decimal_part;
  if (poin == 0) {
    sprintf(str, "%5d", integer_part); // 直接取整
  } else {

    // 手动拼接整数和小数部分
    sprintf(str, "%5d.", integer_part);
    i = strlen(str);
    for (j = 0; j < poin; j++) {
      decimal_part *= 10;
      str[j + i] = '0' + (int)decimal_part;
      decimal_part -= (int)decimal_part;
    }
    str[i + poin] = '\0';
  }

  if (flag_count == 10) {
    flag_count = 0;
    flag = ~flag;
  }
  if (flag)
    switch (status) { // 异常状态显示闪烁
    case 1:
      Show_Str(x, y, GREEN, BLACK, "E1", 16, 0);
      display_name(x - 11, y - 17, type, 16, GREEN);

      dispaly_unit(x + 32, y + 19, type, GREEN);
      // Show_Str(x + 24, y - 16, RED, BLACK, "故障", 16, 0);
      break;
    case 2:
      Show_Str(x, y, GREEN, BLACK, str, 16, 0);
      display_name(x - 11, y - 17, type, 16, GREEN);

      dispaly_unit(x + 32, y + 19, type, GREEN);
      // Show_Str(x + 24, y - 18, RED, BLACK, "故障", 16, 0);
      break;
    case 3:
      Show_Str(x, y, GREEN, BLACK, str, 16, 0);
      display_name(x - 11, y - 17, type, 16, GREEN);

      dispaly_unit(x + 32, y + 19, type, GREEN);
      // Show_Str(x + 24, y - 16, RED, BLACK, "低报", 16, 0);
      break;
    case 4:
      Show_Str(x, y, GREEN, BLACK, str, 16, 0);
      display_name(x - 11, y - 17, type, 16, GREEN);

      dispaly_unit(x + 32, y + 19, type, GREEN);
      // Show_Str(x + 24, y - 16, RED, BLACK, "高报", 16, 0);
      break;
    case 5:
      Show_Str(x, y, GREEN, BLACK, "---    ", 16, 0);
      display_name(x - 11, y - 17, type, 16, GREEN);

      dispaly_unit(x + 32, y + 19, type, GREEN);
      // Show_Str(x + 16, y - 16, RED, BLACK, "超量程", 16, 0);
      break;
    default:
      break;
    }
  else
    switch (status) { // 异常状态显示闪烁
    case 1:
      Show_Str(x, y, YELLOW, BLACK, "E1", 16, 0);
      display_name(x - 11, y - 17, type, 16, YELLOW);

      dispaly_unit(x + 32, y + 19, type, YELLOW);
      // Show_Str(x + 24, y - 16, RED, BLACK, "故障", 16, 0);
      break;
    case 2:
      Show_Str(x, y, GREEN, BLACK, str, 16, 0);
      display_name(x - 11, y - 17, type, 16, GREEN);

      dispaly_unit(x + 32, y + 19, type, GREEN);
      // Show_Str(x + 24, y - 18, RED, BLACK, "故障", 16, 0);
      break;
    case 3:
      Show_Str(x, y, YELLOW, BLACK, str, 16, 0);
      display_name(x - 11, y - 17, type, 16, YELLOW);

      dispaly_unit(x + 32, y + 19, type, YELLOW);
      // Show_Str(x + 24, y - 16, RED, BLACK, "低报", 16, 0);
      break;
    case 4:
      Show_Str(x, y, RED, BLACK, str, 16, 0);
      display_name(x - 11, y - 17, type, 16, RED);

      dispaly_unit(x + 32, y + 19, type, RED);
      // Show_Str(x + 24, y - 16, RED, BLACK, "高报", 16, 0);
      break;
    case 5:
      Show_Str(x, y, RED, BLACK, "---    ", 16, 0);
      display_name(x - 11, y - 17, type, 16, RED);

      dispaly_unit(x + 32, y + 19, type, RED);
      // Show_Str(x + 16, y - 16, RED, BLACK, "超量程", 16, 0);
      break;
    default:
      break;
    }
  flag_count++;
  // 显示字符串
  // Show_Str(x, y, GREEN, BLACK, str, 16, 0);
}

// 绘制电量
void disp_power(u8 power) {
  char str_power[3];
  static u8 power_list = 0;
  sprintf(str_power, "%d", (int)power);
  if (power != power_list) {
    LCD_Fill(139, 5, 157, 16, BLACK);
  }
  LCD_Fill(134, 9, 136, 12, WHITE);
  LCD_DrawLine(137, 4, 158, 4);
  LCD_DrawLine(137, 17, 158, 17);
  LCD_DrawLine(137, 4, 137, 17);
  LCD_DrawLine(158, 4, 158, 17);
  // LCD_Fill(138+(18-18*power/100),5,157,16,GREEN);
  if (power == 100)
    Show_Str(139, 5, WHITE, BLACK, str_power, 12, 0);
  else if (power < 100 && power >= 10)
    Show_Str(142, 5, WHITE, BLACK, str_power, 12, 0);
  else
    Show_Str(146, 5, WHITE, BLACK, str_power, 12, 0);
  power_list = power;
}

/*******************************************************************
 * @name       :void GUI_DrawPoint(u16 x,u16 y,u16 color)
 * @date       :2018-08-09
 * @function   :draw a point in LCD screen
 * @parameters :x:the x coordinate of the point
                y:the y coordinate of the point
                                                                color:the color
value of the point
 * @retvalue   :None
********************************************************************/
// 写一个点
void GUI_DrawPoint(u16 x, u16 y, u16 color) {
  LCD_SetCursor(x, y); // 设置光标位置
  LCD_WR_DATA_16Bit(color);
}

/*******************************************************************
 * @name       :void LCD_Fill(u16 sx,u16 sy,u16 ex,u16 ey,u16 color)
 * @date       :2018-08-09
 * @function   :fill the specified area
 * @parameters :sx:the bebinning x coordinate of the specified area
                sy:the bebinning y coordinate of the specified area
                                                                ex:the ending x
coordinate of the specified area
                                                                ey:the ending y
coordinate of the specified area
                                                                color:the filled
color value
 * @retvalue   :None
********************************************************************/
void LCD_Fill(u16 sx, u16 sy, u16 ex, u16 ey, u16 color) {
  u16 i, j;
  u16 width = ex - sx + 1;        // 得到填充的宽度
  u16 height = ey - sy + 1;       // 高度
  LCD_SetWindows(sx, sy, ex, ey); // 设置显示窗口
  for (i = 0; i < height; i++) {
    for (j = 0; j < width; j++)
      LCD_WR_DATA_16Bit(color); // 写入数据
  }
  LCD_SetWindows(0, 0, lcddev.width - 1,
                 lcddev.height - 1); // 恢复窗口设置为全屏
}

/*******************************************************************
 * @name       :void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2)
 * @date       :2018-08-09
 * @function   :Draw a line between two points
 * @parameters :x1:the bebinning x coordinate of the line
                y1:the bebinning y coordinate of the line
                                                                x2:the ending x
coordinate of the line
                                                                y2:the ending y
coordinate of the line
 * @retvalue   :None
********************************************************************/
void LCD_DrawLine(u16 x1, u16 y1, u16 x2, u16 y2) {
  u16 t;
  int xerr = 0, yerr = 0, delta_x, delta_y, distance;
  int incx, incy, uRow, uCol;

  delta_x = x2 - x1; // 计算坐标增量
  delta_y = y2 - y1;
  uRow = x1;
  uCol = y1;
  if (delta_x > 0)
    incx = 1; // 设置单步方向
  else if (delta_x == 0)
    incx = 0; // 垂直线
  else {
    incx = -1;
    delta_x = -delta_x;
  }
  if (delta_y > 0)
    incy = 1;
  else if (delta_y == 0)
    incy = 0; // 水平线
  else {
    incy = -1;
    delta_y = -delta_y;
  }
  if (delta_x > delta_y)
    distance = delta_x; // 选取基本增量坐标轴
  else
    distance = delta_y;
  for (t = 0; t <= distance + 1; t++) // 画线输出
  {
    LCD_DrawPoint(uRow, uCol); // 画点
    xerr += delta_x;
    yerr += delta_y;
    if (xerr > distance) {
      xerr -= distance;
      uRow += incx;
    }
    if (yerr > distance) {
      yerr -= distance;
      uCol += incy;
    }
  }
}

/*****************************************************************************
 * @name       :void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2)
 * @date       :2018-08-09
 * @function   :Draw a rectangle
 * @parameters :x1:the bebinning x coordinate of the rectangle
                y1:the bebinning y coordinate of the rectangle
                                                                x2:the ending x
coordinate of the rectangle
                                                                y2:the ending y
coordinate of the rectangle
 * @retvalue   :None
******************************************************************************/
// void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2) {
//   LCD_DrawLine(x1, y1, x2, y1);
//   LCD_DrawLine(x1, y1, x1, y2);
//   LCD_DrawLine(x1, y2, x2, y2);
//   LCD_DrawLine(x2, y1, x2, y2);
// }

/*****************************************************************************
 * @name       :void LCD_DrawFillRectangle(u16 x1, u16 y1, u16 x2, u16 y2)
 * @date       :2018-08-09
 * @function   :Filled a rectangle
 * @parameters :x1:the bebinning x coordinate of the filled rectangle
                y1:the bebinning y coordinate of the filled rectangle
                                                                x2:the ending x
coordinate of the filled rectangle
                                                                y2:the ending y
coordinate of the filled rectangle
 * @retvalue   :None
******************************************************************************/
// void LCD_DrawFillRectangle(u16 x1, u16 y1, u16 x2, u16 y2) {
//   LCD_Fill(x1, y1, x2, y2, POINT_COLOR);
// }

/*****************************************************************************
 * @name       :void _draw_circle_8(int xc, int yc, int x, int y, u16 c)
 * @date       :2018-08-09
 * @function   :8 symmetry circle drawing algorithm (internal call)
 * @parameters :xc:the x coordinate of the Circular center
                yc:the y coordinate of the Circular center
                                                                x:the x
coordinate relative to the Circular center
                                                                y:the y
coordinate relative to the Circular center
                                                                c:the color
value of the circle
 * @retvalue   :None
******************************************************************************/
// void _draw_circle_8(int xc, int yc, int x, int y, u16 c) {
//   GUI_DrawPoint(xc + x, yc + y, c);

//   GUI_DrawPoint(xc - x, yc + y, c);

//   GUI_DrawPoint(xc + x, yc - y, c);

//   GUI_DrawPoint(xc - x, yc - y, c);

//   GUI_DrawPoint(xc + y, yc + x, c);

//   GUI_DrawPoint(xc - y, yc + x, c);

//   GUI_DrawPoint(xc + y, yc - x, c);

//   GUI_DrawPoint(xc - y, yc - x, c);
// }

/*****************************************************************************
 * @name       :void gui_circle(int xc, int yc,u16 c,int r, int fill)
 * @date       :2018-08-09
 * @function   :Draw a circle of specified size at a specified location
 * @parameters :xc:the x coordinate of the Circular center
                yc:the y coordinate of the Circular center
                                                                r:Circular
radius
                                                                fill:1-filling,0-no
filling
 * @retvalue   :None
******************************************************************************/
// void gui_circle(int xc, int yc, u16 c, int r, int fill) {
//   int x = 0, y = r, yi, d;

//   d = 3 - 2 * r;

//   if (fill) {
//     // 如果填充（画实心圆）
//     while (x <= y) {
//       for (yi = x; yi <= y; yi++)
//         _draw_circle_8(xc, yc, x, yi, c);

//       if (d < 0) {
//         d = d + 4 * x + 6;
//       } else {
//         d = d + 4 * (x - y) + 10;
//         y--;
//       }
//       x++;
//     }
//   } else {
//     // 如果不填充（画空心圆）
//     while (x <= y) {
//       _draw_circle_8(xc, yc, x, y, c);
//       if (d < 0) {
//         d = d + 4 * x + 6;
//       } else {
//         d = d + 4 * (x - y) + 10;
//         y--;
//       }
//       x++;
//     }
//   }
// }

/*****************************************************************************
 * @name       :void Draw_Triangel(u16 x0,u16 y0,u16 x1,u16 y1,u16 x2,u16 y2)
 * @date       :2018-08-09
 * @function   :Draw a triangle at a specified position
 * @parameters :x0:the bebinning x coordinate of the triangular edge
                y0:the bebinning y coordinate of the triangular edge
                                                                x1:the vertex x
coordinate of the triangular
                                                                y1:the vertex y
coordinate of the triangular
                                                                x2:the ending x
coordinate of the triangular edge
                                                                y2:the ending y
coordinate of the triangular edge
 * @retvalue   :None
******************************************************************************/
// void Draw_Triangel(u16 x0, u16 y0, u16 x1, u16 y1, u16 x2, u16 y2) {
//   LCD_DrawLine(x0, y0, x1, y1);
//   LCD_DrawLine(x1, y1, x2, y2);
//   LCD_DrawLine(x2, y2, x0, y0);
// }

// static void _swap(u16 *a, u16 *b) {
//   u16 tmp;
//   tmp = *a;
//   *a = *b;
//   *b = tmp;
// }

/*****************************************************************************
 * @name       :void Fill_Triangel(u16 x0,u16 y0,u16 x1,u16 y1,u16 x2,u16 y2)
 * @date       :2018-08-09
 * @function   :filling a triangle at a specified position
 * @parameters :x0:the bebinning x coordinate of the triangular edge
                y0:the bebinning y coordinate of the triangular edge
                                                                x1:the vertex x
coordinate of the triangular
                                                                y1:the vertex y
coordinate of the triangular
                                                                x2:the ending x
coordinate of the triangular edge
                                                                y2:the ending y
coordinate of the triangular edge
 * @retvalue   :None
******************************************************************************/
// void Fill_Triangel(u16 x0, u16 y0, u16 x1, u16 y1, u16 x2, u16 y2) {
//   u16 a, b, y, last;
//   int dx01, dy01, dx02, dy02, dx12, dy12;
//   long sa = 0;
//   long sb = 0;
//   if (y0 > y1) {
//     _swap(&y0, &y1);
//     _swap(&x0, &x1);
//   }
//   if (y1 > y2) {
//     _swap(&y2, &y1);
//     _swap(&x2, &x1);
//   }
//   if (y0 > y1) {
//     _swap(&y0, &y1);
//     _swap(&x0, &x1);
//   }
//   if (y0 == y2) {
//     a = b = x0;
//     if (x1 < a) {
//       a = x1;
//     } else if (x1 > b) {
//       b = x1;
//     }
//     if (x2 < a) {
//       a = x2;
//     } else if (x2 > b) {
//       b = x2;
//     }
//     LCD_Fill(a, y0, b, y0, POINT_COLOR);
//     return;
//   }
//   dx01 = x1 - x0;
//   dy01 = y1 - y0;
//   dx02 = x2 - x0;
//   dy02 = y2 - y0;
//   dx12 = x2 - x1;
//   dy12 = y2 - y1;

//   if (y1 == y2) {
//     last = y1;
//   } else {
//     last = y1 - 1;
//   }
//   for (y = y0; y <= last; y++) {
//     a = x0 + sa / dy01;
//     b = x0 + sb / dy02;
//     sa += dx01;
//     sb += dx02;
//     if (a > b) {
//       _swap(&a, &b);
//     }
//     LCD_Fill(a, y, b, y, POINT_COLOR);
//   }
//   sa = dx12 * (y - y1);
//   sb = dx02 * (y - y0);
//   for (; y <= y2; y++) {
//     a = x1 + sa / dy12;
//     b = x0 + sb / dy02;
//     sa += dx12;
//     sb += dx02;
//     if (a > b) {
//       _swap(&a, &b);
//     }
//     LCD_Fill(a, y, b, y, POINT_COLOR);
//   }
// }

/*****************************************************************************
 * @name       :void LCD_ShowChar(u16 x,u16 y,u16 fc, u16 bc, u8 num,u8 size,u8
mode)
 * @date       :2018-08-09
 * @function   :Display a single English character
 * @parameters :x:the bebinning x coordinate of the Character display position
                y:the bebinning y coordinate of the Character display position
                                                                fc:the color
value of display character
                                                                bc:the
background color of display character
                                                                num:the ascii
code of display character(0~94)
                                                                size:the size of
display character
                                                                mode:0-no
overlying,1-overlying
 * @retvalue   :None
******************************************************************************/
void LCD_ShowChar(u16 x, u16 y, u16 fc, u16 bc, u8 num, u8 size, u8 mode) {
  u8 temp;
  u8 pos, t;
  u16 colortemp = POINT_COLOR;

  num = num - ' ';                                      // 得到偏移后的值
  LCD_SetWindows(x, y, x + size / 2 - 1, y + size - 1); // 设置单个文字显示窗口
  if (!mode)                                            // 非叠加方式
  {

    for (pos = 0; pos < size; pos++) {
      if (size == 12)
        temp = asc2_1206[num][pos]; // 调用1206字体
      else
        temp = asc2_1608[num][pos]; // 调用1608字体
      for (t = 0; t < size / 2; t++) {
        if (temp & 0x01)
          LCD_WR_DATA_16Bit(fc);
        else
          LCD_WR_DATA_16Bit(bc);
        temp >>= 1;
      }
    }
  } else // 叠加方式
  {
    for (pos = 0; pos < size; pos++) {
      if (size == 12)
        temp = asc2_1206[num][pos]; // 调用1206字体
      else
        temp = asc2_1608[num][pos]; // 调用1608字体
      for (t = 0; t < size / 2; t++) {
        POINT_COLOR = fc;
        if (temp & 0x01)
          LCD_DrawPoint(x + t, y + pos); // 画一个点
        temp >>= 1;
      }
    }
  }
  POINT_COLOR = colortemp;
  LCD_SetWindows(0, 0, lcddev.width - 1, lcddev.height - 1); // 恢复窗口为全屏
}

/*****************************************************************************
 * @name       :void LCD_ShowString(u16 x,u16 y,u8 size,u8 *p,u8 mode)
 * @date       :2018-08-09
 * @function   :Display English string
 * @parameters :x:the bebinning x coordinate of the English string
                y:the bebinning y coordinate of the English string
                                                                p:the start
address of the English string
                                                                size:the size of
display character
                                                                mode:0-no
overlying,1-overlying
 * @retvalue   :None
******************************************************************************/
void LCD_ShowString(u16 x, u16 y, u8 size, u8 *p, u8 mode) {
  while ((*p <= '~') && (*p >= ' ')) // 判断是不是非法字符!
  {
    if (x > (lcddev.width - 1) || y > (lcddev.height - 1))
      return;
    LCD_ShowChar(x, y, POINT_COLOR, BACK_COLOR, *p, size, mode);
    x += size / 2;
    p++;
  }
}

/*****************************************************************************
 * @name       :u32 mypow(u8 m,u8 n)
 * @date       :2018-08-09
 * @function   :get the nth power of m (internal call)
 * @parameters :m:the multiplier
                n:the power
 * @retvalue   :the nth power of m
******************************************************************************/
u32 mypow(u8 m, u8 n) {
  u32 result = 1;
  while (n--)
    result *= m;
  return result;
}

/*****************************************************************************
 * @name       :void LCD_ShowNum(u16 x,u16 y,u32 num,u8 len,u8 size)
 * @date       :2018-08-09
 * @function   :Display number
 * @parameters :x:the bebinning x coordinate of the number
                y:the bebinning y coordinate of the number
                                                                num:the
number(0~4294967295)
                                                                len:the length
of the display number
                                                                size:the size of
display number
 * @retvalue   :None
******************************************************************************/
void LCD_ShowNum(u16 x, u16 y, u32 num, u8 len, u8 size) {
  u8 t, temp;
  u8 enshow = 0;
  for (t = 0; t < len; t++) {
    temp = (num / mypow(10, len - t - 1)) % 10;
    if (enshow == 0 && t < (len - 1)) {
      if (temp == 0) {
        LCD_ShowChar(x + (size / 2) * t, y, POINT_COLOR, BACK_COLOR, ' ', size,
                     0);
        continue;
      } else
        enshow = 1;
    }
    LCD_ShowChar(x + (size / 2) * t, y, POINT_COLOR, BACK_COLOR, temp + '0',
                 size, 0);
  }
}

/*****************************************************************************
 * @name       :void GUI_DrawFont16(u16 x, u16 y, u16 fc, u16 bc, u8 *s,u8 mode)
 * @date       :2018-08-09
 * @function   :Display a single 16x16 Chinese character
 * @parameters :x:the bebinning x coordinate of the Chinese character
                y:the bebinning y coordinate of the Chinese character
                                                                fc:the color
value of Chinese character
                                                                bc:the
background color of Chinese character
                                                                s:the start
address of the Chinese character
                                                                mode:0-no
overlying,1-overlying
 * @retvalue   :None
******************************************************************************/
void GUI_DrawFont16(u16 x, u16 y, u16 fc, u16 bc, u8 *s, u8 mode) {
  u8 i, j;
  u16 k;
  u16 HZnum;
  u16 x0 = x;

  HZnum = givesize_GB16(); // 自动统计汉字数目

  for (k = 0; k < HZnum; k++) {
    if ((tfont16[k].Index[0] == *(s)) && (tfont16[k].Index[1] == *(s + 1))) {
      LCD_SetWindows(x, y, x + 16 - 1, y + 16 - 1);
      for (i = 0; i < 16 * 2; i++) {
        for (j = 0; j < 8; j++) {
          if (!mode) // 非叠加方式
          {
            if (tfont16[k].Msk[i] & (0x80 >> j))
              LCD_WR_DATA_16Bit(fc);
            else
              LCD_WR_DATA_16Bit(bc);
          } else {
            POINT_COLOR = fc;
            if (tfont16[k].Msk[i] & (0x80 >> j))
              LCD_DrawPoint(x, y); // 画一个点
            x++;
            if ((x - x0) == 16) {
              x = x0;
              y++;
              break;
            }
          }
        }
      }
    }
    continue; // 查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
  }

  LCD_SetWindows(0, 0, lcddev.width - 1, lcddev.height - 1); // 恢复窗口为全屏
}

/*****************************************************************************
 * @name       :void GUI_DrawFont24(u16 x, u16 y, u16 fc, u16 bc, u8 *s,u8 mode)
 * @date       :2018-08-09
 * @function   :Display a single 24x24 Chinese character
 * @parameters :x:the bebinning x coordinate of the Chinese character
                y:the bebinning y coordinate of the Chinese character
                                                                fc:the color
value of Chinese character
                                                                bc:the
background color of Chinese character
                                                                s:the start
address of the Chinese character
                                                                mode:0-no
overlying,1-overlying
 * @retvalue   :None
******************************************************************************/
void GUI_DrawFont24(u16 x, u16 y, u16 fc, u16 bc, u8 *s, u8 mode) {
  u8 i, j;
  u16 k;
  u16 HZnum;
  u16 x0 = x;
  HZnum = givesize_GB24(); // 自动统计汉字数目

  for (k = 0; k < HZnum; k++) {
    if ((tfont24[k].Index[0] == *(s)) && (tfont24[k].Index[1] == *(s + 1))) {
      LCD_SetWindows(x, y, x + 24 - 1, y + 24 - 1);
      for (i = 0; i < 24 * 3; i++) {
        for (j = 0; j < 8; j++) {
          if (!mode) // 非叠加方式
          {
            if (tfont24[k].Msk[i] & (0x80 >> j))
              LCD_WR_DATA_16Bit(fc);
            else
              LCD_WR_DATA_16Bit(bc);
          } else {
            POINT_COLOR = fc;
            if (tfont24[k].Msk[i] & (0x80 >> j))
              LCD_DrawPoint(x, y); // 画一个点
            x++;
            if ((x - x0) == 24) {
              x = x0;
              y++;
              break;
            }
          }
        }
      }
    }
    continue; // 查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
  }

  LCD_SetWindows(0, 0, lcddev.width - 1, lcddev.height - 1); // 恢复窗口为全屏
}

/*****************************************************************************
 * @name       :void GUI_DrawFont32(u16 x, u16 y, u16 fc, u16 bc, u8 *s,u8 mode)
 * @date       :2018-08-09
 * @function   :Display a single 32x32 Chinese character
 * @parameters :x:the bebinning x coordinate of the Chinese character
                y:the bebinning y coordinate of the Chinese character
                                                                fc:the color
value of Chinese character
                                                                bc:the
background color of Chinese character
                                                                s:the start
address of the Chinese character
                                                                mode:0-no
overlying,1-overlying
 * @retvalue   :None
******************************************************************************/
void GUI_DrawFont32(u16 x, u16 y, u16 fc, u16 bc, u8 *s, u8 mode) {
  u8 i, j;
  u16 k;
  u16 HZnum;
  u16 x0 = x;
  HZnum = givesize_GB32(); // 自动统计汉字数目
  for (k = 0; k < HZnum; k++) {
    if ((tfont32[k].Index[0] == *(s)) && (tfont32[k].Index[1] == *(s + 1))) {
      LCD_SetWindows(x, y, x + 32 - 1, y + 32 - 1);
      for (i = 0; i < 32 * 4; i++) {
        for (j = 0; j < 8; j++) {
          if (!mode) // 非叠加方式
          {
            if (tfont32[k].Msk[i] & (0x80 >> j))
              LCD_WR_DATA_16Bit(fc);
            else
              LCD_WR_DATA_16Bit(bc);
          } else {
            POINT_COLOR = fc;
            if (tfont32[k].Msk[i] & (0x80 >> j))
              LCD_DrawPoint(x, y); // 画一个点
            x++;
            if ((x - x0) == 32) {
              x = x0;
              y++;
              break;
            }
          }
        }
      }
    }
    continue; // 查找到对应点阵字库立即退出，防止多个汉字重复取模带来影响
  }

  LCD_SetWindows(0, 0, lcddev.width - 1, lcddev.height - 1); // 恢复窗口为全屏
}

/*****************************************************************************
 * @name       :void Show_Str(u16 x, u16 y, u16 fc, u16 bc, u8 *str,u8 size,u8
mode)
 * @date       :2018-08-09
 * @function   :Display Chinese and English strings
 * @parameters :x:the bebinning x coordinate of the Chinese and English strings
                y:the bebinning y coordinate of the Chinese and English strings
                                                                fc:the color
value of Chinese and English strings
                                                                bc:the
background color of Chinese and English strings
                                                                str:the start
address of the Chinese and English strings
                                                                size:the size of
Chinese and English strings
                                                                mode:0-no
overlying,1-overlying
 * @retvalue   :None
******************************************************************************/
void Show_Str(u16 x, u16 y, u16 fc, u16 bc, u8 *str, u8 size, u8 mode) {
  u16 x0 = x;
  u8 bHz = 0;       // 字符或者中文
  while (*str != 0) // 数据未结束
  {
    if (!bHz) {
      if (x > (lcddev.width - size / 2) || y > (lcddev.height - size))
        return;
      if (*str > 0x80)
        bHz = 1; // 中文
      else       // 字符
      {
        if (*str == 0x0D) // 换行符号
        {
          y += size;
          x = x0;
          str++;
        } else {
          if (size > 16) // 字库中没有集成12X24 16X32的英文字体,用8X16代替
          {
            LCD_ShowChar(x, y, fc, bc, *str, 16, mode);
            x += 8; // 字符,为全字的一半
          } else {
            LCD_ShowChar(x, y, fc, bc, *str, size, mode);
            x += size / 2; // 字符,为全字的一半
          }
        }
        str++;
      }
    } else // 中文
    {
      if (x > (lcddev.width - size) || y > (lcddev.height - size))
        return;
      bHz = 0; // 有汉字库
      if (size == 32)
        GUI_DrawFont32(x, y, fc, bc, str, mode);
      else if (size == 24)
        GUI_DrawFont24(x, y, fc, bc, str, mode);
      else
        GUI_DrawFont16(x, y, fc, bc, str, mode);

      str += 2;
      x += size; // 下一个汉字偏移
    }
  }
}

/*****************************************************************************
 * @name       :void Gui_StrCenter(u16 x, u16 y, u16 fc, u16 bc, u8 *str,u8
size,u8 mode)
 * @date       :2018-08-09
 * @function   :Centered display of English and Chinese strings
 * @parameters :x:the bebinning x coordinate of the Chinese and English strings
                y:the bebinning y coordinate of the Chinese and English strings
                                                                fc:the color
value of Chinese and English strings
                                                                bc:the
background color of Chinese and English strings
                                                                str:the start
address of the Chinese and English strings
                                                                size:the size of
Chinese and English strings
                                                                mode:0-no
overlying,1-overlying
 * @retvalue   :None
******************************************************************************/
// void Gui_StrCenter(u16 x, u16 y, u16 fc, u16 bc, u8 *str, u8 size, u8 mode) {
//   u16 len = strlen((const char *)str);
//   u16 x1 = (lcddev.width - len * 8) / 2;
//   Show_Str(x + x1, y, fc, bc, str, size, mode);
// }

/*****************************************************************************
 * @name       :void Gui_Drawbmp16(u16 x,u16 y,const unsigned char *p)
 * @date       :2018-08-09
 * @function   :Display a 16-bit BMP image
 * @parameters :x:the bebinning x coordinate of the BMP image
                y:the bebinning y coordinate of the BMP image
                                                                p:the start
address of image array
 * @retvalue   :None
******************************************************************************/
void Gui_Drawbmp16(u16 x, u16 y, unsigned char *p) // 显示40*40 图片
{
  int i;
  unsigned char picH, picL;
  LCD_SetWindows(x, y, x + 40 - 1, y + 40 - 1); // 窗口设置
  for (i = 0; i < 40 * 40; i++) {
    picL = *(p + i * 2); // 数据低位在前
    picH = *(p + i * 2 + 1);
    LCD_WR_DATA_16Bit(picH << 8 | picL);
  }
  LCD_SetWindows(0, 0, lcddev.width - 1,
                 lcddev.height - 1); // 恢复显示窗口为全屏
}