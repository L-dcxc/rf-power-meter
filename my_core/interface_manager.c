/**
 * @file interface_manager.c
 * @brief 射频功率计界面管理系统实现文件
 * @author 蝙蝠鸭
 * @date 2025-08-10
 */

#include "interface_manager.h"
#include "gpio.h"
#include "tim.h"
#include <stdio.h>
#include <string.h>

/* 全局变量定义 */
InterfaceManager_t g_interface_manager = {
    .current_interface = INTERFACE_MAIN,
    .menu_cursor = 0,
    .brightness_level = 8,
    .alarm_enabled = 1,
    .vswr_alarm_threshold = 3.0f,
    .last_update_time = 0,
    .need_refresh = 1
};
PowerResult_t g_power_result = {
    .forward_power = 0.0f,
    .reflected_power = 0.0f,
    .forward_unit = POWER_UNIT_W,
    .reflected_unit = POWER_UNIT_W,
    .is_valid = 0
};
RFParams_t g_rf_params = {
    .vswr = 1.0f,
    .reflection_coeff = 0.0f,
    .transmission_eff = 100.0f,
    .vswr_color = VSWR_COLOR_GREEN,
    .is_valid = 0
};

/* 菜单表定义 */
const MenuItem_t menu_table[MAX_MENU_ITEMS] = {
    {INTERFACE_CALIBRATION, INTERFACE_MENU, INTERFACE_STANDARD, Interface_DisplayCalibration, "Calibration"},
    {INTERFACE_STANDARD, INTERFACE_MENU, INTERFACE_ALARM, Interface_DisplayStandard, "Settings"},
    {INTERFACE_ALARM, INTERFACE_MENU, INTERFACE_BRIGHTNESS, Interface_DisplayAlarm, "Alarm Limit"},
    {INTERFACE_BRIGHTNESS, INTERFACE_MENU, INTERFACE_ABOUT, Interface_DisplayBrightness, "Brightness"},
    {INTERFACE_ABOUT, INTERFACE_MENU, INTERFACE_CALIBRATION, Interface_DisplayAbout, "About"},
    {INTERFACE_MENU, INTERFACE_MAIN, INTERFACE_CALIBRATION, Interface_DisplayMenu, "Settings Menu"}
};

/**
 * @brief 界面管理器初始化
 */
int8_t InterfaceManager_Init(void)
{
    // 初始化界面管理器状态
    g_interface_manager.current_interface = INTERFACE_MAIN;
    g_interface_manager.menu_cursor = 0;
    g_interface_manager.brightness_level = 8;  // 默认80%亮度
    g_interface_manager.alarm_enabled = 1;     // 默认使能报警
    g_interface_manager.vswr_alarm_threshold = 3.0f;  // 默认VSWR报警阈值
    g_interface_manager.last_update_time = 0;
    g_interface_manager.need_refresh = 1;
    
    // 初始化功率数据
    g_power_result.forward_power = 0.0f;
    g_power_result.reflected_power = 0.0f;
    g_power_result.forward_unit = POWER_UNIT_W;
    g_power_result.reflected_unit = POWER_UNIT_W;
    g_power_result.is_valid = 0;
    
    // 初始化射频参数
    g_rf_params.vswr = 1.0f;
    g_rf_params.reflection_coeff = 0.0f;
    g_rf_params.transmission_eff = 100.0f;
    g_rf_params.vswr_color = VSWR_COLOR_GREEN;
    g_rf_params.is_valid = 0;
    
    // 设置初始背光亮度
    InterfaceManager_SetBrightness(g_interface_manager.brightness_level);
    
    return 0;
}

/**
 * @brief 界面管理器主循环处理
 */
void InterfaceManager_Process(void)
{
    uint32_t current_time = HAL_GetTick();
    
    // 检查是否需要更新显示
    if (current_time - g_interface_manager.last_update_time >= DISPLAY_UPDATE_MS || 
        g_interface_manager.need_refresh) {
        
        g_interface_manager.last_update_time = current_time;
        g_interface_manager.need_refresh = 0;
        
        // 根据当前界面调用相应的显示函数
        switch (g_interface_manager.current_interface) {
            case INTERFACE_MAIN:
                Interface_DisplayMain();
                break;
            case INTERFACE_MENU:
                Interface_DisplayMenu();
                break;
            case INTERFACE_CALIBRATION:
                Interface_DisplayCalibration();
                break;
            case INTERFACE_STANDARD:
                Interface_DisplayStandard();
                break;
            case INTERFACE_ALARM:
                Interface_DisplayAlarm();
                break;
            case INTERFACE_BRIGHTNESS:
                Interface_DisplayBrightness();
                break;
            case INTERFACE_ABOUT:
                Interface_DisplayAbout();
                break;
            default:
                g_interface_manager.current_interface = INTERFACE_MAIN;
                break;
        }
    }
    
    // 处理按键
    KeyValue_t key = InterfaceManager_GetKey();
    if (key != KEY_NONE) {
        InterfaceManager_HandleKey(key, KEY_STATE_PRESSED);
        InterfaceManager_Beep(30);  // 按键反馈音
    }
}

/**
 * @brief 按键处理函数
 */
void InterfaceManager_HandleKey(KeyValue_t key, KeyState_t state)
{
    switch (g_interface_manager.current_interface) {
        case INTERFACE_MAIN:
            // 主界面按键处理
            if (key == KEY_OK) {
                InterfaceManager_SwitchTo(INTERFACE_MENU);
            }
            break;
            
        case INTERFACE_MENU:
            // 菜单界面按键处理
            if (key == KEY_UP) {
                // 返回主界面
                InterfaceManager_SwitchTo(INTERFACE_MAIN);
            } else if (key == KEY_DOWN) {
                // 切换菜单项
                g_interface_manager.menu_cursor = (g_interface_manager.menu_cursor + 1) % (MAX_MENU_ITEMS - 1);
                g_interface_manager.need_refresh = 1;
            } else if (key == KEY_OK) {
                // 进入选中的菜单项
                InterfaceIndex_t target = (InterfaceIndex_t)(INTERFACE_CALIBRATION + g_interface_manager.menu_cursor);
                InterfaceManager_SwitchTo(target);
            }
            break;
            
        case INTERFACE_BRIGHTNESS:
            // 亮度界面按键处理
            if (key == KEY_UP) {
                // 返回菜单
                InterfaceManager_SwitchTo(INTERFACE_MENU);
            } else if (key == KEY_DOWN) {
                // 调节亮度 (1-10循环)
                uint8_t new_level = g_interface_manager.brightness_level + 1;
                if (new_level > 10) {
                    new_level = 1;  // 循环到最低亮度
                    LCD_Fill(11, 76, 138, 84, BLACK);
                }
                InterfaceManager_SetBrightness(new_level);
                g_interface_manager.need_refresh = 1;  // 立即刷新显示
            } else if (key == KEY_OK) {
                // 确认当前亮度设置，返回菜单
                InterfaceManager_SwitchTo(INTERFACE_MENU);
            }
            break;

        default:
            // 其他界面按键处理
            if (key == KEY_UP) {
                // 返回菜单
                InterfaceManager_SwitchTo(INTERFACE_MENU);
            }
            break;
    }
}

/**
 * @brief 切换到指定界面
 */
void InterfaceManager_SwitchTo(InterfaceIndex_t interface)
{
    if (interface != g_interface_manager.current_interface) {
        g_interface_manager.current_interface = interface;
        g_interface_manager.need_refresh = 1;
        LCD_Clear(BLACK);  // 清屏
    }
}

/**
 * @brief 强制刷新当前界面
 */
void InterfaceManager_ForceRefresh(void)
{
    g_interface_manager.need_refresh = 1;
}

/**
 * @brief 更新功率数据
 */
void InterfaceManager_UpdatePower(float forward_power, float reflected_power)
{
    // 自动选择功率单位
    if (forward_power >= 1000.0f) {
        g_power_result.forward_power = forward_power / 1000.0f;
        g_power_result.forward_unit = POWER_UNIT_KW;
    } else {
        g_power_result.forward_power = forward_power;
        g_power_result.forward_unit = POWER_UNIT_W;
    }
    
    if (reflected_power >= 1000.0f) {
        g_power_result.reflected_power = reflected_power / 1000.0f;
        g_power_result.reflected_unit = POWER_UNIT_KW;
    } else {
        g_power_result.reflected_power = reflected_power;
        g_power_result.reflected_unit = POWER_UNIT_W;
    }
    
    g_power_result.is_valid = 1;
    
    // 如果在主界面，标记需要刷新
    if (g_interface_manager.current_interface == INTERFACE_MAIN) {
        g_interface_manager.need_refresh = 1;
    }
}

/**
 * @brief 更新射频参数
 */
void InterfaceManager_UpdateRFParams(float vswr, float reflection_coeff, float transmission_eff)
{
    g_rf_params.vswr = vswr;
    g_rf_params.reflection_coeff = reflection_coeff;
    g_rf_params.transmission_eff = transmission_eff;
    
    // 根据VSWR值设置颜色警示
    if (vswr <= 1.5f) {
        g_rf_params.vswr_color = VSWR_COLOR_GREEN;
    } else if (vswr <= 2.0f) {
        g_rf_params.vswr_color = VSWR_COLOR_YELLOW;
    } else {
        g_rf_params.vswr_color = VSWR_COLOR_RED;
    }
    
    g_rf_params.is_valid = 1;
    
    // 检查是否需要报警
    if (g_interface_manager.alarm_enabled && vswr > g_interface_manager.vswr_alarm_threshold) {
        InterfaceManager_Beep(500);  // 报警蜂鸣
    }
    
    // 如果在主界面，标记需要刷新
    if (g_interface_manager.current_interface == INTERFACE_MAIN) {
        g_interface_manager.need_refresh = 1;
    }
}

/**
 * @brief 获取按键值
 */
KeyValue_t InterfaceManager_GetKey(void)
{
    static uint32_t last_key_time = 0;
    static KeyValue_t last_key = KEY_NONE;
    uint32_t current_time = HAL_GetTick();
    
    // 防抖处理，50ms内忽略重复按键
    if (current_time - last_key_time < 50) {
        return KEY_NONE;
    }
    
    // 读取按键状态 (低电平有效)
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12) == GPIO_PIN_RESET) {
        if (last_key != KEY_UP) {
            last_key_time = current_time;
            last_key = KEY_UP;
            return KEY_UP;
        }
    } else if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13) == GPIO_PIN_RESET) {
        if (last_key != KEY_OK) {
            last_key_time = current_time;
            last_key = KEY_OK;
            return KEY_OK;
        }
    } else if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == GPIO_PIN_RESET) {
        if (last_key != KEY_DOWN) {
            last_key_time = current_time;
            last_key = KEY_DOWN;
            return KEY_DOWN;
        }
    } else {
        last_key = KEY_NONE;
    }
    
    return KEY_NONE;
}

/**
 * @brief 设置背光亮度
 */
void InterfaceManager_SetBrightness(uint8_t level)
{
    if (level > 10) level = 10;

    g_interface_manager.brightness_level = level;

    // 计算PWM占空比 (10%-100%)
    uint16_t duty = 99 + (level * 90);

    // 设置TIM3 CH3的PWM占空比 (PB0背光控制)
    LCD_SetBacklight(duty);
}

/**
 * @brief 触发蜂鸣器
 */
void InterfaceManager_Beep(uint16_t duration_ms)
{
    // 蜂鸣器控制 (PB4)
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
    HAL_Delay(duration_ms);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);
}

/**
 * @brief 主界面显示
 */
void Interface_DisplayMain(void)
{
    char str_buffer[32];
    uint16_t vswr_color;

    // 显示标题
    Show_Str(20, 5, WHITE, BLACK, (uint8_t*)"RF Power Meter", 16, 0);

    // 绘制分割线
    LCD_DrawLine(0, 25, 160, 25);
    LCD_DrawLine(80, 25, 80, 95);

    // 左侧显示功率信息
    Show_Str(5, 30, CYAN, BLACK, (uint8_t*)"Forward:", 12, 0);
    if (g_power_result.is_valid) {
        sprintf(str_buffer, "%.2f %s", g_power_result.forward_power,
                g_power_result.forward_unit == POWER_UNIT_KW ? "kW" : "W");
        Show_Str(5, 45, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);
    } else {
        Show_Str(5, 45, GRAY, BLACK, (uint8_t*)"-- W", 12, 0);
    }

    Show_Str(5, 60, CYAN, BLACK, (uint8_t*)"Reflect:", 12, 0);
    if (g_power_result.is_valid) {
        sprintf(str_buffer, "%.2f %s", g_power_result.reflected_power,
                g_power_result.reflected_unit == POWER_UNIT_KW ? "kW" : "W");
        Show_Str(5, 75, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);
    } else {
        Show_Str(5, 75, GRAY, BLACK, (uint8_t*)"-- W", 12, 0);
    }

    // 右侧显示射频参数
    Show_Str(85, 30, CYAN, BLACK, (uint8_t*)"VSWR:", 12, 0);
    if (g_rf_params.is_valid) {
        // 根据VSWR值选择颜色
        switch (g_rf_params.vswr_color) {
            case VSWR_COLOR_GREEN:  vswr_color = GREEN; break;
            case VSWR_COLOR_YELLOW: vswr_color = YELLOW; break;
            case VSWR_COLOR_RED:    vswr_color = RED; break;
            default:                vswr_color = WHITE; break;
        }
        sprintf(str_buffer, "%.2f", g_rf_params.vswr);
        Show_Str(85, 45, vswr_color, BLACK, (uint8_t*)str_buffer, 12, 0);
    } else {
        Show_Str(85, 45, GRAY, BLACK, (uint8_t*)"--", 12, 0);
    }

    Show_Str(85, 60, CYAN, BLACK, (uint8_t*)"Refl.Coef:", 12, 0);
    if (g_rf_params.is_valid) {
        sprintf(str_buffer, "%.3f", g_rf_params.reflection_coeff);
        Show_Str(85, 75, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);
    } else {
        Show_Str(85, 75, GRAY, BLACK, (uint8_t*)"--", 12, 0);
    }

    // 底部显示传输效率和频率
    LCD_DrawLine(0, 95, 160, 95);

    Show_Str(5, 100, CYAN, BLACK, (uint8_t*)"Efficiency:", 12, 0);
    if (g_rf_params.is_valid) {
        sprintf(str_buffer, "%.1f%%", g_rf_params.transmission_eff);
        Show_Str(70, 100, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);
    } else {
        Show_Str(70, 100, GRAY, BLACK, (uint8_t*)"--%", 12, 0);
    }

    Show_Str(5, 115, CYAN, BLACK, (uint8_t*)"Freq:", 12, 0);
    FreqResult_t freq_result;
    if (FreqCounter_GetResult(&freq_result) == 0) {
        sprintf(str_buffer, "%.2f %s", freq_result.frequency_display,
                FreqCounter_GetUnitString(freq_result.unit));
        Show_Str(40, 115, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);
    } else {
        Show_Str(40, 115, GRAY, BLACK, (uint8_t*)"-- Hz", 12, 0);
    }

    // 显示操作提示
    Show_Str(110, 115, GRAY, BLACK, (uint8_t*)"OK:Menu", 12, 0);
}

/**
 * @brief 菜单界面显示
 */
void Interface_DisplayMenu(void)
{
    char str_buffer[32];
    const char* menu_items[] = {
        "Calibration",
        "Settings",
        "Alarm Limit",
        "Brightness",
        "About"
    };

    // 显示标题
    Show_Str(40, 5, WHITE, BLACK, (uint8_t*)"Settings Menu", 16, 0);
    LCD_DrawLine(0, 25, 160, 25);

    // 显示菜单项
    for (int i = 0; i < MAX_MENU_ITEMS - 1; i++) {
        uint16_t color = (i == g_interface_manager.menu_cursor) ? YELLOW : WHITE;
        uint16_t bg_color = (i == g_interface_manager.menu_cursor) ? BLUE : BLACK;

        sprintf(str_buffer, "> %s", menu_items[i]);
        Show_Str(10, 35 + i * 18, color, bg_color, (uint8_t*)str_buffer, 16, 0);
    }

    // 显示操作提示
    //Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back DN:Next OK:Enter", 12, 0);
}

/**
 * @brief 校准界面显示
 */
void Interface_DisplayCalibration(void)
{
    Show_Str(40, 5, WHITE, BLACK, (uint8_t*)"Calibration", 16, 0);
    LCD_DrawLine(0, 25, 160, 25);

    Show_Str(10, 35, CYAN, BLACK, (uint8_t*)"Cal Steps:", 16, 0);
    Show_Str(10, 55, WHITE, BLACK, (uint8_t*)"1. Open Cal", 12, 0);
    Show_Str(10, 70, WHITE, BLACK, (uint8_t*)"2. Short Cal", 12, 0);
    Show_Str(10, 85, WHITE, BLACK, (uint8_t*)"3. Load Cal", 12, 0);
    Show_Str(10, 100, WHITE, BLACK, (uint8_t*)"4. Power Cal", 12, 0);

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back", 12, 0);
}

/**
 * @brief 标定界面显示
 */
void Interface_DisplayStandard(void)
{
    Show_Str(50, 5, WHITE, BLACK, (uint8_t*)"Settings", 16, 0);
    LCD_DrawLine(0, 25, 160, 25);

    Show_Str(10, 35, CYAN, BLACK, (uint8_t*)"Power Cal:", 16, 0);
    Show_Str(10, 55, WHITE, BLACK, (uint8_t*)"Forward Slope: 1.00", 12, 0);
    Show_Str(10, 70, WHITE, BLACK, (uint8_t*)"Reflect Slope: 1.00", 12, 0);
    Show_Str(10, 85, WHITE, BLACK, (uint8_t*)"Zero Offset: 0.00V", 12, 0);

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back", 12, 0);
}

/**
 * @brief 报警设置界面显示
 */
void Interface_DisplayAlarm(void)
{
    char str_buffer[32];

    Show_Str(40, 5, WHITE, BLACK, (uint8_t*)"Alarm Limit", 16, 0);
    LCD_DrawLine(0, 25, 160, 25);

    Show_Str(10, 35, CYAN, BLACK, (uint8_t*)"Alarm Setup:", 16, 0);

    // 显示报警使能状态
    sprintf(str_buffer, "Alarm Enable: %s", g_interface_manager.alarm_enabled ? "ON" : "OFF");
    Show_Str(10, 55, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    // 显示VSWR阈值
    sprintf(str_buffer, "VSWR Limit: %.1f", g_interface_manager.vswr_alarm_threshold);
    Show_Str(10, 70, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    Show_Str(10, 85, YELLOW, BLACK, (uint8_t*)"Buzzer when exceed", 12, 0);

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back", 12, 0);
}

/**
 * @brief 亮度设置界面显示
 */
void Interface_DisplayBrightness(void)
{
    char str_buffer[32];

    Show_Str(40, 5, WHITE, BLACK, (uint8_t*)"Brightness", 16, 0);
    LCD_DrawLine(0, 25, 160, 25);

    Show_Str(10, 32, CYAN, BLACK, (uint8_t*)"Backlight:", 16, 0);

    // 显示当前亮度
    sprintf(str_buffer, "Current: %02d0%%", g_interface_manager.brightness_level);
    Show_Str(10, 55, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    // 绘制亮度条
    LCD_DrawRectangle(10, 75, 139, 85);

    // 先清除进度条内部区域
    //LCD_Fill(11, 76, 139, 84, BLACK);

    // 计算并绘制当前亮度条
    uint16_t bar_width = g_interface_manager.brightness_level * 128 / 10;  // 128像素宽度 (139-11)
    if (bar_width > 0) {
        LCD_Fill(11, 76, 11 + bar_width - 1, 84, GREEN);  // 修正结束坐标
    }

    Show_Str(10, 95, YELLOW, BLACK, (uint8_t*)"DN:Add OK:Confirm", 12, 0);
    Show_Str(5, 115, YELLOW, BLACK, (uint8_t*)"UP:Back", 12, 0);
    
}

/**
 * @brief 关于界面显示
 */
void Interface_DisplayAbout(void)
{
    Show_Str(60, 5, WHITE, BLACK, (uint8_t*)"About", 16, 0);
    LCD_DrawLine(0, 25, 160, 25);

    Show_Str(10, 35, CYAN, BLACK, (uint8_t*)"RF Power Meter V1.0", 16, 0);
    Show_Str(10, 55, WHITE, BLACK, (uint8_t*)"Freq: 1Hz-100MHz", 12, 0);
    Show_Str(10, 70, WHITE, BLACK, (uint8_t*)"Power: 0.1W-1kW", 12, 0);
    Show_Str(10, 85, WHITE, BLACK, (uint8_t*)"VSWR: 1.0-10.0", 12, 0);
    Show_Str(10, 100, WHITE, BLACK, (uint8_t*)"Manufacturer: YourCorp", 12, 0);

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back", 12, 0);
}

/**
 * @brief 系统启动序列界面
 */
void System_BootSequence(void)
{
    //uint8_t progress = 0;
    uint8_t i = 0;
    char progress_str[8] = "";

    // 显示启动标题
    LCD_Clear(BLACK);
    Show_Str(20, 10, GREEN, BLACK, (uint8_t*)"RF Power Meter", 16, 0);
    Show_Str(30, 30, WHITE, BLACK, (uint8_t*)"System Boot", 16, 0);

    Show_Str(20, 65, CYAN, BLACK, (uint8_t*)"Initializing...", 12, 0);

    // 初始进度显示 (0-10%)
    for (i = 0; i <= 10; i++) {
        sprintf(progress_str, "%d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);  // 移动到原进度条位置
        HAL_Delay(50);
    }

    // 步骤1：频率计初始化
    Show_Str(20, 80, YELLOW, BLACK, (uint8_t*)"FreqCounter Init", 12, 0);

    // 进度显示 (10-30%)
    for (i = 10; i <= 30; i++) {
        sprintf(progress_str, "%d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);
        HAL_Delay(30);
    }

    if (FreqCounter_Init() != 0) {
        LCD_Clear(BLACK);
        Show_Str(10, 50, RED, BLACK, (uint8_t*)"FreqCounter Init FAIL!", 16, 0);
        Show_Str(30, 70, WHITE, BLACK, (uint8_t*)"System Halted", 16, 0);
        while(1) {
            HAL_Delay(1000);  // 停止运行
        }
    }
    Show_Str(130, 80, GREEN, BLACK, (uint8_t*)"OK", 12, 0);

    // 步骤2：频率计启动
    Show_Str(20, 95, YELLOW, BLACK, (uint8_t*)"FreqCounter Start", 12, 0);

    // 进度显示 (30-50%)
    for (i = 30; i <= 50; i++) {
        sprintf(progress_str, "%d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);
        HAL_Delay(30);
    }

    if (FreqCounter_Start() != 0) {
        LCD_Clear(BLACK);
        Show_Str(10, 50, RED, BLACK, (uint8_t*)"FreqCounter Start FAIL!", 16, 0);
        Show_Str(30, 70, WHITE, BLACK, (uint8_t*)"System Halted", 16, 0);
        while(1) {
            HAL_Delay(1000);  // 停止运行
        }
    }
    Show_Str(130, 95, GREEN, BLACK, (uint8_t*)"OK", 12, 0);

    // 步骤3：界面管理器初始化
    Show_Str(20, 110, YELLOW, BLACK, (uint8_t*)"Interface Init", 12, 0);

    // 进度显示 (50-70%)
    for (i = 50; i <= 70; i++) {
        sprintf(progress_str, "%d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);
        HAL_Delay(30);
    }

    if (InterfaceManager_Init() != 0) {
        LCD_Clear(BLACK);
        Show_Str(10, 50, RED, BLACK, (uint8_t*)"Interface Init FAIL!", 16, 0);
        Show_Str(30, 70, WHITE, BLACK, (uint8_t*)"System Halted", 16, 0);
        while(1) {
            HAL_Delay(1000);  // 停止运行
        }
    }
    Show_Str(130, 110, GREEN, BLACK, (uint8_t*)"OK", 12, 0);
    HAL_Delay(300);

    // 步骤4：PWM启动 (需要清屏重绘，因为屏幕空间不够)
    LCD_Clear(BLACK);
    Show_Str(20, 10, GREEN, BLACK, (uint8_t*)"RF Power Meter", 16, 0);
    Show_Str(30, 30, WHITE, BLACK, (uint8_t*)"System Boot", 16, 0);

    // 恢复之前的进度 (70%)
    sprintf(progress_str, "%d%%", 70);
    Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);

    Show_Str(20, 80, YELLOW, BLACK, (uint8_t*)"PWM Backlight", 12, 0);

    // 进度显示 (70-85%)
    for (i = 70; i <= 85; i++) {
        sprintf(progress_str, "%d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);
        HAL_Delay(30);
    }

    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
    Show_Str(130, 80, GREEN, BLACK, (uint8_t*)"OK", 12, 0);

    // 步骤5：系统就绪
    Show_Str(20, 95, YELLOW, BLACK, (uint8_t*)"System Ready", 12, 0);

    // 进度显示 (85-100%)
    for (i = 85; i <= 100; i++) {
        sprintf(progress_str, "%d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);
        HAL_Delay(25);
    }

    Show_Str(130, 95, GREEN, BLACK, (uint8_t*)"OK", 12, 0);

    // 显示完成信息
    Show_Str(35, 110, WHITE, BLACK, (uint8_t*)"Boot Complete!", 16, 0);
    HAL_Delay(1000);

    // 清屏准备进入主界面
    LCD_Clear(BLACK);
}
