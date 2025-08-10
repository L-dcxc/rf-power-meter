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
#include "MQTT.h"
#include "adc.h"
#include "oledfont.h"
#include "stdlib.h"
#include "control.h"
#include "ds1302.h"
#include "key.h"
#include "audio.h"
#include "interface.h"
#include "interface_E.h"
#include "pwm.h"
#include "AHT30.h"
extern u8 Save[255];
extern u16 runing_timer;
extern u8 ds1302_time_set[6];
extern u8 count_100ms;
extern u8 buttons[3];
extern u8 longPressFlags[3];
extern float concentration_display[4]; // 气体显示浓度
extern float concentration[4];         // 气体真实浓度
extern u8 detector_buffer[4];          //气体状态
extern AHT30_Member AHT30;
extern u8 AHT30_flag;
extern float sensor_adc[5];
extern u8 sleep_Timing;
extern u8 key_en;
// extern u16 adc_zero[4];
u8 count_100ms_en = 0;
char aht_str[10]; //存储温湿度显示内容
u8 audio_busy_flag = 0;
Menu table[17] = {
    {0, 0, 0, main_interface},     // 主界面
    {1, 0, 0, menu_choose},        // 菜单界面
    {2, 0, 0, system_information}, // 系统信息
    {3, 0, 0, shutdowm},           // 关机
    {4, 0, 0, disp_set_time},      // 时间设置
    {5, 0, 0, set_Language},       // 语言设置
    {6, 0, 0, scan_password},      // 密码界面
    {7, 0, 0, balck_system},       //恢复出厂
    {8, 0, 0, zero_set},           // 零点
    {9, 0, 0, Calibration_set},    // 标定
    {10, 0, 0, Alarm_set},         //警报设置
    {11, 0, 0, Alarm_look},        //警报查看
    {12, 0, 0, choose_Gas},        //气体选择
    {13, 0, 0, backlight_set},     //背光调节
    {14, 0, 0, sleep_set},         //休眠设置
    {15, 0, 0, alarm_log_clear},   //报警记录清除
    {16, 0, 0, Mute_mode}          //静音

};

void (*current_operation_index)(void); // 声明函数
u8 func_index = 0;
u8 sleep_flag = 0;
void Menu_desplay(void) {
  if (Save[80]) // 界面切换中英文
    current_operation_index =
        table_E[func_index].current_operation; // 按索引赋值
  else
    current_operation_index = table[func_index].current_operation; // 按索引赋值
  (*current_operation_index)(); // 执行显示函数
  //判断超时：标志位；
  if (sleep_Timing > Save[19]) {
    key_en = 0;
    HPWM_Set(1900, 1);
    // if (sleep_Timing > (Save[19]+2))//延迟退休
    func_index = 0;
  }
  // } else
  // {
  //   HPWM_Set((int)(Save[17] << 8 | Save[18]), 1);
  // }
}
void main_interface() // 主界面显示
{
  float voltage = 0;
  u8 num = 0;
  if (AHT30_flag) {
    //温湿度
    AHT30_flag = 0; //复位采集标志
    sprintf(aht_str, "T:%d H:%d", (int)AHT30.Temp,
            (int)AHT30.Humi); // 在此处理温湿度数据（temp为温度，humi为湿度）
  }

  disp_nowtime();          //显示时间
  disp_power(BAT_cap_num); //显示电量
  //显示温湿度
  Show_Str(70, 3, WHITE, BLACK, aht_str, 12, 0);

  LCD_DrawLine(0, 20, 160, 20); // 上隔线

  LCD_DrawLine(0, 74, 160, 74); // 十字路口分割线
  LCD_DrawLine(80, 20, 80, 129);

  // 显示浓度及状态
  disp_concentration(14, 38, concentration_display[0], Save[9],
                     detector_buffer[0], Save[14]); // x,y,con,poin,stuas,type
  disp_concentration(94, 38, concentration_display[1], Save[29],
                     detector_buffer[1], Save[34]);
  disp_concentration(14, 92, concentration_display[2], Save[49],
                     detector_buffer[2], Save[54]);
  disp_concentration(94, 92, concentration_display[3], Save[69],
                     detector_buffer[3], Save[74]);

  switch (key_get_long_press()) {
  case 4:
    func_index = 3;
    LCD_Clear(BLACK);
    break;
  case 3:
    func_index = 16;
    LCD_Clear(BLACK);
    break;
  default:
    break;
  }
  switch (key_out()) {
  case 2:
    LCD_Clear(BLACK);
    func_index = 2;
    break;
  case 1:
    LCD_Clear(BLACK);
    func_index = 1;
    break;
  default:
    break;
  }
}

void Action() {
  u8 load = 0; // 进度
  u8 i = 0;
  u8 load_Str[4] = "";
  float con_alarm = 0;
  // 按键长按3S才可激活开机界面
  // food();
  // Delay_ms(500);
  // 启动中
  audio_sned(0);
  // pwm_out_end(Save[18] << 8 | Save[17]);
  if (Save[80])
    Show_Str(3, 3, WHITE, BLACK, "Starting...", 16, 0);
  else
    Show_Str(0, 3, WHITE, BLACK, "启动中", 16, 0);
  LCD_DrawLine(30, 72, 130, 72);
  LCD_DrawLine(30, 86, 130, 86);
  LCD_DrawLine(30, 72, 30, 86);
  LCD_DrawLine(130, 72, 130, 86);
  for (i = 0; i < 101; i++) {
    sprintf(load_Str, "%d%%", (int)load + 2);
    Show_Str(64, 56, WHITE, BLACK, load_Str, 16, 0);
    Delay_ms(20);
    food();
    load += 1;
    load = (load == 100) ? 98 : load;
    LCD_Fill(31, 73, 31 + load, 86, WHITE);
  }

  // //锁电
  // P17 = 1;

  // 显示公司信息--图片加name
  // 图标
  LCD_Clear(WHITE);
  Gui_Drawbmp16(60, 25, gImage_pic2);
  LCD_Fill(60, 44, 100, 65, WHITE);
  // 堃联技术
  Show_Str(16, 70, BLACK, WHITE, "广州堃联", 32, 0);
  for (i = 0; i < 5; i++) {
    Delay_ms(250);
    food();
  }

  LCD_Clear(BLACK);
  // 传感器信息
  for (i = 0; i < 4; i++) {
    if (Save[80]) {
      // 名字
      display_name_E(3, 3, i, 16, WHITE);
      Show_Str(3, 36, WHITE, BLACK, "Range:", 16, 0);
      Show_Str(3, 69, WHITE, BLACK, "Alarm_L:", 16, 0);
      Show_Str(3, 102, WHITE, BLACK, "Alarm_H:", 16, 0);
    } else {
      // 名字
      display_name(3, 3, i, 16, WHITE);
      Show_Str(3, 36, WHITE, BLACK, "量程:", 16, 0);
      Show_Str(3, 69, WHITE, BLACK, "低报:", 16, 0);
      Show_Str(3, 102, WHITE, BLACK, "高报:", 16, 0);
    }
    // 单位
    dispaly_unit(126, 40, i, WHITE);
    dispaly_unit(126, 73, i, WHITE);
    dispaly_unit(126, 105, i, WHITE);
    // 数值
    if (Save[9 + 20 * i] == 0) {
      disp_concentration_2(86, 40, Save[10 + 20 * i] << 8 | Save[11 + 20 * i],
                           0, WHITE, BLACK);
      disp_concentration_2(86, 73, Save[5 + 20 * i] << 8 | Save[6 + 20 * i], 0,
                           WHITE, BLACK);
      disp_concentration_2(86, 105, Save[7 + 20 * i] << 8 | Save[8 + 20 * i], 0,
                           WHITE, BLACK);
    } else {
      disp_concentration_2(86, 40, Save[10 + 20 * i] << 8 | Save[11 + 20 * i],
                           0, WHITE, BLACK);
      con_alarm =
          (Save[5 + 20 * i] << 8 | Save[6 + 20 * i]) + Save[12 + 20 * i] / 10.0;
      disp_concentration_2(86, 73, con_alarm, 1, WHITE, BLACK);
      con_alarm =
          (Save[7 + 20 * i] << 8 | Save[8 + 20 * i]) + Save[13 + 20 * i] / 10.0;
      disp_concentration_2(86, 105, con_alarm, 1, WHITE, BLACK);
    }
    Delay_ms(500);
    food();
    Delay_ms(500);
    food();
    Delay_ms(150);
    food();
    LCD_Clear(BLACK);
  }
  // 自检
  // audio_sned(1);
  // 灯光自检
  if (Save[80])
    Show_Str(10, 51, WHITE, BLACK, "Lighting check", 16, 0);
  else
    Show_Str(32, 51, WHITE, BLACK, "灯光自检", 24, 0);
  // 点灯闪烁,红绿蓝
  red = 0;
  green = 1;
  blue = 1;
  food();
  Delay_ms(600);
  red = 1;
  green = 0;
  blue = 1;
  food();
  Delay_ms(600);
  red = 1;
  green = 1;
  blue = 0;
  food();
  Delay_ms(600);
  red = 1;
  green = 1;
  blue = 1;
  LCD_Clear(BLACK);
  // 振动自检
  if (Save[80])
    Show_Str(10, 51, WHITE, BLACK, "Vibration check", 24, 0);
  else
    Show_Str(32, 51, WHITE, BLACK, "振动自检", 24, 0);
  // 振动
  food();
  Delay_ms(500);
  vibration = 1;
  food();
  Delay_ms(500);
  vibration = 0;
  food();
  Delay_ms(500);
  vibration = 1;
  food();
  Delay_ms(500);
  vibration = 0;
  food();
  LCD_Clear(BLACK);
  // 声音自检
  if (Save[80])
    Show_Str(10, 51, WHITE, BLACK, "Voice check", 24, 0);
  else
    Show_Str(32, 51, WHITE, BLACK, "声音自检", 24, 0);
  // 嘀嘀响
  audio_sned(2);
  food();
  Delay_ms(250);
  audio_sned(0xfe);
  food();
  Delay_ms(150);
  food();
  audio_sned(2);
  Delay_ms(250);
  food();
  audio_sned(0xfe);
  Delay_ms(250);
  food();
  LCD_Clear(BLACK);

  // 数据初始化
  for (i = 0; i < 3; i++) {
    buttons[i] = 0;
    longPressFlags[i] = 0;
  }
  sleep_Timing = 0;
  count_100ms_en = 1;
  // 进入主界面
}
void disp_nowtime() {
  u8 str_time[8] = "";
  // LCD_Fill(0,0,);
  sprintf(str_time, "%02d:%02d:%02d", (int)now_time.hour, (int)now_time.minute,
          (int)now_time.second);
  Show_Str(3, 3, WHITE, BLACK, str_time, 12, 0);
}

// 绘制菜单选项
void menu_choose() {
  static u8 flag = 0;
  static u8 choose = 0;
  Show_Str(64, 4, BLACK, WHITE, "菜单", 16, 0);
  // page1
  if (choose <= 4) {
    Show_Str(48, 25, WHITE, BLACK, "报警记录", 16, 0);
    Show_Str(48, 46, WHITE, BLACK, "报警设置", 16, 0);
    Show_Str(48, 67, WHITE, BLACK, "标定设置", 16, 0);
    Show_Str(48, 88, WHITE, BLACK, "时间设置", 16, 0);
    Show_Str(48, 109, WHITE, BLACK, "标零设置", 16, 0);
    // Show_Str(48, 112, WHITE, BLACK, "语言设置", 16, 0);
    Show_Str(32, 25 + choose * 21, YELLOW, BLACK, "→", 16, 0);
    Show_Str(112, 25 + choose * 21, YELLOW, BLACK, "琨", 16, 0);
    flag = 0;
  }

  else if (choose > 4) {
    // page2
    if (flag == 0) {
      LCD_Clear(BLACK);
      Show_Str(64, 4, BLACK, WHITE, "菜单", 16, 0);
      flag = 1;
    }
    Show_Str(48, 25, WHITE, BLACK, "语言设置", 16, 0);
    Show_Str(48, 46, WHITE, BLACK, "恢复出厂", 16, 0);
    Show_Str(48, 67, WHITE, BLACK, "背光调节", 16, 0);
    Show_Str(48, 88, WHITE, BLACK, "休眠设置", 16, 0);
    Show_Str(64, 109, WHITE, BLACK, "退出", 16, 0);
    Show_Str(32, 25 + (choose - 5) * 21, YELLOW, BLACK, "→", 16, 0);
    Show_Str(112, 25 + (choose - 5) * 21, YELLOW, BLACK, "琨", 16, 0);
  }

  // 按键改变choose-待写
  //  choose++;
  switch (key_out()) {
  case 2:
    choose = --choose > 126 ? 9 : choose;
    LCD_Fill(32, 16, 47, 128, BLACK);   //白色清屏
    LCD_Fill(112, 16, 128, 128, BLACK); //白色清屏
    break;
  case 0:
    choose = ++choose > 9 ? 0 : choose;
    LCD_Fill(32, 16, 47, 128, BLACK);   //白色清屏
    LCD_Fill(112, 16, 128, 128, BLACK); //白色清屏
    break;
  case 1:
    LCD_Clear(BLACK);
    switch (choose) {
    case 3: //时间
      func_index = 4;
      choose = 0;
      flag = 0;
      break;
    case 7: //时间
      func_index = 13;
      choose = 0;
      flag = 0;
      break;
    case 5: //语言
      func_index = 6;
      table[func_index].ret = 5;
      choose = 0;
      flag = 0;
      break;
    case 6: //恢复出厂
      func_index = 6;
      table[func_index].ret = 7;
      choose = 0;
      flag = 0;
      break;
    case 4: //零点
      func_index = 6;
      table[func_index].ret = 8;
      choose = 0;
      flag = 0;
      break;
    case 2: //标定
      func_index = 6;
      table[func_index].ret = 12;
      table[func_index].enter = 9;
      choose = 0;
      flag = 0;
      break;
    case 1: //警报设置
      func_index = 6;
      table[func_index].ret = 12;
      table[func_index].enter = 10;
      choose = 0;
      flag = 0;
      break;
    case 0: //警报查看
      func_index = 11;
      // table[func_index].ret = 12;
      // table[func_index].enter = 11;
      choose = 0;
      flag = 0;
      break;
    case 8: //休眠
      func_index = 14;
      choose = 0;
      flag = 0;
      break;
    default: //退出
      func_index = 0;
      choose = 0;
      flag = 0;
      break;
    }
    break;
  default:
    break;
  }
  if (sleep_Timing > Save[19]) {
    HPWM_Set(1900, 1);
    LCD_Clear(BLACK);
    choose = 0;
    flag = 0;
    return;
  }

  if (KEY_DOWN == 0 && KEY_UP == 0) {
    Delay_ms(10);
    if (KEY_DOWN == 0 && KEY_UP == 0) {
      Delay_ms(300);
      buttons[0] = 0;
      buttons[2] = 0;
      LCD_Clear(BLACK);
      func_index = 0;
      choose = 0;
      flag = 0;
    }
  }
}

void system_information() {
  u8 str_time[10] = "";
  if (Save[80])
    Show_Str(3, 12, WHITE, BLACK, "Date: ", 16, 0);
  else
    Show_Str(0, 8, WHITE, BLACK, "日期: ", 24, 0);
  sprintf(str_time, "%02d-%02d-%02d", (int)now_time.year, (int)now_time.month,
          (int)now_time.day);
  Show_Str(64, 12, WHITE, BLACK, str_time, 16, 0);
  if (Save[80])
    Show_Str(3, 44, WHITE, BLACK, "Time: ", 16, 0);
  else
    Show_Str(0, 40, WHITE, BLACK, "时间: ", 24, 0);
  sprintf(str_time, "%02d:%02d:%02d", (int)now_time.hour, (int)now_time.minute,
          (int)now_time.second);
  Show_Str(64, 44, WHITE, BLACK, str_time, 16, 0);
  if (Save[80]) {
    Show_Str(3, 76, WHITE, BLACK, "Version: ", 16, 0);
    Show_Str(84, 76, WHITE, BLACK, "KL_3.2.1", 16, 0);
  } else {
    Show_Str(0, 72, WHITE, BLACK, "版本: ", 24, 0);
    Show_Str(64, 76, WHITE, BLACK, "KL_3.2.1", 16, 0); //实际修改
  }

  if (Save[80])
    Show_Str(3, 108, WHITE, BLACK, "Running Time:", 16, 0);
  else
    Show_Str(0, 104, WHITE, BLACK, "运行时间:", 24, 0);
  LCD_ShowNum(107, 108, runing_timer, 3, 16);
  // Show_Str(107,108,WHITE,BLACK,"480",16,0);
  Show_Str(133, 108, WHITE, BLACK, "min", 16, 0);

  switch (key_out()) {
  case 0:
  case 1:
  case 2:
    LCD_Clear(BLACK);
    func_index = 0;
    break;
  default:
    break;
  }
  if (sleep_Timing > Save[19]) {
    LCD_Clear(BLACK);
    return;
  }
}

void shutdowm() {
  if (Save[80]) {
    Show_Str(48, 56, WHITE, BLACK, "Shutdown?", 16, 0);
    Show_Str(3, 112, WHITE, BLACK, "Yes", 16, 0);
    Show_Str(128, 112, WHITE, BLACK, "Exit", 16, 0);
  } else {
    Show_Str(48, 56, WHITE, BLACK, "是否关机?", 16, 0);
    Show_Str(0, 112, WHITE, BLACK, "关机", 16, 0);
    Show_Str(128, 112, WHITE, BLACK, "退出", 16, 0);
  }
  switch (key_out()) {
  case 2:
    P17 = 0;
    IAP_CONTR |= 0x60; // 软件重启
    break;
  case 0:
    LCD_Clear(BLACK);
    func_index = 0;
    break;
  default:
    break;
  }
  if (sleep_Timing > Save[19]) {
    LCD_Clear(BLACK);
    return;
  }
}

void disp_set_time() {
  static u8 dowmflag = 0; // 当前设置位
  static int count[12];
  static u8 flag = 0;
  static u16 flash_timer = 0; // 闪烁计时器
  static u8 flash_on = 1;     // 闪烁状态
  char str_t[15] = "";
  u8 pos_map_date[] = {2, 3, 6, 7, 10, 11}; // 各数字在字符串中的位置
  u8 pos_map_time[] = {0, 1, 3, 4, 6, 7};   // 各数字在字符串中的位置
  u8 i = 0;
  u8 j = 0;
  // 闪烁控制（500ms周期）
  if (++flash_timer >= 6) {
    flash_timer = 0;
    flash_on = !flash_on;
  }

  if (flag == 0) { // 第一次进入此界面
    flag = 1;
    flash_on = 1; // 重置闪烁状态
    ds1302_time_set[0] = now_time.year;
    ds1302_time_set[1] = now_time.month;
    ds1302_time_set[2] = now_time.day;
    ds1302_time_set[3] = now_time.hour;
    ds1302_time_set[4] = now_time.minute;
    ds1302_time_set[5] = now_time.second;
    for (i = 0; i < 6; i++) {
      count[0 + i * 2] = ds1302_time_set[i] / 10;
      count[1 + i * 2] = ds1302_time_set[i] % 10;
    }
    Show_Str(48, 3, YELLOW, BLACK, "时间设置", 16, 0);
    Show_Str(1, 112, YELLOW, BLACK, "加", 16, 0);
    Show_Str(141, 112, YELLOW, BLACK, "减", 16, 0);
    Show_Str(64, 112, YELLOW, BLACK, "确认", 16, 0);
  }

  /* 日期显示处理 */
  sprintf(str_t, "20%d%d年%d%d月%d%d日", count[0], count[1], count[2], count[3],
          count[4], count[5]);
  // 处理闪烁位
  for (j = 0; j < 6; j++) {
    if ((j == dowmflag) && (dowmflag < 6)) { // 仅处理日期部分
      if (!flash_on) {
        str_t[pos_map_date[j]] = ' '; // 闪烁位替换为空格
      }
    }
  }
  Show_Str(24, 43, WHITE, BLACK, str_t, 16, 0);

  /* 时间显示处理 */
  sprintf(str_t, "%d%d:%d%d:%d%d", count[6], count[7], count[8], count[9],
          count[10], count[11]);
  // 处理闪烁位

  for (j = 0; j < 6; j++) {
    if (((j + 6) == dowmflag) && (dowmflag >= 6)) { // 仅处理时间部分
      if (!flash_on) {
        str_t[pos_map_time[j]] = ' ';
      }
    }
  }
  Show_Str(48, 69, WHITE, BLACK, str_t, 16, 0);

  switch (key_out()) {
  case 2: // 加按键
    count[dowmflag] = ++count[dowmflag] > 9 ? 0 : count[dowmflag];
    flash_timer = 0; // 重置闪烁计时
    flash_on = 1;    // 强制显示
    break;
  case 1: // 确认按键
    if (++dowmflag == 12) {
      for (i = 0; i < 6; i++) {
        ds1302_time_set[i] = count[0 + i * 2] * 10 + count[1 + i * 2];
      }
      Int_time_Set();   //设置时间
      LCD_Clear(BLACK); //清屏
      func_index = 0;   //切换回主界面
      flag = 0;
      dowmflag = 0;
    }
    flash_timer = 0; // 切换时重置闪烁
    flash_on = 1;
    break;
  case 0: // 减按键
    count[dowmflag] = --count[dowmflag] < 0 ? 9 : count[dowmflag];
    flash_timer = 0;
    flash_on = 1;
    break;
  default:
    break;
  }

  if (KEY_DOWN == 0 && KEY_UP == 0) {
    Delay_ms(10);
    if (KEY_DOWN == 0 && KEY_UP == 0) {
      Delay_ms(300);
      buttons[0] = 0;
      buttons[2] = 0;
      LCD_Clear(BLACK);
      func_index = 0;
      flag = 0;
      dowmflag = 0;
    }
  }
  if (sleep_Timing > Save[19]) {
    HPWM_Set(1900, 1);
    LCD_Clear(BLACK);
    flag = 0;
    dowmflag = 0;
    return;
  }
}

void recharge_init() {
  float voltage = 0;
  int num = 0;
  u8 i = 0;
  if (recharge == 0) // 判断这个显示充电标识
  {
    if (BAT_cap_num >= 100) {
      green = 0;
      while (1) {
        if (Save[80])
          Show_Str(4, 56, WHITE, BLACK, "The battery is full", 16, 0);
        else
          Show_Str(32, 56, WHITE, BLACK, "当前电量已满!", 16, 0);
        if (key_get_long_press() == 4) //长按开机
        {
          green = 1;
          LCD_Clear(BLACK);
          P17 = 1;
          return;
        }
      }
    }
    blue = 0;
    while (1) {
      // sensor_adc_get(); //获得adc
      if (Save[80]) {
        //英文
        Show_Str(10, 24, WHITE, BLACK, "Charging,", 16, 0);
        Show_Str(10, 40, WHITE, BLACK, "current power:", 16, 0);
      } else {
        //中文
        Show_Str(10, 40, WHITE, BLACK, "充电中", 16, 0);
        Show_Str(10, 56, WHITE, BLACK, "当前电量:", 16, 0);
      }
      LCD_ShowNum(90, 56, BAT_cap_num, 3, 16);
      if (key_get_long_press() == 4) //长按开机
      {
        blue = 1;
        LCD_Clear(BLACK);
        P17 = 1;
        return;
      }
    }
  }
}

void lowpower_off() {
  u8 i = 0;
  if (BAT_cap_num < 5) { //<=?
    audio_sned(9);
    while (1) {
      if (Save[80]) {
        Show_Str(8, 40, WHITE, BLACK, "The battery is low", 16, 0);
        // Show_Str(32, 58, WHITE, BLACK, "无 法 开 机!", 16, 0);
      } else {
        Show_Str(32, 40, WHITE, BLACK, "电 量 不 足!", 16, 0);
        Show_Str(32, 58, WHITE, BLACK, "无 法 开 机!", 16, 0);
      }
      for (i = 0; i < 4; i++) {
        red = ~red;
        food();
        Delay_ms(500);
        food();
        Delay_ms(500);
        food();
      }
      P17 = 0;
    }
  }
}

void set_Language() {
  static u8 choose = 0;
  static u8 flag = 0;
  if (flag == 0) {
    flag = 1;
    choose = Save[80];
  }
  Show_Str(48, 5, YELLOW, BLACK, "语言切换", 16, 0);
  Show_Str(64, 46, WHITE, BLACK, "中文", 16, 0);
  Show_Str(64, 67, WHITE, BLACK, "英文", 16, 0); // english
  Show_Str(48, 46 + choose * 21, YELLOW, BLACK, "→", 16, 0);
  Show_Str(96, 46 + choose * 21, YELLOW, BLACK, "琨", 16, 0);
  Show_Str(1, 112, YELLOW, BLACK, "加", 16, 0);
  Show_Str(141, 112, YELLOW, BLACK, "减", 16, 0);
  Show_Str(64, 112, YELLOW, BLACK, "确认", 16, 0);

  switch (key_out()) {
  case 0:
    choose = --choose > 1 ? 1 : choose;
    LCD_Fill(48, 46, 63, 103, BLACK);  //白色清屏
    LCD_Fill(96, 46, 112, 103, BLACK); //白色清屏
    break;
  case 1:
    Save[80] = choose;
    Save_set();
    LCD_Clear(BLACK);
    func_index = 0;
    choose = 0;
    flag = 0;
    break;
  case 2:
    choose = ++choose > 1 ? 0 : choose;
    LCD_Fill(48, 46, 63, 103, BLACK);  //白色清屏
    LCD_Fill(96, 46, 112, 103, BLACK); //白色清屏
    break;
  default:
    break;
  }
  if (sleep_Timing > Save[19]) {
    HPWM_Set(1900, 1);
    LCD_Clear(BLACK);
    choose = 0;
    flag = 0;
    return;
  }
}

void scan_password() {
  static char num[4] = {0, 0, 0, 0};
  static count_flag = 0;
  // u16 color_dis[2] = {WHITE, BLACK};

  if (Save[80]) {
    Show_Str(3, 3, YELLOW, BLACK, "Input Password:", 16, 0);
    LCD_ShowNum(58, 56, num[0], 1, 16);
    LCD_ShowNum(70, 56, num[1], 1, 16);
    LCD_ShowNum(82, 56, num[2], 1, 16);
    LCD_ShowNum(94, 56, num[3], 1, 16);
    Show_Str(58 + count_flag * 12, 72, WHITE, BLACK, "^", 16, 0);
    Show_Str(1, 112, YELLOW, BLACK, "Add", 16, 0);
    Show_Str(133, 112, YELLOW, BLACK, "Sub", 16, 0);
    Show_Str(72, 112, YELLOW, BLACK, "OK", 16, 0);
  } else {
    Show_Str(0, 3, YELLOW, BLACK, "请输入密码:", 16, 0);
    LCD_ShowNum(58, 56, num[0], 1, 16);
    LCD_ShowNum(70, 56, num[1], 1, 16);
    LCD_ShowNum(82, 56, num[2], 1, 16);
    LCD_ShowNum(94, 56, num[3], 1, 16);
    Show_Str(58 + count_flag * 12, 72, WHITE, BLACK, "^", 16, 0);
    Show_Str(1, 112, YELLOW, BLACK, "加", 16, 0);
    Show_Str(141, 112, YELLOW, BLACK, "减", 16, 0);
    Show_Str(64, 112, YELLOW, BLACK, "确认", 16, 0);
  }

  switch (key_out()) {
  case 0:
    num[count_flag] = --num[count_flag] < 0 ? 9 : num[count_flag];
    break;
  case 1:
    count_flag++;
    LCD_Fill(58, 72, 104, 88, BLACK);
    if (count_flag == 4) { // 2580--中引文切换，一个判断
      if ((num[0] == 0) && (num[1] == 8) && (num[2] == 0) && (num[3] == 0) &&
          (table[func_index].ret != 7)) //对比密码正确
      {
        count_flag = 0;
        num[0] = 0;
        num[1] = 0;
        num[2] = 0;
        num[3] = 0;
        LCD_Clear(BLACK); //清屏
        if (Save[80])
          func_index = table_E[func_index].ret;
        else
          func_index = table[func_index].ret;
      } else if ((num[0] == 1) && (num[1] == 1) && (num[2] == 1) &&
                 (num[3] == 1) && (table[func_index].ret == 7)) //对比密码正确
      {
        count_flag = 0;
        num[0] = 0;
        num[1] = 0;
        num[2] = 0;
        num[3] = 0;
        LCD_Clear(BLACK); //清屏
        func_index = 7;
      } else //密码错误
      {
        count_flag = 0;
        num[0] = 0;
        num[1] = 0;
        num[2] = 0;
        num[3] = 0;
        LCD_Clear(BLACK); //清屏
        func_index = 0;   //切换回主界面
      }
    }
    break;
  case 2:
    num[count_flag] = ++num[count_flag] > 9 ? 0 : num[count_flag];
    break;
  default:
    break;
  }

  if (KEY_DOWN == 0 && KEY_UP == 0) {
    Delay_ms(10);
    if (KEY_DOWN == 0 && KEY_UP == 0) {
      Delay_ms(300);
      buttons[0] = 0;
      buttons[2] = 0;
      LCD_Clear(BLACK);
      func_index = 0;
      count_flag = 0;
    }
  }
  if (sleep_Timing > Save[19]) {
    HPWM_Set(1900, 1);
    LCD_Clear(BLACK);
    count_flag = 0;
    num[0] = 0;
    num[1] = 0;
    num[2] = 0;
    num[3] = 0;
    return;
  }
}

void balck_system() {

  if (Save[80]) {
    Show_Str(32, 40, YELLOW, BLACK, "Factory reset?", 16, 0);
    Show_Str(2, 111, WHITE, BLACK, "OK", 16, 0);
    Show_Str(126, 111, WHITE, BLACK, "Exit", 16, 0);
  } else {
    Show_Str(3, 40, BLACK, WHITE, "确认恢复出厂设置吗?", 16, 0);
    Show_Str(2, 111, WHITE, BLACK, "确认", 16, 0);
    Show_Str(126, 111, WHITE, BLACK, "取消", 16, 0);
  }
  switch (key_out()) {
  case 0:
    LCD_Clear(BLACK); //清屏
    func_index = 0;   //切换回主界面
    break;
  case 1:
    break;
  case 2:
    LCD_Clear(WHITE); //清屏;
    Delay_ms(500);
    food(); // 喂狗
    Delay_ms(500);
    food();           // 喂狗
    LCD_Clear(BLACK); //清屏; // 全灭
    Delay_ms(500);
    food(); // 喂狗
    Delay_ms(500);
    LCD_Clear(WHITE); // 全亮
    food();           // 喂狗
    Delay_ms(500);
    food(); // 喂狗
    Delay_ms(500);
    food();           // 喂狗
    clear_all_save(); // 全部清除
    P17 = 0;
    IAP_CONTR |= 0x60; // 软件重启
    EA = 0;
    while (1)
      ;

    //恢复
    break;
  default:
    break;
  }
  if (sleep_Timing > Save[19]) {
    LCD_Clear(BLACK);
    return;
  }
}

//气体标零
void zero_set() { // adc_zero
  static u8 flag = 0;
  static u8 count_down = 0;
  u8 i = 0;
  // 显示项配置结构体
  typedef struct {
    u16 x;
    u16 y;
    const char *text;
    u8 flag_id;
  } MenuItem;

  static const MenuItem menu_items[] = {{3, 25, " 可燃气 ", 0},
                                        {3, 44, "  氧气  ", 1},
                                        {3, 63, "一氧化碳", 2},
                                        {3, 82, " 硫化氢 ", 3}};
  sleep_Timing = 0;
  if (flag == 0) {
    flag = 1;
    Show_Str(48, 3, YELLOW, BLACK, "标零设置", 16, 0);
    Show_Str(67, 25, WHITE, BLACK, ":", 16, 0);
    Show_Str(67, 44, WHITE, BLACK, ":", 16, 0);
    Show_Str(67, 63, WHITE, BLACK, ":", 16, 0);
    Show_Str(67, 82, WHITE, BLACK, ":", 16, 0);
    Show_Str(6, 110, WHITE, BLACK, "切换", 16, 0);
    Show_Str(64, 110, WHITE, BLACK, "确认", 16, 0);
    Show_Str(120, 110, WHITE, BLACK, "退出", 16, 0);
  }
  for (i = 0; i < 4; i++) {
    if (menu_items[i].flag_id == count_down) {
      Show_Str(menu_items[i].x, menu_items[i].y, BLACK, WHITE,
               menu_items[i].text, 16, 0);
    } else
      Show_Str(menu_items[i].x, menu_items[i].y, WHITE, BLACK,
               menu_items[i].text, 16, 0);
  }

  LCD_ShowNum(85, 25, sensor_adc[0], 4, 16);
  LCD_ShowNum(85, 44, sensor_adc[1], 4, 16);
  LCD_ShowNum(85, 63, sensor_adc[2], 4, 16);
  LCD_ShowNum(85, 82, sensor_adc[3], 4, 16);

  switch (key_out()) {
  case 0:
    LCD_Clear(BLACK);
    func_index = 0;
    flag = 0;
    count_down = 0;
    break;
  case 1:
    Save[1 + 20 * count_down] = (int)sensor_adc[count_down] >> 8;
    Save[2 + 20 * count_down] = (int)sensor_adc[count_down];
    Save_set();
    Show_Str(127, 25 + count_down * 19, BLACK, WHITE, " OK ", 16, 0);
    break;
  case 2:
    count_down = ++count_down > 3 ? 0 : count_down;
    break;
  default:
    break;
  }
  if (sleep_Timing > Save[19]) {
    HPWM_Set(1900, 1);
    LCD_Clear(BLACK);
    count_down = 0;
    flag = 0;
    return;
  }
}

void Calibration_set() {
  static u8 count_down = 0;
  static u8 gas_flag = 0;
  static u8 flag = 0;
  static u8 num[4] = {0, 0, 0, 0};
  static int zero_adc = 0; // 零点
  static int range = 0;    // 量程
  u16 dem = 0;
  float con = 0;
  char str_Calibration[8] = "";

  sleep_Timing = 0;

  if (flag == 0) {
    flag = 1;
    gas_flag = table[func_index].enter - 1;
    gas_flag = gas_flag > 3 ? 0 : gas_flag;
    // Uart1_printf("气体下标：%d\r\n",(int)gas_flag);
    zero_adc = Save[1 + 20 * gas_flag] << 8 | Save[2 + 20 * gas_flag];
    // Uart1_printf("气体零点：%d\r\n",(int)zero_adc);
    range = Save[10 + 20 * gas_flag] << 8 | Save[11 + 20 * gas_flag];
    // Uart1_printf("气体量程：%d\r\n",(int)range);
    Show_Str(48, 3, YELLOW, BLACK, "标定设置", 16, 0);
    dispaly_unit(120, 52, gas_flag, WHITE);
    Show_Str(3, 52, WHITE, BLACK, "设定浓度:", 16, 0);
    Show_Str(44, 82, WHITE, BLACK, "ADC:", 16, 0);
    Show_Str(64, 110, WHITE, BLACK, "确认", 16, 0);
    Show_Str(2, 112, WHITE, BLACK, "→", 16, 0);
    Show_Str(142, 112, WHITE, BLACK, "栕", 16, 0);
    if (gas_flag == 0) //默认显示值
    {
      flag = 56; //气体x坐标
      num[0] = 0;
      num[1] = 0;
      num[2] = 4;
      num[3] = 0;
    } else if (gas_flag == 1) {
      flag = 64; //气体x坐标
      num[0] = 1;
      num[1] = 0;
      num[2] = 0;
      num[3] = 0;
    } else if (gas_flag == 2) {
      flag = 48; //气体x坐标
      num[0] = 0;
      num[1] = 7;
      num[2] = 0;
      num[3] = 2;
    } else {
      flag = 56; //气体x坐标
      num[0] = 0;
      num[1] = 0;
      num[2] = 6;
      num[3] = 0;
    }
    display_name(flag, 25, gas_flag, 16, YELLOW);
  }
  LCD_ShowNum(76, 82, sensor_adc[gas_flag], 5, 16);
  if (gas_flag == 1) {
    sprintf(str_Calibration, "%d%d.%d", (int)num[0], (int)num[1], (int)num[3]);
  } else {
    sprintf(str_Calibration, "%d%d%d%d", (int)num[0], (int)num[1], (int)num[2],
            (int)num[3]);
  }
  Show_Str(81, 52, WHITE, BLACK, str_Calibration, 16, 0);
  Show_Str(81 + count_down * 8, 68, WHITE, BLACK, "^", 16, 0);
  switch (key_out()) {
  case 0:
    num[count_down] = ++num[count_down] > 9 ? 0 : num[count_down];
    break;
  case 1:
    //计算标定值
    switch (Save[0 + 20 * gas_flag]) { // 计算标定值
    case 0:
    case 3:
    case 2:
      con = num[0] * 1000 + num[1] * 100 + num[2] * 10 + num[3];
      if ((con == 0) || (con > range))
        break;
      dem = (((float)(sensor_adc[gas_flag] - zero_adc) * range) / con);
      dem = dem > 4095 ? 4095 : dem < 0 ? 0 : dem;
      Save[3 + 20 * gas_flag] = dem >> 8; // 改变标定高
      Save[4 + 20 * gas_flag] = dem;      // 改变标定低
      break;
    case 1:
      con = (num[0] * 100 + num[1] * 10 + num[3]) / 10.0;
      if ((con == 20.9) || (con > range))
        break;
      dem = (((float)(sensor_adc[gas_flag] - zero_adc) * range) /
             (con - 20.9)); //
      dem = dem > 4095 ? 4095 : dem < 0 ? 0 : dem;
      Save[3 + 20 * gas_flag] = dem >> 8; // 改变标定高
      Save[4 + 20 * gas_flag] = dem;      // 改变标定低
      break;
    default:
      break;
    }
    //存储
    Save_set();
    LCD_Clear(BLACK);
    func_index = 12;
    flag = 0;
    count_down = 0;

    break;
  case 2:
    count_down = ++count_down > 3 ? 0 : count_down;
    if ((gas_flag == 1) && count_down == 2) {
      count_down = 3;
    }
    LCD_Fill(81, 68, 116, 84, BLACK);
    break;
  default:
    break;
  }
  if (KEY_DOWN == 0 && KEY_UP == 0) {
    Delay_ms(10);
    if (KEY_DOWN == 0 && KEY_UP == 0) {
      Delay_ms(300);
      buttons[0] = 0;
      buttons[2] = 0;
      LCD_Clear(BLACK);
      func_index = 12;
      flag = 0;
      count_down = 0;
    }
  }
  if (sleep_Timing > Save[19]) {
    HPWM_Set(1900, 1);
    LCD_Clear(BLACK);
    count_down = 0;
    flag = 0;
    gas_flag = 0;
    return;
  }
}

void Alarm_set() {
  static u8 count_down = 0;
  static u8 flag = 0;
  static u8 gas_count = 0;
  static float low_alarm = 0;
  static float high_alarm = 0;
  static u8 num[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  char str_alarm[6] = "";

  if (flag == 0) {
    flag = 1;
    if (Save[80]) {
      Show_Str(48, 3, YELLOW, BLACK, "Alarm Set", 16, 0);
      Show_Str(64, 111, WHITE, BLACK, "OK", 16, 0);
      Show_Str(15, 72, WHITE, BLACK, "Alarm_H:", 16, 0);
      Show_Str(15, 36, WHITE, BLACK, "Alarm_L:", 16, 0);
    } else {
      Show_Str(48, 3, YELLOW, BLACK, "报警设置", 16, 0);
      Show_Str(64, 111, WHITE, BLACK, "确认", 16, 0);
      Show_Str(30, 72, WHITE, BLACK, "高报:", 16, 0);
      Show_Str(30, 36, WHITE, BLACK, "低报:", 16, 0);
    }
    Show_Str(2, 112, WHITE, BLACK, "→", 16, 0);
    Show_Str(142, 112, WHITE, BLACK, "栕", 16, 0);
    if (Save[80]) {
      gas_count = table_E[func_index].enter - 1;
    } else
      gas_count = table[func_index].enter - 1; // table[func_index].enter]-1
    low_alarm = (Save[5 + 20 * gas_count] << 8 | Save[6 + 20 * gas_count]) +
                Save[12 + 20 * gas_count] / 10.0;
    high_alarm = (Save[7 + 20 * gas_count] << 8 | Save[8 + 20 * gas_count]) +
                 Save[13 + 20 * gas_count] / 10.0;
    // Uart1_printf("%f---%f\r\n",low_alarm,high_alarm);
    if (Save[9 + 20 * gas_count] == 0) {
      num[3] = (int)low_alarm % 10;
      num[2] = (int)low_alarm % 100 / 10;
      num[1] = (int)low_alarm % 1000 / 100;
      num[0] = (int)low_alarm / 1000;
      num[7] = (int)high_alarm % 10;
      num[6] = (int)high_alarm % 100 / 10;
      num[5] = (int)high_alarm % 1000 / 100;
      num[4] = (int)high_alarm / 1000;
    } else {
      num[2] = (int)(low_alarm * 10) % 10;
      num[1] = (int)(low_alarm * 10) % 100 / 10;
      num[0] = (int)(low_alarm * 10) % 1000 / 100;
      // num[3] = (int)(low_alarm * 10) / 1000;
      num[5] = (int)(high_alarm * 10) % 10;
      num[4] = (int)(high_alarm * 10) % 100 / 10;
      num[3] = (int)(high_alarm * 10) % 1000 / 100;
      // num[7] = (int)(high_alarm * 10) / 1000;
    }
  }

  if (Save[9 + 20 * gas_count] == 0) {
    // dibao
    sprintf(str_alarm, "%d%d%d%d", (int)num[0], (int)num[1], (int)num[2],
            (int)num[3]);
    Show_Str(75, 36, WHITE, BLACK, str_alarm, 16, 0);
    // gaobao
    sprintf(str_alarm, "%d%d%d%d", (int)num[4], (int)num[5], (int)num[6],
            (int)num[7]);
    Show_Str(75, 72, WHITE, BLACK, str_alarm, 16, 0);
    if (count_down < 4)
      Show_Str(75 + count_down * 8, 52, WHITE, BLACK, "^", 16, 0);
    else
      Show_Str(75 + (count_down - 4) * 8, 88, WHITE, BLACK, "^", 16, 0);
  } else {
    // dibao
    sprintf(str_alarm, "%d%d.%d", (int)num[0], (int)num[1], (int)num[2]);
    Show_Str(75, 36, WHITE, BLACK, str_alarm, 16, 0);
    // gaobao
    sprintf(str_alarm, "%d%d.%d", (int)num[3], (int)num[4], (int)num[5]);
    Show_Str(75, 72, WHITE, BLACK, str_alarm, 16, 0);
    if (count_down < 3) {
      if (count_down == 2)
        Show_Str(75 + (count_down + 1) * 8, 52, WHITE, BLACK, "^", 16, 0);
      else
        Show_Str(75 + count_down * 8, 52, WHITE, BLACK, "^", 16, 0);
    } else {
      if (count_down == 5)
        Show_Str(75 + (count_down - 3 + 1) * 8, 88, WHITE, BLACK, "^", 16, 0);
      else
        Show_Str(75 + (count_down - 3) * 8, 88, WHITE, BLACK, "^", 16, 0);
    }
  }
  switch (key_out()) {
  case 0: //调整数值
    num[count_down] = ++num[count_down] > 9 ? 0 : num[count_down];
    break;
  case 1: //确认
    flag = 0;
    count_down = 0;
    if (Save[9 + 20 * gas_count] == 0) {
      low_alarm = num[0] * 1000 + num[1] * 100 + num[2] * 10 + num[3];
      high_alarm = num[4] * 1000 + num[5] * 100 + num[6] * 10 + num[7];
      Save[5 + 20 * gas_count] = (int)low_alarm >> 8;
      Save[6 + 20 * gas_count] = (int)low_alarm;
      Save[7 + 20 * gas_count] = (int)high_alarm >> 8;
      Save[8 + 20 * gas_count] = (int)high_alarm;
      Save_set();
    } else {
      low_alarm = num[0] * 10 + num[1];
      high_alarm = num[3] * 10 + num[4];
      Save[5 + 20 * gas_count] = (int)low_alarm >> 8;
      Save[6 + 20 * gas_count] = (int)low_alarm;
      Save[7 + 20 * gas_count] = (int)high_alarm >> 8;
      Save[8 + 20 * gas_count] = (int)high_alarm;
      Save[12 + 20 * gas_count] = num[2];
      Save[13 + 20 * gas_count] = num[5];
      Save_set();
    }
    // Uart1_printf("%f---%f--%x--%x\r\n",low_alarm,high_alarm,Save[5 + 20 *
    // gas_count],Save[6 + 20 * gas_count]);
    LCD_Clear(BLACK);
    func_index = 12;
    break;
  case 2: //切换位选
    LCD_Fill(75, 52, 110, 68, BLACK);
    LCD_Fill(75, 88, 110, 104, BLACK);
    if (Save[9 + 20 * gas_count] == 0)
      count_down = ++count_down > 7 ? 0 : count_down;
    else
      count_down = ++count_down > 5 ? 0 : count_down;
    break;
  default:
    break;
  }

  if (KEY_DOWN == 0 && KEY_UP == 0) { //退出
    Delay_ms(10);
    if (KEY_DOWN == 0 && KEY_UP == 0) {
      Delay_ms(300);
      buttons[0] = 0;
      buttons[2] = 0;
      LCD_Clear(BLACK);
      func_index = 12;
      flag = 0;
      count_down = 0;
    }
  }

  if (sleep_Timing > Save[19]) { //达到休眠时间
    HPWM_Set(1900, 1);
    LCD_Clear(BLACK);
    count_down = 0;
    flag = 0;
    return;
  }
}

void Alarm_look() {
  u8 str_time[10] = "";
  static u16 choose = 0;
  u16 addr, warning_num = 0;
  static u8 flag = 0;
  static u8 data_warnlog[11];
  sleep_Timing = 0;
  if (flag == 0) {
    flag = 1;
    choose = Save[37] << 8 | Save[38];
    if (Save[80]) {
      Show_Str(44, 3, YELLOW, BLACK, "Alarm Log", 16, 0);
      Show_Str(6, 112, YELLOW, BLACK, "Add", 16, 0);
      Show_Str(64, 112, YELLOW, BLACK, "Exit", 16, 0);
      Show_Str(135, 112, YELLOW, BLACK, "Sub", 16, 0);
    } else {
      Show_Str(48, 3, YELLOW, BLACK, "报警记录", 16, 0);
      Show_Str(6, 112, YELLOW, BLACK, "加", 16, 0);
      Show_Str(64, 112, YELLOW, BLACK, "退出", 16, 0);
      Show_Str(140, 112, YELLOW, BLACK, "减", 16, 0);
    }

    if (choose > 0) {
      addr = (choose - 1) * 11 + 512;
      data_warnlog[0] = IapRead(addr);   // 报警通道
      data_warnlog[1] = IapRead(++addr); // 气体类型
      data_warnlog[2] = IapRead(++addr); // 气体类型
      data_warnlog[3] = IapRead(++addr); // 报警状态
      data_warnlog[4] = IapRead(++addr); // 报警状态
      data_warnlog[5] = IapRead(++addr); // 报警时间
      data_warnlog[6] = IapRead(++addr);
      data_warnlog[7] = IapRead(++addr);
      data_warnlog[8] = IapRead(++addr);
      data_warnlog[9] = IapRead(++addr);
      data_warnlog[10] = IapRead(++addr);
    }
  }
  warning_num = Save[37] << 8 | Save[38];
  if (warning_num == 0) {
    if (Save[80])
      Show_Str(16, 56, WHITE, BLACK, "No Alarm record!", 16, 0);
    else
      Show_Str(36, 56, WHITE, BLACK, "无报警记录!", 16, 0);
  } else {
    if (Save[80]) {
      Show_Str(3, 39, WHITE, BLACK, "Data:", 16, 0);
      Show_Str(3, 57, WHITE, BLACK, "Time:", 16, 0);
      Show_Str(3, 75, WHITE, BLACK, "Type:", 16, 0);
      Show_Str(3, 93, WHITE, BLACK, "Norm:", 16, 0);
      display_name_E(3, 21, data_warnlog[0], 16, WHITE);
    } else {
      Show_Str(3, 39, WHITE, BLACK, "日期:", 16, 0);
      Show_Str(3, 57, WHITE, BLACK, "时间:", 16, 0);
      Show_Str(3, 75, WHITE, BLACK, "类型:", 16, 0);
      Show_Str(3, 93, WHITE, BLACK, "浓度:", 16, 0);
      display_name(3, 21, data_warnlog[0], 16, WHITE);
    }

    sprintf(str_time, "%03d", (int)choose);
    Show_Str(130, 21, WHITE, BLACK, str_time, 16, 0);
    sprintf(str_time, "%02d/%02d/%02d", (int)data_warnlog[5],
            (int)data_warnlog[6], (int)data_warnlog[7]);
    Show_Str(55, 39, WHITE, BLACK, str_time, 16, 0);
    sprintf(str_time, "%02d:%02d:%02d", (int)data_warnlog[8],
            (int)data_warnlog[9], (int)data_warnlog[10]);
    Show_Str(55, 57, WHITE, BLACK, str_time, 16, 0);
    dis_low_high(55, 75, data_warnlog[4]);
    sprintf(str_time, "%4d.%d", (int)(data_warnlog[1] << 8 | data_warnlog[2]),
            (int)data_warnlog[3]);
    Show_Str(43, 93, WHITE, BLACK, str_time, 16, 0);
  }
  if (key_get_long_press() == 3) {
    LCD_Clear(BLACK);
    flag = 0;
    func_index = 15;
  }

  switch (key_out()) {
  case 0:
    choose = --choose < 1 ? warning_num : choose;
    addr = (choose - 1) * 11 + 512;
    data_warnlog[0] = IapRead(addr);   // 报警通道
    data_warnlog[1] = IapRead(++addr); // 气体类型
    data_warnlog[2] = IapRead(++addr); // 气体类型
    data_warnlog[3] = IapRead(++addr); // 报警状态
    data_warnlog[4] = IapRead(++addr); // 报警状态
    data_warnlog[5] = IapRead(++addr); // 报警时间
    data_warnlog[6] = IapRead(++addr);
    data_warnlog[7] = IapRead(++addr);
    data_warnlog[8] = IapRead(++addr);
    data_warnlog[9] = IapRead(++addr);
    data_warnlog[10] = IapRead(++addr);
    break;
  case 1:
    LCD_Clear(BLACK);
    func_index = 0;
    flag = 0;
    // choose = 0; //未必清
    break;
  case 2:
    choose = ++choose > warning_num ? 1 : choose;
    addr = (choose - 1) * 11 + 512;
    data_warnlog[0] = IapRead(addr);   // 气体类型
    data_warnlog[1] = IapRead(++addr); // 气体浓度
    data_warnlog[2] = IapRead(++addr); // 浓度
    data_warnlog[3] = IapRead(++addr); // 浓度小数
    data_warnlog[4] = IapRead(++addr); // 报警状态
    data_warnlog[5] = IapRead(++addr); // 报警时间
    data_warnlog[6] = IapRead(++addr);
    data_warnlog[7] = IapRead(++addr);
    data_warnlog[8] = IapRead(++addr);
    data_warnlog[9] = IapRead(++addr);
    data_warnlog[10] = IapRead(++addr);
    break;
  default:
    break;
  }
  if (sleep_Timing > Save[19]) {
    HPWM_Set(1900, 1);
    LCD_Clear(BLACK);
    flag = 0;
    return;
  }
}
void alarm_log_clear() {
  if (!Save[80]) {
    Show_Str(12, 56, BLACK, WHITE, "确认清空报警记录?", 16, 0);
    Show_Str(2, 111, WHITE, BLACK, "确认", 16, 0);
    Show_Str(126, 111, WHITE, BLACK, "取消", 16, 0);
  } else {
    Show_Str(16, 56, BLACK, WHITE, "Clear Alarm log?", 16, 0);
    Show_Str(2, 111, WHITE, BLACK, "OK", 16, 0);
    Show_Str(126, 111, WHITE, BLACK, "Exit", 16, 0);
  }
  switch (key_out()) {
  case 0:
    LCD_Clear(BLACK);
    func_index = 11;
    break;
  case 2:
    clear_logs();
    LCD_Clear(BLACK);
    func_index = 11;
    break;
  default:
    break;
  }
  if (sleep_Timing > Save[19]) {
    HPWM_Set(1900, 1);
    LCD_Clear(BLACK);
    // flag = 0;
    return;
  }
}
void dis_low_high(u8 x, u8 y, u8 num) {
  switch (num) {
  case 3:
    if (Save[80])
      Show_Str(x, y, WHITE, BLACK, "Low report  ", 16, 0);
    else
      Show_Str(x, y, WHITE, BLACK, "低报  ", 16, 0);
    break;
  case 4:
    if (Save[80])
      Show_Str(x, y, WHITE, BLACK, "High report ", 16, 0);
    else
      Show_Str(x, y, WHITE, BLACK, "高报  ", 16, 0);
    break;
  case 5:
    if (Save[80])
      Show_Str(x, y, WHITE, BLACK, "Excess ranfe", 16, 0);
    else
      Show_Str(x, y, WHITE, BLACK, "超量程", 16, 0);
    break;
  default:
    break;
  }
}
void choose_Gas() {
  u16 color_dis[2] = {WHITE, BLACK};
  u8 i = 0;
  static char count_flag = 0; //反色下标
  // 显示项配置结构体
  typedef struct {
    u16 x;
    u16 y;
    const char *text;
    u8 flag_id;
  } MenuItem;

  static const MenuItem menu_items[] = {
      {0, 3, "气体选择", 0}, // 标题固定反色
      {48, 30, " 可燃气 ", 1}, {48, 56, "  氧气  ", 2},
      {48, 82, "一氧化碳", 3}, {48, 108, " 硫化氢 ", 4},
      {126, 3, "退出", 5}};

  for (i = 0; i < 6; i++) {
    if (menu_items[i].flag_id == count_flag) {
      Show_Str(menu_items[i].x, menu_items[i].y, color_dis[1], color_dis[0],
               menu_items[i].text, 16, 0);
    } else
      Show_Str(menu_items[i].x, menu_items[i].y, color_dis[0], color_dis[1],
               menu_items[i].text, 16, 0);
  }
  switch (key_out()) {
  case 0: //+
    count_flag = ++count_flag > 5 ? 1 : count_flag;
    break;
  case 1: // OK
    switch (count_flag) {
    case 1:
    case 2:
    case 3:
    case 4:
      LCD_Clear(BLACK);                     //清屏
      func_index = table[6].enter;          //进入选定的设置界面
      table[func_index].enter = count_flag; //传递气体下标到下一个界面
      break;
    case 5:
      LCD_Clear(BLACK); //清屏
      func_index = 0;   //切换回主界面
      count_flag = 0;
      break;
    default:
      break;
    }
    break;
  case 2: //-
    count_flag = --count_flag < 1 ? 5 : count_flag;
    break;
  default:
    break;
  }
  if (sleep_Timing > Save[19]) {
    HPWM_Set(1900, 1);
    LCD_Clear(BLACK);
    count_flag = 0;
    return;
  }
}

void backlight_set() {
  static u16 backlight = 0;
  u8 load = 0; // 进度
  u8 i = 50;
  u8 load_Str[4] = "";
  static u8 flag = 0;

  if (flag == 0) {
    flag = 1;
    if (Save[80]) {
      Show_Str(28, 3, YELLOW, BLACK, "Backlight Set", 16, 0);
      Show_Str(1, 112, YELLOW, BLACK, "sub", 16, 0);
      Show_Str(133, 112, YELLOW, BLACK, "add", 16, 0);
      Show_Str(72, 112, YELLOW, BLACK, "ok", 16, 0);
    } else {
      Show_Str(48, 3, YELLOW, BLACK, "背光调节", 16, 0);
      Show_Str(1, 112, YELLOW, BLACK, "减", 16, 0);
      Show_Str(141, 112, YELLOW, BLACK, "加", 16, 0);
      Show_Str(64, 112, YELLOW, BLACK, "确认", 16, 0);
    }
    backlight = Save[17] << 8 | Save[18];
  }

  load = ((2000 - backlight) / 20); //计算百分比数值
  LCD_DrawLine(30, 72, 130, 72);    //绘制方框
  LCD_DrawLine(30, 86, 130, 86);
  LCD_DrawLine(30, 72, 30, 86);
  LCD_DrawLine(130, 72, 130, 86);
  //绘制进度条
  sprintf(load_Str, "%3d%%", (int)load + 1);
  Show_Str(64, 56, WHITE, BLACK, load_Str, 16, 0);
  food();
  LCD_Fill(31, 73, 31 + load, 86, WHITE);
  switch (key_out()) {
  case 0: //减
    backlight -= 20;
    backlight = backlight < 30 ? 20 : backlight;
    break;
  case 1: //确认
    flag = 0;
    LCD_Clear(BLACK);
    func_index = 0;
    Save[17] = backlight >> 8;
    Save[18] = backlight;
    Save_set();
    break;
  case 2: //加
    backlight += 20;
    backlight = backlight > 1980 ? 1990 : backlight;
    LCD_Fill(31 + load, 73, 129, 85, BLACK);
    break;
  default:
    //长按
    i = 100;
    while (KEY_DOWN == 0) {
      while ((KEY_DOWN == 0) && i) {
        i--;
        food();
        Delay_ms(5); //消抖
      }
      if (KEY_DOWN == 1) {
        break;
      }

      backlight -= 20;
      backlight = backlight < 30 ? 20 : backlight;
      load = ((2000 - backlight) / 2000.0) * 100; //计算百分比数值
      // LCD_Fill(31 + load, 73, 129, 85, BLACK);
      //绘制进度条
      sprintf(load_Str, "%3d%%", (int)load + 1);
      Show_Str(64, 56, WHITE, BLACK, load_Str, 16, 0);
      food();
      LCD_Fill(31, 73, 31 + load, 86, WHITE);
      pwm_out_end((int)backlight); // PWM背光输出
      Delay_ms(60);
    }
    //长按
    while (KEY_UP == 0) {
      while ((KEY_UP == 0) && i) {
        i--;
        food();
        Delay_ms(5); //消抖
      }
      if (KEY_UP == 1) {
        break;
      }
      backlight += 20;
      backlight = backlight > 1980 ? 1990 : backlight;
      load = ((2000 - backlight) / 2000.0) * 100; //计算百分比数值
      LCD_Fill(31 + load, 73, 129, 85, BLACK);
      //绘制进度条
      sprintf(load_Str, "%3d%%", (int)load + 1);
      Show_Str(64, 56, WHITE, BLACK, load_Str, 16, 0);
      food();
      LCD_Fill(31, 73, 31 + load, 86, WHITE);
      pwm_out_end((int)backlight); // PWM背光输出
      Delay_ms(60);
    }
    break;
  }

  pwm_out_end((int)backlight); // PWM背光输出
  if (sleep_Timing > Save[19]) {
    HPWM_Set(1900, 1);
    LCD_Clear(BLACK);
    flag = 0;
    return;
  }
}

void sleep_set() {
  static u8 count_down = 0;
  static u8 flag = 0;
  static u8 num[2] = {0, 0};
  char str_alarm[3] = "";

  if (flag == 0) {
    flag = 1;
    num[0] = Save[19] / 10;
    num[1] = Save[19] % 10;
    if (Save[80]) {
      Show_Str(12, 3, YELLOW, BLACK, "Hibernation Set", 16, 0);
      Show_Str(72, 112, WHITE, BLACK, "OK", 16, 0);
      Show_Str(35, 56, WHITE, BLACK, "Time:", 16, 0);
    } else {
      Show_Str(48, 3, YELLOW, BLACK, "休眠设置", 16, 0);
      Show_Str(64, 112, WHITE, BLACK, "确认", 16, 0);
      Show_Str(3, 56, WHITE, BLACK, "休眠时间:", 16, 0);
    }
    Show_Str(1, 112, WHITE, BLACK, "→", 16, 0);
    Show_Str(141, 112, WHITE, BLACK, "栕", 16, 0);
  }
  sprintf(str_alarm, "%d%d", (int)num[0], (int)num[1]);
  Show_Str(77, 56, WHITE, BLACK, str_alarm, 16, 0);
  Show_Str(77 + count_down * 8, 72, WHITE, BLACK, "^", 16, 0);
  switch (key_out()) {
  case 0:
    num[count_down] = ++num[count_down] > 9 ? 0 : num[count_down];
    break;
  case 1:
    Save[19] = num[0] * 10 + num[1];
    Save[19] = Save[19] < 5 ? 5 : Save[19];
    Save_set();
    LCD_Clear(BLACK);
    func_index = 0;
    flag = 0;
    count_down = 0;
    break;
  case 2:
    count_down = ++count_down > 1 ? 0 : count_down;
    LCD_Fill(77, 72, 95, 88, BLACK);
    break;
  default:
    break;
  }
  if (KEY_DOWN == 0 && KEY_UP == 0) {
    Delay_ms(10);
    if (KEY_DOWN == 0 && KEY_UP == 0) {
      Delay_ms(300);
      buttons[0] = 0;
      buttons[2] = 0;
      LCD_Clear(BLACK);
      func_index = 0;
      flag = 0;
      count_down = 0;
    }
  }
  if (sleep_Timing > Save[19]) {
    flag = 0;
    HPWM_Set(1900, 1);
    LCD_Clear(BLACK);
    count_down = 0;
    return;
  }
}

void Mute_mode() {
  static u8 choose = 0, flag = 0;
  if (flag == 0) {
    flag = 1;
    choose = Save[39];
    if (Save[80]) {
      Show_Str(24, 3, YELLOW, BLACK, "Sound settings", 16, 0);
      Show_Str(1, 112, YELLOW, BLACK, "sub", 16, 0);
      Show_Str(133, 112, YELLOW, BLACK, "add", 16, 0);
      Show_Str(72, 112, YELLOW, BLACK, "ok", 16, 0);
      Show_Str(64, 46, WHITE, BLACK, "Open ", 16, 0);
      Show_Str(64, 67, WHITE, BLACK, "Close", 16, 0);
    } else {
      Show_Str(48, 3, YELLOW, BLACK, "声音设置", 16, 0);
      Show_Str(1, 112, YELLOW, BLACK, "减", 16, 0);
      Show_Str(141, 112, YELLOW, BLACK, "加", 16, 0);
      Show_Str(64, 112, YELLOW, BLACK, "确认", 16, 0);
      Show_Str(64, 46, WHITE, BLACK, "开启 ", 16, 0);
      Show_Str(64, 67, WHITE, BLACK, "关闭 ", 16, 0);
    }
  }
  Show_Str(48, 46 + choose * 21, YELLOW, BLACK, "→", 16, 0);

  switch (key_out()) {
  case 0:
    choose = --choose > 1 ? 1 : choose;
    LCD_Fill(48, 46, 63, 103, BLACK); //白色清屏
    break;
  case 1:
    Save[39] = choose;
    Save_set();
    LCD_Clear(BLACK);
    func_index = 0;
    choose = 0;
    flag = 0;
    if (Save[39]) {
      audio_sned(0xfe);
      audio_busy_flag = 1;
    } else
      audio_busy_flag = 0;

    break;
  case 2:
    choose = ++choose > 1 ? 0 : choose;
    LCD_Fill(48, 46, 63, 103, BLACK); //白色清屏
    break;
  default:
    break;
  }
  if (sleep_Timing > Save[19]) {
    HPWM_Set(1900, 1);
    LCD_Clear(BLACK);
    choose = 0;
    flag = 0;
    return;
  }
}
