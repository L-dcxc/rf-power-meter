/**
 * @file interface_manager.c
 * @brief 射频功率计界面管理系统实现文件
 * @author 蝙蝠鸭
 * @date 2025-08-10
 */

#include "interface_manager.h"
#include "gpio.h"
#include "tim.h"
#include "adc.h"
#include "eeprom_24c16.h"
#include "frequency_counter.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "stm32f1xx_it.h"
// 外部变量声明
extern IWDG_HandleTypeDef hiwdg;
extern ADC_HandleTypeDef hadc1;

/* 全局变量定义 */
InterfaceManager_t g_interface_manager = {
    .current_interface = INTERFACE_MAIN,
    .menu_cursor = 0,
    .brightness_level = 8,
    .alarm_enabled = 1,
    .vswr_alarm_threshold = 3.0f,
    .alarm_selected_item = 0,
    .vswr_digit_position = 0,
    .last_update_time = 0,
    .need_refresh = 1,
    .key_press_start_time = 0,
    .key_long_press_detected = 0,
    .interface_first_enter = 1
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

/* 蜂鸣器状态全局变量 */
BuzzerState_t g_buzzer_state = {
    .is_active = 0,
    .duration_count = 0
};

/* 校准数据全局变量 */
CalibrationData_t g_calibration_data = {
    .forward_offset = 0.0f,
    .reflected_offset = 0.0f,
    .fwd_table = {
        // 初始化20个校准点的默认值
        {100.0f, 1.0f}, {200.0f, 2.0f}, {300.0f, 3.0f}, {400.0f, 4.0f}, {500.0f, 5.0f},
        {600.0f, 6.0f}, {700.0f, 7.0f}, {800.0f, 8.0f}, {900.0f, 9.0f}, {1000.0f, 10.0f},
        {1100.0f, 11.0f}, {1200.0f, 12.0f}, {1300.0f, 13.0f}, {1400.0f, 14.0f}, {1500.0f, 15.0f},
        {1600.0f, 16.0f}, {1700.0f, 17.0f}, {1800.0f, 18.0f}, {1900.0f, 19.0f}, {2000.0f, 20.0f}
    },
    .ref_table = {
        // 反射功率校准表初始值
        {100.0f, 0.1f}, {200.0f, 0.2f}, {300.0f, 0.3f}, {400.0f, 0.4f}, {500.0f, 0.5f},
        {600.0f, 0.6f}, {700.0f, 0.7f}, {800.0f, 0.8f}, {900.0f, 0.9f}, {1000.0f, 1.0f},
        {1100.0f, 1.1f}, {1200.0f, 1.2f}, {1300.0f, 1.3f}, {1400.0f, 1.4f}, {1500.0f, 1.5f},
        {1600.0f, 1.6f}, {1700.0f, 1.7f}, {1800.0f, 1.8f}, {1900.0f, 1.9f}, {2000.0f, 2.0f}
    },
    .fwd_points = 0,            // 初始无校准点
    .ref_points = 0,            // 初始无校准点
    .freq_gain_fwd = 1.0f,      // 频率增益修正系数(正向)
    .freq_gain_ref = 1.0f,      // 频率增益修正系数(反射)
    .cal_frequency = 14.0f,     // 标定频率(MHz)
    .freq_trim = 1.0f,
    .is_calibrated = 0
};

/* 校准状态全局变量 */
CalibrationState_t g_calibration_state = {
    .current_step = CAL_STEP_CONFIRM,
    .cal_frequency = 14.0f,     // 默认标定频率14MHz
    .current_power_point = 0,
    .current_channel = 0,
    .sample_count = 0,
    .sample_sum_fwd = 0.0f,
    .sample_sum_ref = 0.0f,
    .is_stable = 0,
    .stable_count = 0,
    .target_power = 100.0f,     // 默认目标功率100W
    .power_cal_mode = 0,        // 默认正向功率校准
    .sample_completed = 0       // 采样未完成
};

/* 频率标定范围定义 */
#define MIN_CAL_FREQ    1.0f    // 最小标定频率(MHz)
#define MAX_CAL_FREQ    50.0f   // 最大标定频率(MHz)
#define FREQ_STEP       1.0f    // 频率调整步长(MHz)

/* 功率校准点定义 (0W-2kW范围，20个校准点) */
static const float power_cal_points[20] = {
    100.0f, 200.0f, 300.0f, 400.0f, 500.0f,      // 100W-500W
    600.0f, 700.0f, 800.0f, 900.0f, 1000.0f,     // 600W-1000W
    1100.0f, 1200.0f, 1300.0f, 1400.0f, 1500.0f, // 1100W-1500W
    1600.0f, 1700.0f, 1800.0f, 1900.0f, 2000.0f  // 1600W-2000W
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
    // 初始化EEPROM
    BL24C16_Init();

    // 初始化界面管理器状态
    g_interface_manager.current_interface = INTERFACE_MAIN;
    g_interface_manager.menu_cursor = 0;

    // 从EEPROM读取亮度值，失败则使用默认值
    uint8_t saved_brightness;
    if (BL24C16_Read(0x0000, &saved_brightness, 1) == EEPROM_OK && saved_brightness >= 1 && saved_brightness <= 10) {
        g_interface_manager.brightness_level = saved_brightness;
    } else {
        g_interface_manager.brightness_level = 8;  // 默认80%亮度
    }

    // 从EEPROM读取报警设置，失败则使用默认值
    uint8_t saved_alarm_enabled;
    float saved_vswr_threshold;
    if (BL24C16_Read(0x0001, &saved_alarm_enabled, 1) == EEPROM_OK && saved_alarm_enabled <= 1) {
        g_interface_manager.alarm_enabled = saved_alarm_enabled;
    } else {
        g_interface_manager.alarm_enabled = 1;     // 默认使能报警
    }
    if (BL24C16_Read(0x0002, (uint8_t*)&saved_vswr_threshold, sizeof(float)) == EEPROM_OK &&
        saved_vswr_threshold >= 1.0f && saved_vswr_threshold <= 999.9f) {
        g_interface_manager.vswr_alarm_threshold = saved_vswr_threshold;
    } else {
        g_interface_manager.vswr_alarm_threshold = 3.0f;  // 默认VSWR报警阈值
    }
    g_interface_manager.alarm_selected_item = 0;  // 默认选中报警开关
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

    // 初始化校准系统
    Calibration_Init();

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
            case INTERFACE_CAL_STEP_SELECT:
                Interface_DisplayCalStepSelect();
                break;
            case INTERFACE_CAL_CONFIRM:
                Interface_DisplayCalConfirm();
                break;
            case INTERFACE_CAL_ZERO:
                Interface_DisplayCalZero();
                break;
            case INTERFACE_CAL_POWER:
                Interface_DisplayCalPower();
                break;
            case INTERFACE_CAL_BAND:
                Interface_DisplayCalBand();
                break;

            case INTERFACE_CAL_COMPLETE:
                Interface_DisplayCalComplete();
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
    
    // 处理校准采样（如果在校准模式中）
    if (g_interface_manager.current_interface >= INTERFACE_CAL_ZERO &&
        g_interface_manager.current_interface <= INTERFACE_CAL_BAND) {
        Calibration_ProcessSample();
    }

    // 处理按键
    KeyValue_t key = InterfaceManager_GetKey();
    if (key != KEY_NONE) {
        InterfaceManager_HandleKey(key, KEY_STATE_PRESSED);
        InterfaceManager_Beep(50);  // 按键反馈音
    }
}

/**
 * @brief 长按处理函数
 */
void InterfaceManager_HandleLongPress(uint8_t key)
{
    if (key == 1) {  // UP键长按
        switch (g_interface_manager.current_interface) {
            case INTERFACE_CAL_POWER:
                // 功率标定界面长按UP键返回零点标定
                Calibration_StartStep(CAL_STEP_ZERO);
                InterfaceManager_SwitchTo(INTERFACE_CAL_ZERO);
                InterfaceManager_Beep(200);  // 长按反馈音
                break;

            case INTERFACE_CAL_BAND://不处理
                // // 频率标定界面长按UP键返回零点标定
                // Calibration_StartStep(CAL_STEP_ZERO);
                // InterfaceManager_SwitchTo(INTERFACE_CAL_ZERO);
                // InterfaceManager_Beep(200);  // 长按反馈音
                break;

            default:
                // 其他界面不处理长按
                break;
        }
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
                InterfaceIndex_t target;
                switch (g_interface_manager.menu_cursor) {
                    case 0: target = INTERFACE_CALIBRATION; break;
                    case 1: target = INTERFACE_STANDARD; break;
                    case 2: target = INTERFACE_ALARM; break;
                    case 3: target = INTERFACE_BRIGHTNESS; break;
                    case 4: target = INTERFACE_ABOUT; break;
                    default: target = INTERFACE_CALIBRATION; break;
                }
                InterfaceManager_SwitchTo(target);
            }
            break;
            
        case INTERFACE_STANDARD:
            // 设置界面按键处理
            if (key == KEY_UP) {
                // 返回菜单
                InterfaceManager_SwitchTo(INTERFACE_MENU);
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
                // 确认当前亮度设置，保存到EEPROM
                BL24C16_Write(0x0000, &g_interface_manager.brightness_level, 1);
                // 返回菜单
                InterfaceManager_SwitchTo(INTERFACE_MENU);
            }
            break;

        case INTERFACE_ALARM:
            // 超限报警界面按键处理
            if (key == KEY_UP) {
                // 返回菜单
                InterfaceManager_SwitchTo(INTERFACE_MENU);
            } else if (key == KEY_DOWN) {
                // 切换选中项（报警开关 → 百位 → 十位 → 个位 → 小数位 → 报警开关）
                g_interface_manager.alarm_selected_item =
                    (g_interface_manager.alarm_selected_item + 1) % 5;
                g_interface_manager.need_refresh = 1;
            } else if (key == KEY_OK) {
                // 调节当前选中的项目
                if (g_interface_manager.alarm_selected_item == 0) {
                    // 切换报警开关
                    g_interface_manager.alarm_enabled = !g_interface_manager.alarm_enabled;
                } else {
                    // 调节VSWR阈值的各个位数（修复浮点精度问题）
                    float current_value = g_interface_manager.vswr_alarm_threshold;
                    // 转换为整数避免浮点精度问题
                    int total_tenths = (int)(current_value * 10.0f + 0.5f);  // 四舍五入到0.1

                    int hundreds = (total_tenths / 1000) % 10;
                    int tens = (total_tenths / 100) % 10;
                    int ones = (total_tenths / 10) % 10;
                    int decimal = total_tenths % 10;

                    switch (g_interface_manager.alarm_selected_item) {
                        case 1: // 百位
                            hundreds = (hundreds + 1) % 10;
                            break;
                        case 2: // 十位
                            tens = (tens + 1) % 10;
                            break;
                        case 3: // 个位
                            ones = (ones + 1) % 10;
                            break;
                        case 4: // 小数位
                            decimal = (decimal + 1) % 10;
                            break;
                    }

                    // 重新计算VSWR值（使用整数计算避免精度问题）
                    int new_total_tenths = hundreds * 1000 + tens * 100 + ones * 10 + decimal;
                    float new_value = (float)new_total_tenths / 10.0f;

                    // 限制范围 1.0-999.9
                    if (new_value < 1.0f) new_value = 1.0f;
                    if (new_value > 999.9f) new_value = 999.9f;

                    g_interface_manager.vswr_alarm_threshold = new_value;
                }
                // 保存报警设置到EEPROM
                BL24C16_Write(0x0001, &g_interface_manager.alarm_enabled, 1);
                BL24C16_Write(0x0002, (uint8_t*)&g_interface_manager.vswr_alarm_threshold, sizeof(float));
                g_interface_manager.need_refresh = 1;
            }
            break;

        case INTERFACE_CALIBRATION:
            // 校准入口界面按键处理
            if (key == KEY_UP) {
                InterfaceManager_SwitchTo(INTERFACE_MENU);
            } else if (key == KEY_OK) {
                // 进入步骤选择界面
                g_calibration_state.selected_step = 0;  // 默认选择完整标定
                InterfaceManager_SwitchTo(INTERFACE_CAL_STEP_SELECT);
            }
            break;

        case INTERFACE_CAL_STEP_SELECT:
            // 步骤选择界面按键处理
            if (key == KEY_UP) {
                InterfaceManager_SwitchTo(INTERFACE_CALIBRATION);
            } else if (key == KEY_DOWN) {
                // 切换选择项 (0→1→2→3→0循环)
                g_calibration_state.selected_step = (g_calibration_state.selected_step + 1) % 4;
                g_interface_manager.need_refresh = 1;
            } else if (key == KEY_OK) {
                // 开始选中的标定类型
                switch (g_calibration_state.selected_step) {
                    case 0:  // 完整标定
                        g_calibration_state.is_single_step = 0;
                        Calibration_StartStep(CAL_STEP_CONFIRM);
                        InterfaceManager_SwitchTo(INTERFACE_CAL_CONFIRM);
                        break;
                    case 1:  // 零点标定
                        g_calibration_state.is_single_step = 1;
                        Calibration_StartStep(CAL_STEP_ZERO);
                        InterfaceManager_SwitchTo(INTERFACE_CAL_ZERO);
                        break;
                    case 2:  // 功率标定
                        g_calibration_state.is_single_step = 1;
                        Calibration_InitPowerStep();  // 初始化功率标定状态
                        Calibration_StartStep(CAL_STEP_POWER);
                        InterfaceManager_SwitchTo(INTERFACE_CAL_POWER);
                        break;
                    case 3:  // 频率标定
                        g_calibration_state.is_single_step = 1;
                        Calibration_InitBandStep();  // 初始化频率标定状态
                        Calibration_StartStep(CAL_STEP_BAND);
                        InterfaceManager_SwitchTo(INTERFACE_CAL_BAND);
                        break;
                }
            }
            break;

        case INTERFACE_CAL_CONFIRM:
            // 校准确认页按键处理
            if (key == KEY_UP) {
                InterfaceManager_SwitchTo(INTERFACE_CAL_STEP_SELECT);
            } else if (key == KEY_OK) {
                // 确认开始校准
                Calibration_StartStep(CAL_STEP_ZERO);
                InterfaceManager_SwitchTo(INTERFACE_CAL_ZERO);
            }
            break;

        case INTERFACE_CAL_ZERO:
            // 零点校准按键处理
            if (key == KEY_UP) {
                InterfaceManager_SwitchTo(INTERFACE_CAL_CONFIRM);
            } else if (key == KEY_OK) {
                // 开始零点采样
                if (g_calibration_state.sample_count == 0) {
                    g_calibration_state.sample_count = 1;  // 开始采样
                    g_calibration_state.sample_sum_fwd = 0.0f;
                    g_calibration_state.sample_sum_ref = 0.0f;
                    g_interface_manager.need_refresh = 1;
                }
            }
            break;

        case INTERFACE_CAL_POWER:
            // 功率校准按键处理
            if (key == KEY_UP) {
                // UP键：上一个校准点
                if (g_calibration_state.current_channel == 0) {  // 正向功率校准
                    if (g_calibration_state.current_power_point > 0) {
                        g_calibration_state.current_power_point--;
                        g_calibration_state.target_power = power_cal_points[g_calibration_state.current_power_point];
                    }
                } else {  // 反射功率校准
                    if (g_calibration_state.current_power_point > 0) {
                        g_calibration_state.current_power_point--;
                        g_calibration_state.target_power = power_cal_points[g_calibration_state.current_power_point];
                    } else {
                        // 从反射第0点返回到正向最后一点
                        g_calibration_state.current_channel = 0;
                        g_calibration_state.current_power_point = 19;
                        g_calibration_state.target_power = power_cal_points[19];
                    }
                }
                g_interface_manager.need_refresh = 1;
            } else if (key == KEY_DOWN) {
                // 切换到下一个校准点或通道
                if (g_calibration_state.current_channel == 0) {  // 正向功率校准
                    if (g_calibration_state.current_power_point < 19) {  // 0-19共20个点
                        g_calibration_state.current_power_point++;
                        g_calibration_state.target_power = power_cal_points[g_calibration_state.current_power_point];
                    } else {
                        // 切换到反射功率校准
                        g_calibration_state.current_channel = 1;
                        g_calibration_state.current_power_point = 0;
                        g_calibration_state.target_power = power_cal_points[0];
                    }
                } else {  // 反射功率校准
                    if (g_calibration_state.current_power_point < 19) {  // 0-19共20个点
                        g_calibration_state.current_power_point++;
                        g_calibration_state.target_power = power_cal_points[g_calibration_state.current_power_point];
                    } else {
                        // 完成功率校准
                        if (g_calibration_state.is_single_step) {
                            // 单步标定完成，返回步骤选择界面
                            InterfaceManager_SwitchTo(INTERFACE_CAL_STEP_SELECT);
                        } else {
                            // 完整标定，进入下一步
                            Calibration_InitBandStep();  // 初始化频率标定状态
                            Calibration_StartStep(CAL_STEP_BAND);
                            InterfaceManager_SwitchTo(INTERFACE_CAL_BAND);
                        }
                        return;
                    }
                }
                g_interface_manager.need_refresh = 1;
            } else if (key == KEY_OK) {
                // 开始当前点的采样
                if (g_calibration_state.sample_count == 0 && g_calibration_state.is_stable) {
                    g_calibration_state.sample_count = 1;  // 开始采样
                    g_calibration_state.sample_sum_fwd = 0.0f;
                    g_calibration_state.sample_sum_ref = 0.0f;
                    g_interface_manager.need_refresh = 1;
                }
            }
            // 长按UP键返回零点标定功能已实现
            break;

        case INTERFACE_CAL_BAND:
            // 频率标定按键处理
            if (key == KEY_UP) {
                // 返回功率校准时重置状态
                Calibration_StartStep(CAL_STEP_POWER);
                InterfaceManager_SwitchTo(INTERFACE_CAL_POWER);
            } else if (key == KEY_DOWN) {
                // 调整标定频率 (+1MHz，循环)
                g_calibration_state.cal_frequency += FREQ_STEP;
                if (g_calibration_state.cal_frequency > MAX_CAL_FREQ) {
                    g_calibration_state.cal_frequency = MIN_CAL_FREQ;  // 循环到最小值
                }
                g_interface_manager.need_refresh = 1;
            } else if (key == KEY_OK) {
                // 开始频率标定采样
                if (g_calibration_state.sample_count == 0 && g_calibration_state.is_stable) {
                    g_calibration_state.sample_count = 1;  // 开始采样
                    g_calibration_state.sample_sum_fwd = 0.0f;
                    g_calibration_state.sample_sum_ref = 0.0f;
                    g_interface_manager.need_refresh = 1;
                }
            }
            break;





        case INTERFACE_CAL_COMPLETE:
            // 校准完成按键处理
            if (key == KEY_UP) {
                InterfaceManager_SwitchTo(INTERFACE_MENU);
            } else if (key == KEY_OK) {
                InterfaceManager_SwitchTo(INTERFACE_MAIN);
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
        g_interface_manager.interface_first_enter = 1;  // 设置首次进入标志
        LCD_Clear(BLACK);  // 清屏

        // 界面切换时的特殊处理
        if (interface == INTERFACE_ALARM) {
            g_interface_manager.alarm_selected_item = 0;  // 重置为选中报警开关
        }
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
    // 功率显示为整数W (0-2000W范围)
    g_power_result.forward_power = forward_power;
    g_power_result.forward_unit = POWER_UNIT_W;

    g_power_result.reflected_power = reflected_power;
    g_power_result.reflected_unit = POWER_UNIT_W;
    
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
    if (vswr >= 999.0f) {
        g_rf_params.vswr_color = VSWR_COLOR_RED;  // 无穷大显示红色
    } else if (vswr <= 1.5f) {
        g_rf_params.vswr_color = VSWR_COLOR_GREEN;
    } else if (vswr <= 2.0f) {
        g_rf_params.vswr_color = VSWR_COLOR_YELLOW;
    } else {
        g_rf_params.vswr_color = VSWR_COLOR_RED;
    }
    
    g_rf_params.is_valid = 1;
    
    // 检查是否需要报警（标定界面不报警）
    if (g_interface_manager.alarm_enabled &&
        vswr > g_interface_manager.vswr_alarm_threshold &&
        g_interface_manager.current_interface != INTERFACE_CAL_ZERO &&
        g_interface_manager.current_interface != INTERFACE_CAL_POWER &&
        g_interface_manager.current_interface != INTERFACE_CAL_BAND) {
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
    // 从定时器中断获取按键值
    uint8_t key = GetKeyValue();

    // 转换按键值：1=UP, 2=DOWN, 3=OK
    switch(key) {
        case 1: return KEY_UP;
        case 2: return KEY_DOWN;
        case 3: return KEY_OK;
        default: return KEY_NONE;
    }
}

/**
 * @brief 设置背光亮度
 */
void InterfaceManager_SetBrightness(uint8_t level)
{
    if (level > 10) level = 10;
    if (level < 1) level = 1;
    g_interface_manager.brightness_level = level;

    // 计算PWM占空比 (10%-100%)，Period=499
    // level=1时约10%占空比(50), level=10时100%占空比(499)
    uint16_t duty = (level * 449) / 10 + 50;  // 50-499范围
    if (duty > 499) duty = 499;  // 限制最大值

    // 设置TIM3 CH3的PWM占空比 (PB0背光控制)
    LCD_SetBacklight(duty-1);
}

/**
 * @brief 触发蜂鸣器（非阻塞方式）
 */
void InterfaceManager_Beep(uint16_t duration_ms)
{
    // 设置蜂鸣器状态
    g_buzzer_state.is_active = 1;
    g_buzzer_state.duration_count = duration_ms;

    // 立即开启蜂鸣器 (PB4)
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
}

/**
 * @brief 蜂鸣器定时处理函数（在TIM3中断中调用）
 * @note TIM3现在是200Hz（每5ms中断一次），所以每次减5ms
 */
void InterfaceManager_BuzzerProcess(void)
{
    if (g_buzzer_state.is_active) {
        if (g_buzzer_state.duration_count > 0) {
            // 每次减5，因为现在是5ms中断一次（200Hz）
            if (g_buzzer_state.duration_count >= 5) {
                g_buzzer_state.duration_count -= 5;
            } else {
                g_buzzer_state.duration_count = 0;
            }
        } else {
            // 时间到，关闭蜂鸣器
            g_buzzer_state.is_active = 0;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);
        }
    }
}

/* ========== 校准功能实现 ========== */

/**
 * @brief 校准系统初始化
 */
void Calibration_Init(void)
{
    // 从EEPROM加载校准数据
    Calibration_LoadFromEEPROM();

    // 重置校准状态
    g_calibration_state.current_step = CAL_STEP_CONFIRM;
    g_calibration_state.cal_frequency = 14.0f;
    g_calibration_state.current_power_point = 0;
    g_calibration_state.current_channel = 0;
    g_calibration_state.sample_count = 0;
    g_calibration_state.sample_sum_fwd = 0.0f;
    g_calibration_state.sample_sum_ref = 0.0f;
    g_calibration_state.is_stable = 0;
    g_calibration_state.stable_count = 0;
    g_calibration_state.target_power = 100.0f;
    g_calibration_state.power_cal_mode = 0;
    g_calibration_state.sample_completed = 0;
}

/**
 * @brief 从EEPROM加载校准数据
 *
 * EEPROM地址布局 (支持0W-2kW功率范围):
 * 0x0010-0x0017: 零点偏移数据 (8字节)
 * 0x0020-0x011F: 正向功率校准表 (20点×8字节=160字节)
 * 0x0120-0x021F: 反射功率校准表 (20点×8字节=160字节)
 * 0x0220-0x0221: 校准点数 (2字节)
 * 0x0300-0x030B: 频率增益修正 (3×4字节=12字节)
 * 0x0400-0x0407: 频率微调和标志 (8字节)
 */
void Calibration_LoadFromEEPROM(void)
{
    // 读取零点偏移
    if (BL24C16_Read(0x0010, (uint8_t*)&g_calibration_data.forward_offset, sizeof(float)) != EEPROM_OK) {
        g_calibration_data.forward_offset = 0.0f;
    }
    if (BL24C16_Read(0x0014, (uint8_t*)&g_calibration_data.reflected_offset, sizeof(float)) != EEPROM_OK) {
        g_calibration_data.reflected_offset = 0.0f;
    }

    // 读取正向功率校准表 (20个点，每个点8字节，共160字节)
    // 分块读取以避免单次读取过多数据
    for (int i = 0; i < 20; i++) {
        uint16_t addr = 0x0020 + i * sizeof(PowerCalPoint_t);
        if (BL24C16_Read(addr, (uint8_t*)&g_calibration_data.fwd_table[i], sizeof(PowerCalPoint_t)) != EEPROM_OK) {
            // 使用默认值 (0V=0W, 2V=2kW的线性关系)
            g_calibration_data.fwd_table[i].power = power_cal_points[i];
            g_calibration_data.fwd_table[i].voltage = power_cal_points[i] / 1000.0f;  // 1000W/V系数
        }
    }

    // 读取反射功率校准表 (地址从0x0120开始，避免与正向表冲突)
    for (int i = 0; i < 20; i++) {
        uint16_t addr = 0x0120 + i * sizeof(PowerCalPoint_t);
        if (BL24C16_Read(addr, (uint8_t*)&g_calibration_data.ref_table[i], sizeof(PowerCalPoint_t)) != EEPROM_OK) {
            // 使用默认值 (0V=0W, 2V=2kW的线性关系)
            g_calibration_data.ref_table[i].power = power_cal_points[i];
            g_calibration_data.ref_table[i].voltage = power_cal_points[i] / 1000.0f;  // 1000W/V系数
        }
    }

    // 读取校准点数 (调整地址避免与校准表冲突)
    if (BL24C16_Read(0x0220, &g_calibration_data.fwd_points, 1) != EEPROM_OK) {
        g_calibration_data.fwd_points = 0;
    }
    if (BL24C16_Read(0x0221, &g_calibration_data.ref_points, 1) != EEPROM_OK) {
        g_calibration_data.ref_points = 0;
    }

    // 读取频率增益修正系数
    if (BL24C16_Read(0x0300, (uint8_t*)&g_calibration_data.freq_gain_fwd, sizeof(float)) != EEPROM_OK) {
        g_calibration_data.freq_gain_fwd = 1.0f;
    }
    if (BL24C16_Read(0x0304, (uint8_t*)&g_calibration_data.freq_gain_ref, sizeof(float)) != EEPROM_OK) {
        g_calibration_data.freq_gain_ref = 1.0f;
    }
    if (BL24C16_Read(0x0308, (uint8_t*)&g_calibration_data.cal_frequency, sizeof(float)) != EEPROM_OK) {
        g_calibration_data.cal_frequency = 14.0f;  // 默认14MHz
    }

    // 验证频率增益数据有效性
    if (g_calibration_data.freq_gain_fwd < 0.1f || g_calibration_data.freq_gain_fwd > 10.0f ||
        isnan(g_calibration_data.freq_gain_fwd) || isinf(g_calibration_data.freq_gain_fwd)) {
        g_calibration_data.freq_gain_fwd = 1.0f;

    }
    if (g_calibration_data.freq_gain_ref < 0.1f || g_calibration_data.freq_gain_ref > 10.0f ||
        isnan(g_calibration_data.freq_gain_ref) || isinf(g_calibration_data.freq_gain_ref)) {
        g_calibration_data.freq_gain_ref = 1.0f;

    }

    // 读取频率微调 (调整地址避免冲突)
    if (BL24C16_Read(0x0400, (uint8_t*)&g_calibration_data.freq_trim, sizeof(float)) != EEPROM_OK) {
        g_calibration_data.freq_trim = 1.0f;
    }

    // 验证freq_trim数据有效性，防止EEPROM损坏
    if (g_calibration_data.freq_trim < 0.1f || g_calibration_data.freq_trim > 10.0f ||
        isnan(g_calibration_data.freq_trim) || isinf(g_calibration_data.freq_trim)) {
        g_calibration_data.freq_trim = 1.0f;  // 重置为安全默认值

    }

    // 读取校准完成标志
    if (BL24C16_Read(0x0404, &g_calibration_data.is_calibrated, 1) != EEPROM_OK) {
        g_calibration_data.is_calibrated = 0;
    }

    // 一次性清除损坏的EEPROM数据（临时修复）
    static uint8_t eeprom_cleared = 0;
    if (!eeprom_cleared) {

        g_calibration_data.freq_trim = 1.0f;
        g_calibration_data.freq_gain_fwd = 1.0f;
        g_calibration_data.freq_gain_ref = 1.0f;
        g_calibration_data.cal_frequency = 14.0f;

        // 保存正确的数据到EEPROM
        BL24C16_Write(0x0400, (uint8_t*)&g_calibration_data.freq_trim, sizeof(float));
        BL24C16_Write(0x0300, (uint8_t*)&g_calibration_data.freq_gain_fwd, sizeof(float));
        BL24C16_Write(0x0304, (uint8_t*)&g_calibration_data.freq_gain_ref, sizeof(float));
        BL24C16_Write(0x0308, (uint8_t*)&g_calibration_data.cal_frequency, sizeof(float));

        eeprom_cleared = 1;

    }


}

/**
 * @brief 保存校准数据到EEPROM
 */
void Calibration_SaveToEEPROM(void)
{
    // 保存零点偏移
    BL24C16_Write(0x0010, (uint8_t*)&g_calibration_data.forward_offset, sizeof(float));
    BL24C16_Write(0x0014, (uint8_t*)&g_calibration_data.reflected_offset, sizeof(float));

    // 保存正向功率校准表 (分块保存以确保可靠性)
    for (int i = 0; i < 20; i++) {
        uint16_t addr = 0x0020 + i * sizeof(PowerCalPoint_t);
        BL24C16_Write(addr, (uint8_t*)&g_calibration_data.fwd_table[i], sizeof(PowerCalPoint_t));
        HAL_Delay(5);  // 写入间隔，确保EEPROM写入完成
    }

    // 保存反射功率校准表 (地址从0x0120开始)
    for (int i = 0; i < 20; i++) {
        uint16_t addr = 0x0120 + i * sizeof(PowerCalPoint_t);
        BL24C16_Write(addr, (uint8_t*)&g_calibration_data.ref_table[i], sizeof(PowerCalPoint_t));
        HAL_Delay(5);  // 写入间隔，确保EEPROM写入完成
    }

    // 保存校准点数 (使用新地址)
    BL24C16_Write(0x0220, &g_calibration_data.fwd_points, 1);
    BL24C16_Write(0x0221, &g_calibration_data.ref_points, 1);

    // 保存频率增益修正系数
    BL24C16_Write(0x0300, (uint8_t*)&g_calibration_data.freq_gain_fwd, sizeof(float));
    BL24C16_Write(0x0304, (uint8_t*)&g_calibration_data.freq_gain_ref, sizeof(float));
    BL24C16_Write(0x0308, (uint8_t*)&g_calibration_data.cal_frequency, sizeof(float));

    // 保存频率微调 (使用新地址)
    BL24C16_Write(0x0400, (uint8_t*)&g_calibration_data.freq_trim, sizeof(float));

    // 保存校准完成标志
    g_calibration_data.is_calibrated = 1;
    BL24C16_Write(0x0404, &g_calibration_data.is_calibrated, 1);
}



/**
 * @brief 查表计算功率（线性插值，优化支持0W-2kW范围）
 */
float Calibration_CalculatePowerFromTable(float voltage, PowerCalPoint_t* table, uint8_t points)
{
    if (points == 0) {
        return 0.0f;  // 无校准数据
    }

    // 处理0W情况：电压接近0时返回0功率
    if (voltage <= 0.001f) {  // 1mV以下认为是0功率
        return 0.0f;
    }

    // 如果电压小于第一个校准点，使用第一个点的斜率外推
    if (voltage <= table[0].voltage) {
        if (points >= 2) {
            float slope = (table[1].power - table[0].power) / (table[1].voltage - table[0].voltage);
            float extrapolated_power = table[0].power + slope * (voltage - table[0].voltage);
            return (extrapolated_power > 0.0f) ? extrapolated_power : 0.0f;  // 确保不返回负功率
        } else {
            return table[0].power * (voltage / table[0].voltage);  // 线性比例
        }
    }

    // 如果电压大于最后一个点，使用最后两个点的斜率外推（支持2kW以上）
    if (voltage >= table[points-1].voltage) {
        if (points >= 2) {
            float slope = (table[points-1].power - table[points-2].power) /
                         (table[points-1].voltage - table[points-2].voltage);
            return table[points-1].power + slope * (voltage - table[points-1].voltage);
        } else {
            return table[points-1].power * (voltage / table[points-1].voltage);
        }
    }

    // 在表格范围内，找到对应区间进行线性插值
    for (uint8_t i = 0; i < points - 1; i++) {
        if (voltage >= table[i].voltage && voltage <= table[i+1].voltage) {
            float ratio = (voltage - table[i].voltage) / (table[i+1].voltage - table[i].voltage);
            return table[i].power + ratio * (table[i+1].power - table[i].power);
        }
    }

    return 0.0f;  // 不应该到达这里
}

/**
 * @brief 应用校准修正（新的查表方式）
 */
float Calibration_ApplyCorrection(float raw_voltage, uint8_t is_forward, float frequency)
{
    if (!g_calibration_data.is_calibrated) {
        // 未校准时使用简单线性转换 (0V=0W, 2V=2kW)
        return (raw_voltage - (is_forward ? g_calibration_data.forward_offset : g_calibration_data.reflected_offset)) * 1000.0f;
    }

    // 去除零点偏移
    float corrected_voltage = raw_voltage - (is_forward ? g_calibration_data.forward_offset : g_calibration_data.reflected_offset);
    if (corrected_voltage < 0.0f) {
        corrected_voltage = 0.0f;
    }

    // 查表计算功率
    float base_power;
    if (is_forward) {
        base_power = Calibration_CalculatePowerFromTable(corrected_voltage, g_calibration_data.fwd_table, g_calibration_data.fwd_points);
    } else {
        base_power = Calibration_CalculatePowerFromTable(corrected_voltage, g_calibration_data.ref_table, g_calibration_data.ref_points);
    }

    // 应用频率增益修正
    float final_power = base_power * (is_forward ? g_calibration_data.freq_gain_fwd : g_calibration_data.freq_gain_ref);

    return (final_power > 0.0f) ? final_power : 0.0f;
}

/**
 * @brief 计算功率（简化接口，自动获取当前频率）
 */
float Calibration_CalculatePower(float voltage, uint8_t is_forward)
{
    // 临时调试：直接调用查表函数，跳过频率修正
    if (!g_calibration_data.is_calibrated) {
        // 未校准时使用简单线性转换 (0V=0W, 2V=2kW)
        return (voltage - (is_forward ? g_calibration_data.forward_offset : g_calibration_data.reflected_offset)) * 1000.0f;
    }

    // 去除零点偏移
    float corrected_voltage = voltage - (is_forward ? g_calibration_data.forward_offset : g_calibration_data.reflected_offset);
    if (corrected_voltage < 0.0f) {
        corrected_voltage = 0.0f;
    }

    // 直接查表计算功率，暂时不应用频率增益修正
    float base_power;
    if (is_forward) {
        base_power = Calibration_CalculatePowerFromTable(corrected_voltage, g_calibration_data.fwd_table, g_calibration_data.fwd_points);
    } else {
        base_power = Calibration_CalculatePowerFromTable(corrected_voltage, g_calibration_data.ref_table, g_calibration_data.ref_points);
    }

    return (base_power > 0.0f) ? base_power : 0.0f;
}

/**
 * @brief 开始校准步骤
 */
void Calibration_StartStep(CalibrationStep_t step)
{
    g_calibration_state.current_step = step;
    g_calibration_state.sample_count = 0;
    g_calibration_state.sample_sum_fwd = 0.0f;
    g_calibration_state.sample_sum_ref = 0.0f;
    g_calibration_state.sample_completed = 0;  // 重置完成标志
    g_calibration_state.is_stable = 0;
    g_calibration_state.stable_count = 0;

    g_interface_manager.need_refresh = 1;
}

/**
 * @brief 初始化功率标定（只在开始时调用一次）
 */
void Calibration_InitPowerStep(void)
{
    g_calibration_state.current_power_point = 0;
    g_calibration_state.current_channel = 0;  // 从正向功率开始
    g_calibration_state.target_power = power_cal_points[0];  // 100W
    g_calibration_state.power_cal_mode = 0;
}

/**
 * @brief 初始化频率标定（只在开始时调用一次）
 */
void Calibration_InitBandStep(void)
{
    g_calibration_state.cal_frequency = 14.0f;  // 默认14MHz
    g_calibration_state.target_power = 100.0f;  // 频率标定使用固定功率
}

/**
 * @brief 读取ADC电压值
 */
static void Calibration_ReadADCVoltage(float* fwd_voltage, float* ref_voltage)
{
    // 启动ADC转换并读取值
    extern ADC_HandleTypeDef hadc1;

    // 配置并读取Channel 2 (PA2正向功率)
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_2;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint32_t adc_forward = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    // 配置并读取Channel 3 (PA3反射功率)
    sConfig.Channel = ADC_CHANNEL_3;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint32_t adc_reflected = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    // 转换为电压值 (2.5V参考电压，12位ADC)
    *fwd_voltage = (float)adc_forward * 2.5f / 4095.0f;
    *ref_voltage = (float)adc_reflected * 2.5f / 4095.0f;
}

/**
 * @brief 显示ADC调试信息（用于校准界面）
 * @param x: X坐标位置
 * @param y: Y坐标起始位置
 */
static void Display_ADCInfo(uint16_t x, uint16_t y)
{
    char str_buffer[16];
    uint32_t adc_forward, adc_reflected;
    float fwd_voltage, ref_voltage;

    // 启动ADC转换并读取值
    extern ADC_HandleTypeDef hadc1;

    // 配置并读取Channel 2 (PA2正向功率)
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_2;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    adc_forward = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    // 配置并读取Channel 3 (PA3反射功率)
    sConfig.Channel = ADC_CHANNEL_3;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    adc_reflected = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    // 转换为电压值 (2.5V参考电压，12位ADC)
    fwd_voltage = (float)adc_forward * 2.5f / 4095.0f;
    ref_voltage = (float)adc_reflected * 2.5f / 4095.0f;

    // 显示ADC原始值
    sprintf(str_buffer, "F:%4d", (int)adc_forward);
    Show_Str(x, y, GRAY, BLACK, (uint8_t*)str_buffer, 12, 0);

    sprintf(str_buffer, "R:%4d", (int)adc_reflected);
    Show_Str(x, y + 15, GRAY, BLACK, (uint8_t*)str_buffer, 12, 0);

    // 显示电压值
    sprintf(str_buffer, "%5.2fV", fwd_voltage);
    Show_Str(x, y + 30, GRAY, BLACK, (uint8_t*)str_buffer, 12, 0);

    sprintf(str_buffer, "%5.2fV", ref_voltage);
    Show_Str(x, y + 45, GRAY, BLACK, (uint8_t*)str_buffer, 12, 0);
}

/**
 * @brief 处理校准采样
 */
void Calibration_ProcessSample(void)
{
    static float last_fwd = 0.0f;
    static float last_ref = 0.0f;

    // 获取当前ADC电压值
    float current_fwd, current_ref;
    Calibration_ReadADCVoltage(&current_fwd, &current_ref);

    // 稳定性检测（变化小于5%认为稳定）
    if (fabs(current_fwd - last_fwd) < 0.05f * last_fwd &&
        fabs(current_ref - last_ref) < 0.05f * last_ref) {
        g_calibration_state.stable_count++;
        if (g_calibration_state.stable_count >= 10) {  // 稳定1秒（100ms×10）
            g_calibration_state.is_stable = 1;
        }
    } else {
        g_calibration_state.stable_count = 0;
        g_calibration_state.is_stable = 0;
    }

    last_fwd = current_fwd;
    last_ref = current_ref;

    // 如果正在采样，累计数据
    if (g_calibration_state.sample_count > 0) {
        g_calibration_state.sample_sum_fwd += current_fwd;
        g_calibration_state.sample_sum_ref += current_ref;
        g_calibration_state.sample_count++;

        // 采样完成（10次）
        if (g_calibration_state.sample_count >= 10) {
            // 修复：实际采样次数是sample_count-1次（从1开始计数）
            uint8_t actual_samples = g_calibration_state.sample_count - 1;
            float avg_fwd = g_calibration_state.sample_sum_fwd / (float)actual_samples;
            float avg_ref = g_calibration_state.sample_sum_ref / (float)actual_samples;

            printf("Actual samples taken: %d\r\n", actual_samples);

            // 设置完成标志，显示完成提示
            g_calibration_state.sample_completed = 1;
            g_interface_manager.need_refresh = 1;

            // 串口调试输出采样结果
            printf("\r\n=== Calibration Sample Complete ===\r\n");
            printf("Step: %d\r\n", g_calibration_state.current_step);
            printf("Average Forward: %.3fV\r\n", avg_fwd);
            printf("Average Reflected: %.3fV\r\n", avg_ref);

            // 根据当前步骤处理采样结果
            switch (g_calibration_state.current_step) {
                case CAL_STEP_ZERO:
                    g_calibration_data.forward_offset = avg_fwd;
                    g_calibration_data.reflected_offset = avg_ref;

                    printf("Zero calibration: Fwd=%.3fV, Ref=%.3fV\r\n", avg_fwd, avg_ref);

                    // 保存零点数据到EEPROM
                    Calibration_SaveToEEPROM();

                    InterfaceManager_Beep(200);  // 成功提示音

                    // 延迟后进入下一步或返回选择界面
                    HAL_Delay(1000);  // 显示完成提示1秒
                    if (g_calibration_state.is_single_step) {
                        // 单步标定完成，返回步骤选择界面
                        InterfaceManager_SwitchTo(INTERFACE_CAL_STEP_SELECT);
                    } else {
                        // 完整标定，继续下一步：功率校准
                        Calibration_InitPowerStep();  // 初始化功率标定状态
                        Calibration_StartStep(CAL_STEP_POWER);
                        InterfaceManager_SwitchTo(INTERFACE_CAL_POWER);
                    }
                    break;

                case CAL_STEP_POWER:
                    {
                        // 多点功率校准
                        uint8_t point = g_calibration_state.current_power_point;
                        uint8_t channel = g_calibration_state.current_channel;

                        if (channel == 0) {  // 正向功率校准
                            float corrected_voltage = avg_fwd - g_calibration_data.forward_offset;
                            g_calibration_data.fwd_table[point].power = g_calibration_state.target_power;
                            g_calibration_data.fwd_table[point].voltage = corrected_voltage;
                            g_calibration_data.fwd_points = point + 1;

                            printf("Forward Power Cal P%d: %.0fW @ %.3fV (Raw=%.3fV, Offset=%.3fV)\r\n",
                                   point, g_calibration_state.target_power, corrected_voltage, avg_fwd, g_calibration_data.forward_offset);
                        } else {  // 反射功率校准
                            float corrected_voltage = avg_ref - g_calibration_data.reflected_offset;
                            g_calibration_data.ref_table[point].power = g_calibration_state.target_power;
                            g_calibration_data.ref_table[point].voltage = corrected_voltage;
                            g_calibration_data.ref_points = point + 1;

                            printf("Reflected Power Cal P%d: %.0fW @ %.3fV (Raw=%.3fV, Offset=%.3fV)\r\n",
                                   point, g_calibration_state.target_power, corrected_voltage, avg_ref, g_calibration_data.reflected_offset);
                        }
                        InterfaceManager_Beep(200);

                        // 保存当前标定点到EEPROM
                        Calibration_SaveToEEPROM();

                        // 延迟显示完成提示
                        HAL_Delay(800);  // 显示完成提示0.8秒

                        // 自动进入下一个校准点
                        if (channel == 0) {  // 正向功率校准
                            if (point < 19) {  // 还有更多正向点
                                g_calibration_state.current_power_point++;
                                g_calibration_state.target_power = power_cal_points[g_calibration_state.current_power_point];
                                Calibration_StartStep(CAL_STEP_POWER);  // 重置采样状态
                            } else {
                                // 切换到反射功率校准
                                g_calibration_state.current_channel = 1;
                                g_calibration_state.current_power_point = 0;
                                g_calibration_state.target_power = power_cal_points[0];
                                Calibration_StartStep(CAL_STEP_POWER);  // 重置采样状态
                            }
                        } else {  // 反射功率校准
                            if (point < 19) {  // 还有更多反射点
                                g_calibration_state.current_power_point++;
                                g_calibration_state.target_power = power_cal_points[g_calibration_state.current_power_point];
                                Calibration_StartStep(CAL_STEP_POWER);  // 重置采样状态
                            } else {
                                // 完成功率校准
                                if (g_calibration_state.is_single_step) {
                                    // 单步标定完成，返回步骤选择界面
                                    InterfaceManager_SwitchTo(INTERFACE_CAL_STEP_SELECT);
                                } else {
                                    // 完整标定，进入下一步
                                    Calibration_InitBandStep();  // 初始化频率标定状态
                                    Calibration_StartStep(CAL_STEP_BAND);
                                    InterfaceManager_SwitchTo(INTERFACE_CAL_BAND);
                                }
                                return;
                            }
                        }
                    }
                    break;

                case CAL_STEP_BAND:
                    {
                        // 使用查表方式计算基准功率
                        float base_fwd = Calibration_CalculatePowerFromTable(avg_fwd - g_calibration_data.forward_offset,
                                                                            g_calibration_data.fwd_table,
                                                                            g_calibration_data.fwd_points);
                        float base_ref = Calibration_CalculatePowerFromTable(avg_ref - g_calibration_data.reflected_offset,
                                                                            g_calibration_data.ref_table,
                                                                            g_calibration_data.ref_points);

                        // 计算频率增益修正系数
                        if (base_fwd > 0.0f) {
                            g_calibration_data.freq_gain_fwd = g_calibration_state.target_power / base_fwd;
                        }
                        if (base_ref > 0.0f) {
                            g_calibration_data.freq_gain_ref = g_calibration_state.target_power / base_ref;
                        }

                        // 保存标定频率
                        g_calibration_data.cal_frequency = g_calibration_state.cal_frequency;

                        // 计算频率微调系数
                        extern FreqResult_t g_freq_result;
                        if (g_freq_result.is_valid) {
                            // 当前测量的原始频率（Hz），使用当前的微调系数
                            float current_measured_hz = (float)g_freq_result.frequency_hz * 16.0f * g_calibration_data.freq_trim;
                            // 目标频率（Hz）
                            float target_freq_hz = g_calibration_state.cal_frequency * 1000000.0f;
                            // 计算新的微调系数：当前微调 × (目标频率 / 当前显示频率)
                            if (current_measured_hz > 0.0f) {
                                float correction_factor = target_freq_hz / current_measured_hz;
                                g_calibration_data.freq_trim = g_calibration_data.freq_trim * correction_factor;

                            }
                        }

                        InterfaceManager_Beep(200);

                        // 延迟显示完成提示
                        HAL_Delay(1000);  // 显示完成提示1秒

                        // 频率标定完成，保存数据
                        Calibration_SaveToEEPROM();
                        if (g_calibration_state.is_single_step) {
                            // 单步标定完成，返回步骤选择界面
                            InterfaceManager_SwitchTo(INTERFACE_CAL_STEP_SELECT);
                        } else {
                            // 完整标定完成
                            Calibration_StartStep(CAL_STEP_COMPLETE);
                            InterfaceManager_SwitchTo(INTERFACE_CAL_COMPLETE);
                        }
                    }
                    break;



                default:
                    break;
            }

            // 重置采样状态
            g_calibration_state.sample_count = 0;
            g_calibration_state.sample_sum_fwd = 0.0f;
            g_calibration_state.sample_sum_ref = 0.0f;
            g_interface_manager.need_refresh = 1;
        }
    }
}

/* ========== 校准界面显示函数 ========== */

/**
 * @brief 显示校准确认页
 */
void Interface_DisplayCalConfirm(void)
{
    Show_Str(30, 5, WHITE, BLACK, (uint8_t*)"Calibration", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(10, 30, YELLOW, BLACK, (uint8_t*)"WARNING:", 16, 0);
    Show_Str(10, 50, WHITE, BLACK, (uint8_t*)"This will reset", 12, 0);
    Show_Str(10, 65, WHITE, BLACK, (uint8_t*)"all cal data!", 12, 0);

    Show_Str(10, 80, CYAN, BLACK, (uint8_t*)"Prepare:", 12, 0);
    Show_Str(10, 95, WHITE, BLACK, (uint8_t*)"50ohm load ready", 12, 0);

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"OK:Start UP:Cancel", 12, 0);
}

/**
 * @brief 显示零点校准
 */
void Interface_DisplayCalZero(void)
{
    // 首次进入时清屏，防止残留
    if (g_interface_manager.interface_first_enter) {
        g_interface_manager.interface_first_enter = 0;
        LCD_Clear(BLACK);
    }
    Show_Str(35, 5, WHITE, BLACK, (uint8_t*)"Zero Cal", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(10, 30, CYAN, BLACK, (uint8_t*)"Step 1/3:", 12, 0);
    Show_Str(10, 45, WHITE, BLACK, (uint8_t*)"Turn OFF RF", 16, 0);
    Show_Str(10, 65, WHITE, BLACK, (uint8_t*)"No signal input", 12, 0);

    if (g_calibration_state.sample_completed) {
        Show_Str(10, 92, GREEN, BLACK, (uint8_t*)"COMPLETED!      ", 12, 0);
    } else if (g_calibration_state.sample_count > 0) {
        Show_Str(10, 92, YELLOW, BLACK, (uint8_t*)"Sampling...     ", 12, 0);
        char progress[16];
        sprintf(progress, "%2d/10 ", g_calibration_state.sample_count);
        Show_Str(100, 92, YELLOW, BLACK, (uint8_t*)progress, 12, 0);
    } else {
        Show_Str(10, 92, GREEN, BLACK, (uint8_t*)"Ready to sample ", 12, 0);
    }

    // 显示ADC调试信息
    Display_ADCInfo(115, 30);



    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"OK:Sample UP:Back", 12, 0);
}

/**
 * @brief 显示功率校准
 */
void Interface_DisplayCalPower(void)
{
    char str_buffer[32];

    Show_Str(30, 5, WHITE, BLACK, (uint8_t*)"Power Cal", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(10, 30, CYAN, BLACK, (uint8_t*)"Step 2/3:", 12, 0);

    // 显示当前校准通道和点
    if (g_calibration_state.current_channel == 0) {
        sprintf(str_buffer, "FWD Point %2d/20 ", g_calibration_state.current_power_point + 1);
    } else {
        sprintf(str_buffer, "REF Point %2d/20 ", g_calibration_state.current_power_point + 1);
    }
    Show_Str(10, 45, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    // 显示当前校准点信息
    const char* channel_name = (g_calibration_state.current_channel == 0) ? "Fwd" : "Ref";
    sprintf(str_buffer, "Point %2d/20 (%s)  ", g_calibration_state.current_power_point + 1, channel_name);
    Show_Str(10, 60, CYAN, BLACK, (uint8_t*)str_buffer, 12, 0);

    // 显示目标功率，支持kW单位，固定宽度防止残留
    if (g_calibration_state.target_power >= 1000.0f) {
        sprintf(str_buffer, "Target: %4.1fkW  ", g_calibration_state.target_power / 1000.0f);
    } else {
        sprintf(str_buffer, "Target: %4.0fW   ", g_calibration_state.target_power);
    }
    Show_Str(10, 75, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    if (g_calibration_state.is_stable) {
        Show_Str(10, 90, GREEN, BLACK, (uint8_t*)"STABLE - Ready   ", 12, 0);
    } else {
        Show_Str(10, 90, YELLOW, BLACK, (uint8_t*)"Set power & wait ", 12, 0);
    }

    if (g_calibration_state.sample_completed) {
        Show_Str(10, 103, GREEN, BLACK, (uint8_t*)"COMPLETED!      ", 12, 0);
    } else if (g_calibration_state.sample_count > 0) {
        Show_Str(10, 103, YELLOW, BLACK, (uint8_t*)"Sampling...     ", 12, 0);
        char progress[16];
        sprintf(progress, "%2d/10 ", g_calibration_state.sample_count);
        Show_Str(100, 103, YELLOW, BLACK, (uint8_t*)progress, 12, 0);
    }

    // 显示ADC调试信息
    Display_ADCInfo(119, 30);

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Prev DOWN:Next OK:Cal", 12, 0);
}

/**
 * @brief 显示频率标定
 */
void Interface_DisplayCalBand(void)
{
    char str_buffer[32];

    Show_Str(35, 5, WHITE, BLACK, (uint8_t*)"Freq Cal", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(10, 30, CYAN, BLACK, (uint8_t*)"Step 3/3:", 12, 0);

    // 显示标定频率
    sprintf(str_buffer, "Cal: %5.1f MHz    ", g_calibration_state.cal_frequency);
    Show_Str(10, 45, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    // 显示实际测量频率（应用16倍补偿和微调）
    extern FreqResult_t g_freq_result;
    if (g_freq_result.is_valid) {
        // 计算真实频率：原始值 × 16倍补偿 × 微调系数
        float real_freq_hz = (float)g_freq_result.frequency_hz * 16.0f * g_calibration_data.freq_trim;

        if (real_freq_hz >= 1000000.0f) {  // >= 1MHz
            sprintf(str_buffer, "Meas: %5.6f MHz   ", real_freq_hz / 1000000.0f);
        } else if (real_freq_hz >= 1000.0f) {  // >= 1kHz
            sprintf(str_buffer, "Meas: %5.3f kHz   ", real_freq_hz / 1000.0f);
        } else {  // < 1kHz
            sprintf(str_buffer, "Meas: %5.0f Hz    ", real_freq_hz);
        }
    } else {
        sprintf(str_buffer, "Meas: No Signal   ");
    }
    Show_Str(10, 60, CYAN, BLACK, (uint8_t*)str_buffer, 12, 0);



    if (g_calibration_state.is_stable) {
        Show_Str(10, 75, GREEN, BLACK, (uint8_t*)"STABLE - Ready   ", 12, 0);
    } else {
        Show_Str(10, 75, YELLOW, BLACK, (uint8_t*)"Set freq & power ", 12, 0);
    }

    if (g_calibration_state.sample_completed) {
        Show_Str(10, 88, GREEN, BLACK, (uint8_t*)"COMPLETED!      ", 12, 0);
    } else if (g_calibration_state.sample_count > 0) {
        Show_Str(10, 88, YELLOW, BLACK, (uint8_t*)"Sampling...     ", 12, 0);
        char progress[16];
        sprintf(progress, "%2d/10 ", g_calibration_state.sample_count);
        Show_Str(100, 88, YELLOW, BLACK, (uint8_t*)progress, 12, 0);
    }

    // 显示ADC调试信息
    //Display_ADCInfo(122, 30);

    Show_Str(5, 103, GRAY, BLACK, (uint8_t*)"DOWN:Freq(1-50MHz)   ", 12, 0);
    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"OK:Cal UP:Back       ", 12, 0);
}



/**
 * @brief 显示校准完成
 */
void Interface_DisplayCalComplete(void)
{
    Show_Str(25, 5, WHITE, BLACK, (uint8_t*)"Cal Complete", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(50, 40, GREEN, BLACK, (uint8_t*)"SUCCESS!", 16, 0);
    Show_Str(10, 60, WHITE, BLACK, (uint8_t*)"All data saved", 12, 0);
    Show_Str(10, 75, WHITE, BLACK, (uint8_t*)"to EEPROM", 12, 0);

    Show_Str(10, 90, CYAN, BLACK, (uint8_t*)"Cal status: ON", 12, 0);

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"OK:Main UP:Menu", 12, 0);
}

/**
 * @brief 主界面显示
 */
void Interface_DisplayMain(void)
{
    char str_buffer[32];
    uint16_t vswr_color;

    // 显示标题
    Show_Str(20, 5, WHITE, BLACK, (uint8_t*)"RF Power Meter", 16, 0); //射频功率计标题

    // 绘制分割线
    LCD_DrawLine(0, 25, LCD_W - 1, 25); //水平分割线
    LCD_DrawLine(80, 25, 80, 95); //垂直分割线

    // 左侧显示功率信息
    Show_Str(5, 30, CYAN, BLACK, (uint8_t*)"Forward:", 12, 0); //正向功率标签
    if (g_power_result.is_valid) {
        sprintf(str_buffer, "%5.0fW", g_power_result.forward_power);
        Show_Str(5, 45, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //正向功率数值
    } else {
        Show_Str(5, 45, GRAY, BLACK, (uint8_t*)"   --W", 12, 0); //正向功率无效显示
    }

    Show_Str(5, 60, CYAN, BLACK, (uint8_t*)"Reflect:", 12, 0); //反射功率标签
    if (g_power_result.is_valid) {
        sprintf(str_buffer, "%5.0fW", g_power_result.reflected_power);
        Show_Str(5, 75, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //反射功率数值
    } else {
        Show_Str(5, 75, GRAY, BLACK, (uint8_t*)"   --W", 12, 0); //反射功率无效显示
    }

    // 右侧显示射频参数
    Show_Str(85, 30, CYAN, BLACK, (uint8_t*)"VSWR:", 12, 0); //驻波比标签
    if (g_rf_params.is_valid) {
        // 根据VSWR值选择颜色
        switch (g_rf_params.vswr_color) {
            case VSWR_COLOR_GREEN:  vswr_color = GREEN; break;
            case VSWR_COLOR_YELLOW: vswr_color = YELLOW; break;
            case VSWR_COLOR_RED:    vswr_color = RED; break;
            default:                vswr_color = WHITE; break;
        }

        // 先清空显示区域
        //Show_Str(85, 45, BLACK, BLACK, (uint8_t*)"      ", 12, 0);

        // 特殊处理无穷大情况，添加滞回防止闪烁
        static uint8_t vswr_is_inf = 0;
        if (g_rf_params.vswr >= 999.0f) {
            vswr_is_inf = 1;  // 设置无穷大标志
        } else if (g_rf_params.vswr <= 50.0f) {
            vswr_is_inf = 0;  // 只有当VSWR降到50以下才清除标志
        }

        if (vswr_is_inf) {
            sprintf(str_buffer, "%6s", "INF");  // 显示无穷大，固定宽度
        } else {
            sprintf(str_buffer, "%6.2f", g_rf_params.vswr);  // 固定宽度6字符
        }
        Show_Str(85, 45, vswr_color, BLACK, (uint8_t*)str_buffer, 12, 0); //驻波比数值(带颜色)
    } else {
        Show_Str(85, 45, GRAY, BLACK, (uint8_t*)"    --", 12, 0); //驻波比无效显示
    }

    Show_Str(85, 60, CYAN, BLACK, (uint8_t*)"Refl.Coef:", 12, 0); //反射系数标签
    if (g_rf_params.is_valid) {
        sprintf(str_buffer, "%5.3f", g_rf_params.reflection_coeff);
        Show_Str(85, 75, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //反射系数数值
    } else {
        Show_Str(85, 75, GRAY, BLACK, (uint8_t*)"   --", 12, 0); //反射系数无效显示
    }

    // 底部显示传输效率和频率
    LCD_DrawLine(0, 95, LCD_W - 1, 95); //底部分割线

    Show_Str(5, 100, CYAN, BLACK, (uint8_t*)"Efficiency:", 12, 0); //传输效率标签
    if (g_rf_params.is_valid) {
        sprintf(str_buffer, "%5.1f%%", g_rf_params.transmission_eff);
        Show_Str(70, 100, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //传输效率数值
    } else {
        Show_Str(70, 100, GRAY, BLACK, (uint8_t*)"   --%", 12, 0); //传输效率无效显示
    }

    Show_Str(5, 115, CYAN, BLACK, (uint8_t*)"Freq:", 12, 0); //频率标签
    FreqResult_t freq_result;
    if (FreqCounter_GetResult(&freq_result) == 0) {
        // 使用原始frequency_hz值，应用16倍补偿和频率微调
        float freq_hz = (float)freq_result.frequency_hz * 16.0f * g_calibration_data.freq_trim;  // 实际频率(Hz)

        if (freq_hz >= 1000000.0f) {  // >= 1MHz
            sprintf(str_buffer, "%4.6f MHz  ", freq_hz / 1000000.0f);
        } else if (freq_hz >= 1000.0f) {  // >= 1kHz，显示为kHz，3位小数
            sprintf(str_buffer, "%4.3f kHz    ", freq_hz / 1000.0f);
        } else {  // < 1kHz
            sprintf(str_buffer, "%4.0f Hz     ", freq_hz);
        }
        Show_Str(40, 115, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //频率数值
    } else {
        Show_Str(40, 115, GRAY, BLACK, (uint8_t*)"      -- Hz", 12, 0); //频率无效显示
    }
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
    Show_Str(40, 5, WHITE, BLACK, (uint8_t*)"Settings Menu", 16, 0); //设置菜单标题
    LCD_DrawLine(0, 25, LCD_W - 1, 25); //标题分割线

    // 显示菜单项
    for (int i = 0; i < MAX_MENU_ITEMS - 1; i++) {
        uint16_t color = (i == g_interface_manager.menu_cursor) ? YELLOW : WHITE;
        uint16_t bg_color = (i == g_interface_manager.menu_cursor) ? BLUE : BLACK;

        sprintf(str_buffer, "> %s", menu_items[i]);
        Show_Str(10, 28 + i * 18, color, bg_color, (uint8_t*)str_buffer, 16, 0); //菜单项(当前选中为黄色背景蓝色)
    }

    // 显示操作提示
    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back DN:Next OK:Enter", 12, 0); //操作提示(已注释)
}

/**
 * @brief 校准界面显示（入口页面）
 */
void Interface_DisplayCalibration(void)
{
    Show_Str(40, 5, WHITE, BLACK, (uint8_t*)"Calibration", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(10, 30, CYAN, BLACK, (uint8_t*)"Cal Status:", 12, 0);
    if (g_calibration_data.is_calibrated) {
        Show_Str(10, 45, GREEN, BLACK, (uint8_t*)"CALIBRATED", 16, 0);
    } else {
        Show_Str(10, 45, RED, BLACK, (uint8_t*)"NOT CALIBRATED", 16, 0);
    }

    Show_Str(10, 65, WHITE, BLACK, (uint8_t*)"5-Step Wizard:", 12, 0);
    Show_Str(10, 80, WHITE, BLACK, (uint8_t*)"Zero->Power->Band", 12, 0);
    Show_Str(10, 95, WHITE, BLACK, (uint8_t*)"->Reflect->Freq", 12, 0);

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"OK:Start UP:Back", 12, 0);
}

/**
 * @brief 标定步骤选择界面显示
 */
void Interface_DisplayCalStepSelect(void)
{
    // 首次进入时清屏，防止残留
    if (g_interface_manager.interface_first_enter) {
        g_interface_manager.interface_first_enter = 0;
        LCD_Clear(BLACK);
    }

    Show_Str(25, 5, WHITE, BLACK, (uint8_t*)"Cal Select", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    // 显示选项，当前选中项高亮显示
    uint16_t sel_color = GREEN;
    uint16_t normal_color = WHITE;

    Show_Str(10, 35, (g_calibration_state.selected_step == 0) ? sel_color : normal_color,
             BLACK, (uint8_t*)"0. Full Cal", 12, 0);
    Show_Str(10, 50, (g_calibration_state.selected_step == 1) ? sel_color : normal_color,
             BLACK, (uint8_t*)"1. Zero Cal", 12, 0);
    Show_Str(10, 65, (g_calibration_state.selected_step == 2) ? sel_color : normal_color,
             BLACK, (uint8_t*)"2. Power Cal", 12, 0);
    Show_Str(10, 80, (g_calibration_state.selected_step == 3) ? sel_color : normal_color,
             BLACK, (uint8_t*)"3. Freq Cal", 12, 0);

    // 显示当前标定状态
    Show_Str(10, 100, CYAN, BLACK, (uint8_t*)"Status:", 12, 0);
    if (g_calibration_data.is_calibrated) {
        Show_Str(60, 100, GREEN, BLACK, (uint8_t*)"CALIBRATED", 12, 0);
    } else {
        Show_Str(60, 100, RED, BLACK, (uint8_t*)"NOT CAL", 12, 0);
    }

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back DOWN:Sel OK:Start", 12, 0);
}

/**
 * @brief 标定界面显示
 */
void Interface_DisplayStandard(void)
{
    char str_buffer[32];

    Show_Str(40, 5, WHITE, BLACK, (uint8_t*)"Cal Debug", 16, 0); //校准调试标题
    LCD_DrawLine(0, 25, LCD_W - 1, 25); //标题分割线

    // 获取当前ADC值和电压进行实时调试
    extern ADC_HandleTypeDef hadc1;
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_2;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint32_t adc_fwd = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    float raw_voltage = (float)adc_fwd * 2.5f / 4095.0f;
    float corrected_voltage = raw_voltage - g_calibration_data.forward_offset;
    if (corrected_voltage < 0.0f) corrected_voltage = 0.0f;

    // 串口调试输出
    printf("\r\n=== Calibration Debug Info ===\r\n");
    printf("ADC Raw Value: %d\r\n", (int)adc_fwd);
    printf("Raw Voltage: %.3fV\r\n", raw_voltage);
    printf("Forward Offset: %.3fV\r\n", g_calibration_data.forward_offset);
    printf("Corrected Voltage: %.3fV\r\n", corrected_voltage);
    printf("Is Calibrated: %s\r\n", g_calibration_data.is_calibrated ? "YES" : "NO");
    printf("Forward Points: %d/20\r\n", g_calibration_data.fwd_points);
    printf("Reflected Points: %d/20\r\n", g_calibration_data.ref_points);

    // 显示查表结果和校准点信息
    if (g_calibration_data.fwd_points > 0) {
        float table_power = Calibration_CalculatePowerFromTable(corrected_voltage, g_calibration_data.fwd_table, g_calibration_data.fwd_points);
        printf("Table Lookup Result: %.0fW\r\n", table_power);

        // 显示前几个校准点
        printf("Calibration Points (First 5):\r\n");
        for (int i = 0; i < 5 && i < g_calibration_data.fwd_points; i++) {
            printf("  P%d: %.0fW @ %.3fV\r\n", i, g_calibration_data.fwd_table[i].power, g_calibration_data.fwd_table[i].voltage);
        }

        // 显示最后几个校准点
        if (g_calibration_data.fwd_points > 5) {
            printf("Calibration Points (Last 5):\r\n");
            int start = (g_calibration_data.fwd_points > 5) ? g_calibration_data.fwd_points - 5 : 0;
            for (int i = start; i < g_calibration_data.fwd_points; i++) {
                printf("  P%d: %.0fW @ %.3fV\r\n", i, g_calibration_data.fwd_table[i].power, g_calibration_data.fwd_table[i].voltage);
            }
        }

        // 查找1.0V附近的校准点
        printf("Points near 1.0V:\r\n");
        for (int i = 0; i < g_calibration_data.fwd_points; i++) {
            if (g_calibration_data.fwd_table[i].voltage >= 0.8f && g_calibration_data.fwd_table[i].voltage <= 1.2f) {
                printf("  P%d: %.0fW @ %.3fV (MATCH!)\r\n", i, g_calibration_data.fwd_table[i].power, g_calibration_data.fwd_table[i].voltage);
            }
        }
    } else {
        printf("No calibration data found!\r\n");
    }
    printf("==============================\r\n\r\n");

    // 显示实时调试信息到屏幕
    sprintf(str_buffer, "ADC:%4d Raw:%4.3f", (int)adc_fwd, raw_voltage);
    Show_Str(10, 30, CYAN, BLACK, (uint8_t*)str_buffer, 12, 0);

    sprintf(str_buffer, "Off:%4.3f Cor:%4.3f", g_calibration_data.forward_offset, corrected_voltage);
    Show_Str(10, 45, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    sprintf(str_buffer, "Cal:%c Pts:%2d/20", g_calibration_data.is_calibrated ? 'Y' : 'N', g_calibration_data.fwd_points);
    Show_Str(10, 60, g_calibration_data.is_calibrated ? GREEN : RED, BLACK, (uint8_t*)str_buffer, 12, 0);

    if (g_calibration_data.fwd_points > 0) {
        float table_power = Calibration_CalculatePowerFromTable(corrected_voltage, g_calibration_data.fwd_table, g_calibration_data.fwd_points);
        sprintf(str_buffer, "Result:%5.0fW", table_power);
        Show_Str(10, 75, YELLOW, BLACK, (uint8_t*)str_buffer, 12, 0);
    } else {
        Show_Str(10, 75, GRAY, BLACK, (uint8_t*)"Result: No Cal", 12, 0);
    }

    Show_Str(10, 90, WHITE, BLACK, (uint8_t*)"Check Serial Output", 12, 0);
    Show_Str(10, 105, WHITE, BLACK, (uint8_t*)"for detailed info", 12, 0);

    Show_Str(10, 115, GRAY, BLACK, (uint8_t*)"UP:Back", 12, 0); //返回提示
}

/**
 * @brief 报警设置界面显示
 */
void Interface_DisplayAlarm(void)
{
    char str_buffer[32];

    Show_Str(40, 5, WHITE, BLACK, (uint8_t*)"Alarm Limit", 16, 0); //超限报警标题
    LCD_DrawLine(0, 25, LCD_W - 1, 25); //标题分割线

    Show_Str(10, 35, CYAN, BLACK, (uint8_t*)"Alarm Setup:", 16, 0); //报警设置标签

    // 显示报警使能状态（带选中指示）
    if (g_interface_manager.alarm_selected_item == 0) {
        Show_Str(5, 55, GREEN, BLACK, (uint8_t*)">", 12, 0); //选中指示符
    } else {
        Show_Str(5, 55, BLACK, BLACK, (uint8_t*)" ", 12, 0); //清除指示符
    }
    sprintf(str_buffer, "Alarm Enable: %3s", g_interface_manager.alarm_enabled ? "ON" : "OFF");
    Show_Str(15, 55, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //报警使能状态

    // 显示VSWR阈值（四位数编辑模式）
    // 显示VSWR选中指示符
    if (g_interface_manager.alarm_selected_item >= 1 && g_interface_manager.alarm_selected_item <= 4) {
        Show_Str(5, 70, GREEN, BLACK, (uint8_t*)">", 12, 0); //VSWR选中指示符
    } else {
        Show_Str(5, 70, BLACK, BLACK, (uint8_t*)" ", 12, 0); //清除指示符
    }

    Show_Str(15, 70, WHITE, BLACK, (uint8_t*)"VSWR Limit: ", 12, 0);

    // 分解VSWR值为各个位数（修复浮点精度问题）
    float current_value = g_interface_manager.vswr_alarm_threshold;
    // 转换为整数避免浮点精度问题
    int total_tenths = (int)(current_value * 10.0f + 0.5f);  // 四舍五入到0.1

    int hundreds = (total_tenths / 1000) % 10;
    int tens = (total_tenths / 100) % 10;
    int ones = (total_tenths / 10) % 10;
    int decimal = total_tenths % 10;

    // 显示百位（带选中指示）
    uint16_t color_h = (g_interface_manager.alarm_selected_item == 1) ? GREEN : WHITE;
    sprintf(str_buffer, "%d", hundreds);
    Show_Str(106, 70, color_h, BLACK, (uint8_t*)str_buffer, 12, 0);

    // 显示十位（带选中指示）
    uint16_t color_t = (g_interface_manager.alarm_selected_item == 2) ? GREEN : WHITE;
    sprintf(str_buffer, "%d", tens);
    Show_Str(112, 70, color_t, BLACK, (uint8_t*)str_buffer, 12, 0);

    // 显示个位（带选中指示）
    uint16_t color_o = (g_interface_manager.alarm_selected_item == 3) ? GREEN : WHITE;
    sprintf(str_buffer, "%d", ones);
    Show_Str(118, 70, color_o, BLACK, (uint8_t*)str_buffer, 12, 0);

    // 显示小数点
    Show_Str(124, 70, WHITE, BLACK, (uint8_t*)".", 12, 0);

    // 显示小数位（带选中指示）
    uint16_t color_d = (g_interface_manager.alarm_selected_item == 4) ? GREEN : WHITE;
    sprintf(str_buffer, "%d", decimal);
    Show_Str(128, 70, color_d, BLACK, (uint8_t*)str_buffer, 12, 0);

    Show_Str(10, 85, YELLOW, BLACK, (uint8_t*)"Buzzer when exceed", 12, 0); //超限蜂鸣器提示

    Show_Str(5, 100, GRAY, BLACK, (uint8_t*)"DOWN:Switch OK:Edit", 12, 0); //操作提示
    Show_Str(10, 115, GRAY, BLACK, (uint8_t*)"UP:Back", 12, 0); //返回提示
}

/**
 * @brief 亮度设置界面显示
 */
void Interface_DisplayBrightness(void)
{
    char str_buffer[32];

    Show_Str(40, 5, WHITE, BLACK, (uint8_t*)"Brightness", 16, 0); //显示亮度标题
    LCD_DrawLine(0, 25, LCD_W - 1, 25); //标题分割线

    Show_Str(10, 32, CYAN, BLACK, (uint8_t*)"Backlight:", 16, 0); //背光亮度标签

    // 显示当前亮度
    sprintf(str_buffer, "Current: %02d0%%", g_interface_manager.brightness_level);
    Show_Str(10, 55, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //当前亮度百分比

    // 绘制亮度条
    LCD_DrawRectangle(10, 75, 139, 85); //亮度条边框

    // 先清除进度条内部区域
    //LCD_Fill(11, 76, 139, 84, BLACK); //清除亮度条内容(已注释)

    // 计算并绘制当前亮度条
    uint16_t bar_width = g_interface_manager.brightness_level * 128 / 10;  // 128像素宽度 (139-11)
    if (bar_width > 0) {
        LCD_Fill(11, 76, 11 + bar_width - 1, 84, GREEN);  // 绘制亮度条
    }

    Show_Str(10, 95, YELLOW, BLACK, (uint8_t*)"DN:Add OK:Confirm", 12, 0); //操作提示
    Show_Str(10, 115, YELLOW, BLACK, (uint8_t*)"UP:Back", 12, 0); //返回提示
    
}

/**
 * @brief 关于界面显示
 */
void Interface_DisplayAbout(void)
{
    Show_Str(60, 5, WHITE, BLACK, (uint8_t*)"About", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(0, 35, CYAN, BLACK, (uint8_t*)"RF Power Meter V1.0", 16, 0);//版本
    Show_Str(10, 55, WHITE, BLACK, (uint8_t*)"Freq: 1Hz-100MHz", 12, 0);//频率
    Show_Str(10, 70, WHITE, BLACK, (uint8_t*)"Power: 0W-2kW", 12, 0);//功率范围更新
    Show_Str(10, 85, WHITE, BLACK, (uint8_t*)"VSWR: 1.0-999.0", 12, 0);//驻波比
    Show_Str(10, 100, WHITE, BLACK, (uint8_t*)"Author: XUN YU TEK", 12, 0);//作者

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
    LCD_Clear(BLACK); //清屏
    Show_Str(20, 10, GREEN, BLACK, (uint8_t*)"RF Power Meter", 16, 0); //射频功率计标题
    Show_Str(30, 30, WHITE, BLACK, (uint8_t*)"System Boot", 16, 0); //系统启动提示

    Show_Str(20, 65, CYAN, BLACK, (uint8_t*)"Initializing...", 12, 0); //初始化提示

    // 初始进度显示 (0-10%)
    for (i = 0; i <= 10; i++) {
        sprintf(progress_str, "%3d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);  // 移动到原进度条位置
        HAL_Delay(50);
    }

    // 步骤1：频率计初始化
    Show_Str(20, 80, YELLOW, BLACK, (uint8_t*)"FreqCounter Init", 12, 0); //频率计初始化提示

    // 进度显示 (10-30%)
    for (i = 10; i <= 30; i++) {
        sprintf(progress_str, "%3d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0); //进度百分比显示
        HAL_Delay(30);
    }

    if (FreqCounter_Init() != 0) {
        LCD_Clear(BLACK); //清屏显示错误
        Show_Str(10, 50, RED, BLACK, (uint8_t*)"FreqCounter Init FAIL!", 16, 0); //频率计初始化失败
        Show_Str(30, 70, WHITE, BLACK, (uint8_t*)"System Halted", 16, 0); //系统停止提示
        while(1) {
            HAL_Delay(1000);  // 停止运行，不喂狗让系统复位
        }
    }
    Show_Str(120, 80, GREEN, BLACK, (uint8_t*)"OK", 12, 0); //步骤1完成标记
    HAL_IWDG_Refresh(&hiwdg);  // 步骤1完成后喂狗

    // 步骤2：频率计启动
    Show_Str(20, 95, YELLOW, BLACK, (uint8_t*)"FreqCounter Start", 12, 0);

    // 进度显示 (30-50%)
    for (i = 30; i <= 50; i++) {
        sprintf(progress_str, "%3d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);
        HAL_Delay(30);
    }

    if (FreqCounter_Start() != 0) {
        LCD_Clear(BLACK);
        Show_Str(10, 50, RED, BLACK, (uint8_t*)"FreqCounter Start FAIL!", 16, 0);
        Show_Str(30, 70, WHITE, BLACK, (uint8_t*)"System Halted", 16, 0);
        while(1) {
            HAL_Delay(1000);  // 停止运行，不喂狗让系统复位
        }
    }
    Show_Str(135, 95, GREEN, BLACK, (uint8_t*)"OK", 12, 0); // 修复：向右移动避免重叠
    HAL_IWDG_Refresh(&hiwdg);  // 步骤2完成后喂狗

    // 步骤3：界面管理器初始化
    Show_Str(20, 110, YELLOW, BLACK, (uint8_t*)"Interface Init", 12, 0);

    // 进度显示 (50-70%)
    for (i = 50; i <= 70; i++) {
        sprintf(progress_str, "%3d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);
        HAL_Delay(30);
    }

    if (InterfaceManager_Init() != 0) {
        LCD_Clear(BLACK);
        Show_Str(10, 50, RED, BLACK, (uint8_t*)"Interface Init FAIL!", 16, 0);
        Show_Str(30, 70, WHITE, BLACK, (uint8_t*)"System Halted", 16, 0);
        while(1) {
            HAL_Delay(1000);  // 停止运行，不喂狗让系统复位
        }
    }
    Show_Str(135, 110, GREEN, BLACK, (uint8_t*)"OK", 12, 0); // 修复：向右移动避免重叠
    HAL_IWDG_Refresh(&hiwdg);  // 步骤3完成后喂狗
    HAL_Delay(300);

    // 步骤4：PWM启动 (只清除下半屏，保留标题和进度显示)
    const uint16_t y_boot = 47;      // "System Boot"的y坐标
    const uint16_t font_h = 16;      // 字号高度
    const uint16_t margin = 3;      // 额外边距，确保不擦到进度文字
    uint16_t y_clear_start = y_boot + font_h + margin;
    LCD_Fill(0, y_clear_start, lcddev.width - 1, lcddev.height - 1, BLACK);
    Show_Str(20, 10, GREEN, BLACK, (uint8_t*)"RF Power Meter", 16, 0);
    Show_Str(30, 30, WHITE, BLACK, (uint8_t*)"System Boot", 16, 0);

    // 恢复之前的进度 (70%)
    sprintf(progress_str, "%3d%%", 70);
    Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);

    Show_Str(20, 80, YELLOW, BLACK, (uint8_t*)"PWM Backlight", 12, 0);

    // 进度显示 (70-85%)
    for (i = 70; i <= 85; i++) {
        sprintf(progress_str, "%3d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);
        HAL_Delay(30);
    }

    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
    Show_Str(135, 80, GREEN, BLACK, (uint8_t*)"OK", 12, 0); // 修复：向右移动避免重叠

    // 步骤5：系统就绪
    Show_Str(20, 95, YELLOW, BLACK, (uint8_t*)"System Ready", 12, 0);

    // 进度显示 (85-100%)
    for (i = 85; i <= 100; i++) {
        sprintf(progress_str, "%3d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);
        HAL_Delay(25);
    }

    Show_Str(135, 95, GREEN, BLACK, (uint8_t*)"OK", 12, 0); // 修复：向右移动避免重叠
    HAL_IWDG_Refresh(&hiwdg);  // 启动序列完成后最后一次喂狗

    // 显示完成信息
    Show_Str(35, 110, WHITE, BLACK, (uint8_t*)"Boot Complete!", 16, 0);
    HAL_Delay(1000);

    // 清屏准备进入主界面
    LCD_Clear(BLACK);
}
