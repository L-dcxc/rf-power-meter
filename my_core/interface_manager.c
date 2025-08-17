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
    .band_gain_fwd = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
    .band_gain_ref = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
    .freq_trim = 1.0f,
    .is_calibrated = 0
};

/* 校准状态全局变量 */
CalibrationState_t g_calibration_state = {
    .current_step = CAL_STEP_CONFIRM,
    .current_band = 0,
    .current_power_point = 0,
    .current_channel = 0,
    .sample_count = 0,
    .sample_sum_fwd = 0.0f,
    .sample_sum_ref = 0.0f,
    .is_stable = 0,
    .stable_count = 0,
    .target_power = 100.0f,     // 默认目标功率100W
    .power_cal_mode = 0         // 默认正向功率校准
};

/* 频段定义（11个HF常用频段）*/
static const char* band_names[11] = {
    "160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m"
};

static const float band_frequencies[11] = {
    1.8f, 3.5f, 5.3f, 7.0f, 10.1f, 14.0f, 18.1f, 21.0f, 24.9f, 28.0f, 50.0f  // MHz
};

/* 功率校准点定义 (0W-2kW范围，20个校准点) */
static const float power_cal_points[20] = {
    100.0f, 200.0f, 300.0f, 400.0f, 500.0f,      // 100W-500W
    600.0f, 700.0f, 800.0f, 900.0f, 1000.0f,     // 600W-1000W
    1100.0f, 1200.0f, 1300.0f, 1400.0f, 1500.0f, // 1100W-1500W
    1600.0f, 1700.0f, 1800.0f, 1900.0f, 2000.0f  // 1600W-2000W
};

/* 反射功率校准点定义 (同样支持0W-2kW范围) */
static const float ref_power_cal_points[20] = {
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
        saved_vswr_threshold >= 1.5f && saved_vswr_threshold <= 5.0f) {
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
            case INTERFACE_CAL_REFLECT:
                Interface_DisplayCalReflect();
                break;
            case INTERFACE_CAL_FREQ:
                Interface_DisplayCalFreq();
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
        g_interface_manager.current_interface <= INTERFACE_CAL_FREQ) {
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
                // 切换选中项（报警开关 ? VSWR阈值）
                g_interface_manager.alarm_selected_item =
                    (g_interface_manager.alarm_selected_item + 1) % 2;
                g_interface_manager.need_refresh = 1;
            } else if (key == KEY_OK) {
                // 调节当前选中的项目
                if (g_interface_manager.alarm_selected_item == 0) {
                    // 切换报警开关
                    g_interface_manager.alarm_enabled = !g_interface_manager.alarm_enabled;
                } else {
                    // 调节VSWR阈值（循环：1.5 → 2.0 → 2.5 → 3.0 → 3.5 → 4.0 → 5.0 → 1.5）
                    if (g_interface_manager.vswr_alarm_threshold >= 5.0f) {
                        g_interface_manager.vswr_alarm_threshold = 1.5f;
                    } else if (g_interface_manager.vswr_alarm_threshold >= 4.0f) {
                        g_interface_manager.vswr_alarm_threshold = 5.0f;
                    } else if (g_interface_manager.vswr_alarm_threshold >= 3.5f) {
                        g_interface_manager.vswr_alarm_threshold = 4.0f;
                    } else if (g_interface_manager.vswr_alarm_threshold >= 3.0f) {
                        g_interface_manager.vswr_alarm_threshold = 3.5f;
                    } else if (g_interface_manager.vswr_alarm_threshold >= 2.5f) {
                        g_interface_manager.vswr_alarm_threshold = 3.0f;
                    } else if (g_interface_manager.vswr_alarm_threshold >= 2.0f) {
                        g_interface_manager.vswr_alarm_threshold = 2.5f;
                    } else if (g_interface_manager.vswr_alarm_threshold >= 1.5f) {
                        g_interface_manager.vswr_alarm_threshold = 2.0f;
                    } else {
                        g_interface_manager.vswr_alarm_threshold = 1.5f;
                    }
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
                // 开始校准向导
                Calibration_StartStep(CAL_STEP_CONFIRM);
                InterfaceManager_SwitchTo(INTERFACE_CAL_CONFIRM);
            }
            break;

        case INTERFACE_CAL_CONFIRM:
            // 校准确认页按键处理
            if (key == KEY_UP) {
                InterfaceManager_SwitchTo(INTERFACE_CALIBRATION);
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
                InterfaceManager_SwitchTo(INTERFACE_CAL_ZERO);
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
                        g_calibration_state.target_power = ref_power_cal_points[0];
                    }
                } else {  // 反射功率校准
                    if (g_calibration_state.current_power_point < 19) {  // 0-19共20个点
                        g_calibration_state.current_power_point++;
                        g_calibration_state.target_power = ref_power_cal_points[g_calibration_state.current_power_point];
                    } else {
                        // 完成功率校准，进入下一步
                        Calibration_StartStep(CAL_STEP_BAND);
                        InterfaceManager_SwitchTo(INTERFACE_CAL_BAND);
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

    g_interface_manager.brightness_level = level;

    // 计算PWM占空比 (10%-100%)
    uint16_t duty = 99 + (level * 90);

    // 设置TIM3 CH3的PWM占空比 (PB0背光控制)
    LCD_SetBacklight(duty);
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
 */
void InterfaceManager_BuzzerProcess(void)
{
    if (g_buzzer_state.is_active) {
        if (g_buzzer_state.duration_count > 0) {
            g_buzzer_state.duration_count--;
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
    g_calibration_state.current_band = 0;
    g_calibration_state.current_power_point = 0;
    g_calibration_state.current_channel = 0;
    g_calibration_state.sample_count = 0;
    g_calibration_state.sample_sum_fwd = 0.0f;
    g_calibration_state.sample_sum_ref = 0.0f;
    g_calibration_state.is_stable = 0;
    g_calibration_state.stable_count = 0;
    g_calibration_state.target_power = 100.0f;
    g_calibration_state.power_cal_mode = 0;
}

/**
 * @brief 从EEPROM加载校准数据
 *
 * EEPROM地址布局 (支持0W-2kW功率范围):
 * 0x0010-0x0017: 零点偏移数据 (8字节)
 * 0x0020-0x011F: 正向功率校准表 (20点×8字节=160字节)
 * 0x0120-0x021F: 反射功率校准表 (20点×8字节=160字节)
 * 0x0220-0x0221: 校准点数 (2字节)
 * 0x0300-0x035B: 频段增益修正 (11×8字节=88字节)
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
            // 使用默认值
            g_calibration_data.fwd_table[i].power = power_cal_points[i];
            g_calibration_data.fwd_table[i].voltage = (i + 1) * 1.0f;  // 默认电压
        }
    }

    // 读取反射功率校准表 (地址从0x0120开始，避免与正向表冲突)
    for (int i = 0; i < 20; i++) {
        uint16_t addr = 0x0120 + i * sizeof(PowerCalPoint_t);
        if (BL24C16_Read(addr, (uint8_t*)&g_calibration_data.ref_table[i], sizeof(PowerCalPoint_t)) != EEPROM_OK) {
            // 使用默认值
            g_calibration_data.ref_table[i].power = ref_power_cal_points[i];
            g_calibration_data.ref_table[i].voltage = (i + 1) * 0.1f;  // 默认电压
        }
    }

    // 读取校准点数 (调整地址避免与校准表冲突)
    if (BL24C16_Read(0x0220, &g_calibration_data.fwd_points, 1) != EEPROM_OK) {
        g_calibration_data.fwd_points = 0;
    }
    if (BL24C16_Read(0x0221, &g_calibration_data.ref_points, 1) != EEPROM_OK) {
        g_calibration_data.ref_points = 0;
    }

    // 读取频段增益修正 (调整地址避免冲突)
    for (int i = 0; i < 11; i++) {
        if (BL24C16_Read(0x0300 + i * 4, (uint8_t*)&g_calibration_data.band_gain_fwd[i], sizeof(float)) != EEPROM_OK) {
            g_calibration_data.band_gain_fwd[i] = 1.0f;
        }
        if (BL24C16_Read(0x0330 + i * 4, (uint8_t*)&g_calibration_data.band_gain_ref[i], sizeof(float)) != EEPROM_OK) {
            g_calibration_data.band_gain_ref[i] = 1.0f;
        }
    }

    // 读取频率微调 (调整地址避免冲突)
    if (BL24C16_Read(0x0400, (uint8_t*)&g_calibration_data.freq_trim, sizeof(float)) != EEPROM_OK) {
        g_calibration_data.freq_trim = 1.0f;
    }

    // 读取校准完成标志
    if (BL24C16_Read(0x0404, &g_calibration_data.is_calibrated, 1) != EEPROM_OK) {
        g_calibration_data.is_calibrated = 0;
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

    // 保存频段增益修正 (使用新地址)
    for (int i = 0; i < 11; i++) {
        BL24C16_Write(0x0300 + i * 4, (uint8_t*)&g_calibration_data.band_gain_fwd[i], sizeof(float));
        BL24C16_Write(0x0330 + i * 4, (uint8_t*)&g_calibration_data.band_gain_ref[i], sizeof(float));
    }

    // 保存频率微调 (使用新地址)
    BL24C16_Write(0x0400, (uint8_t*)&g_calibration_data.freq_trim, sizeof(float));

    // 保存校准完成标志
    g_calibration_data.is_calibrated = 1;
    BL24C16_Write(0x0404, &g_calibration_data.is_calibrated, 1);
}

/**
 * @brief 获取频段索引
 */
uint8_t Calibration_GetBandIndex(float frequency)
{
    // 根据频率返回对应的频段索引(0-10)
    if (frequency < 2.5f) return 0;        // 160m (1.8MHz)
    else if (frequency < 4.5f) return 1;   // 80m (3.5MHz)
    else if (frequency < 6.0f) return 2;   // 60m (5.3MHz)
    else if (frequency < 8.5f) return 3;   // 40m (7.0MHz)
    else if (frequency < 12.0f) return 4;  // 30m (10.1MHz)
    else if (frequency < 16.0f) return 5;  // 20m (14.0MHz)
    else if (frequency < 19.5f) return 6;  // 17m (18.1MHz)
    else if (frequency < 23.0f) return 7;  // 15m (21.0MHz)
    else if (frequency < 26.5f) return 8;  // 12m (24.9MHz)
    else if (frequency < 35.0f) return 9;  // 10m (28.0MHz)
    else return 10;                        // 6m (50.0MHz)
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
        // 未校准时使用简单线性转换
        return (raw_voltage - (is_forward ? g_calibration_data.forward_offset : g_calibration_data.reflected_offset)) * 100.0f;
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

    // 应用频段增益修正
    uint8_t band_index = Calibration_GetBandIndex(frequency);
    float final_power = base_power * (is_forward ? g_calibration_data.band_gain_fwd[band_index] : g_calibration_data.band_gain_ref[band_index]);

    return (final_power > 0.0f) ? final_power : 0.0f;
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
    g_calibration_state.is_stable = 0;
    g_calibration_state.stable_count = 0;

    // 根据步骤初始化特定状态
    if (step == CAL_STEP_POWER) {
        g_calibration_state.current_power_point = 0;
        g_calibration_state.current_channel = 0;  // 从正向功率开始
        g_calibration_state.target_power = power_cal_points[0];  // 100W
        g_calibration_state.power_cal_mode = 0;
    } else if (step == CAL_STEP_BAND) {
        g_calibration_state.current_band = 0;
        g_calibration_state.target_power = 100.0f;  // 频段校准使用固定功率
    }

    g_interface_manager.need_refresh = 1;
}

/**
 * @brief 读取ADC电压值
 */
static void Calibration_ReadADCVoltage(float* fwd_voltage, float* ref_voltage)
{
    // 启动ADC转换
    HAL_ADC_Start(&hadc1);

    // 读取PA2 (正向功率)
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint32_t adc_forward = HAL_ADC_GetValue(&hadc1);

    // 读取PA3 (反射功率)
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint32_t adc_reflected = HAL_ADC_GetValue(&hadc1);

    HAL_ADC_Stop(&hadc1);

    // 转换为电压值 (3.3V参考电压，12位ADC)
    *fwd_voltage = (float)adc_forward * 3.3f / 4095.0f;
    *ref_voltage = (float)adc_reflected * 3.3f / 4095.0f;
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

    // 启动ADC转换
    HAL_ADC_Start(&hadc1);

    // 读取PA2 (正向功率)
    HAL_ADC_PollForConversion(&hadc1, 10);
    adc_forward = HAL_ADC_GetValue(&hadc1);

    // 读取PA3 (反射功率)
    HAL_ADC_PollForConversion(&hadc1, 10);
    adc_reflected = HAL_ADC_GetValue(&hadc1);

    HAL_ADC_Stop(&hadc1);

    // 转换为电压值 (3.3V参考电压，12位ADC)
    fwd_voltage = (float)adc_forward * 3.3f / 4095.0f;
    ref_voltage = (float)adc_reflected * 3.3f / 4095.0f;

    // 显示ADC原始值
    sprintf(str_buffer, "F:%d", (int)adc_forward);
    Show_Str(x, y, GRAY, BLACK, (uint8_t*)str_buffer, 12, 0);

    sprintf(str_buffer, "R:%d", (int)adc_reflected);
    Show_Str(x, y + 15, GRAY, BLACK, (uint8_t*)str_buffer, 12, 0);

    // 显示电压值
    //sprintf(str_buffer, "%.2fV", fwd_voltage);
    //Show_Str(x, y + 30, GRAY, BLACK, (uint8_t*)str_buffer, 12, 0);

    //sprintf(str_buffer, "%.2fV", ref_voltage);
    //Show_Str(x, y + 45, GRAY, BLACK, (uint8_t*)str_buffer, 12, 0);
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

        // 采样完成（32次）
        if (g_calibration_state.sample_count >= 32) {
            float avg_fwd = g_calibration_state.sample_sum_fwd / 32.0f;
            float avg_ref = g_calibration_state.sample_sum_ref / 32.0f;

            // 根据当前步骤处理采样结果
            switch (g_calibration_state.current_step) {
                case CAL_STEP_ZERO:
                    g_calibration_data.forward_offset = avg_fwd;
                    g_calibration_data.reflected_offset = avg_ref;
                    InterfaceManager_Beep(200);  // 成功提示音
                    break;

                case CAL_STEP_POWER:
                    {
                        // 多点功率校准
                        uint8_t point = g_calibration_state.current_power_point;
                        uint8_t channel = g_calibration_state.current_channel;

                        if (channel == 0) {  // 正向功率校准
                            g_calibration_data.fwd_table[point].power = g_calibration_state.target_power;
                            g_calibration_data.fwd_table[point].voltage = avg_fwd - g_calibration_data.forward_offset;
                            g_calibration_data.fwd_points = point + 1;
                        } else {  // 反射功率校准
                            g_calibration_data.ref_table[point].power = g_calibration_state.target_power;
                            g_calibration_data.ref_table[point].voltage = avg_ref - g_calibration_data.reflected_offset;
                            g_calibration_data.ref_points = point + 1;
                        }
                        InterfaceManager_Beep(200);
                    }
                    break;

                case CAL_STEP_BAND:
                    {
                        uint8_t band = g_calibration_state.current_band;
                        // 使用查表方式计算基准功率
                        float base_fwd = Calibration_CalculatePowerFromTable(avg_fwd - g_calibration_data.forward_offset,
                                                                            g_calibration_data.fwd_table,
                                                                            g_calibration_data.fwd_points);
                        float base_ref = Calibration_CalculatePowerFromTable(avg_ref - g_calibration_data.reflected_offset,
                                                                            g_calibration_data.ref_table,
                                                                            g_calibration_data.ref_points);

                        if (base_fwd > 0.0f) {
                            g_calibration_data.band_gain_fwd[band] = g_calibration_state.target_power / base_fwd;
                        }
                        if (base_ref > 0.0f) {
                            g_calibration_data.band_gain_ref[band] = g_calibration_state.target_power / base_ref;
                        }
                        InterfaceManager_Beep(200);
                    }
                    break;

                case CAL_STEP_REFLECT:
                    {
                        // 反射通道定标（使用已知SWR负载）
                        float measured_fwd = Calibration_CalculatePowerFromTable(avg_fwd - g_calibration_data.forward_offset,
                                                                                g_calibration_data.fwd_table,
                                                                                g_calibration_data.fwd_points);
                        float measured_ref_voltage = avg_ref - g_calibration_data.reflected_offset;

                        // 假设使用25Ω负载（理论VSWR=2.0，反射系数=1/3）
                        float target_ref_power = measured_fwd / 9.0f;  // VSWR=2.0时的反射功率

                        // 更新反射功率校准表的最后一个点
                        if (g_calibration_data.ref_points > 0) {
                            uint8_t last_point = g_calibration_data.ref_points - 1;
                            g_calibration_data.ref_table[last_point].power = target_ref_power;
                            g_calibration_data.ref_table[last_point].voltage = measured_ref_voltage;
                        }
                        InterfaceManager_Beep(200);
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
    Show_Str(35, 5, WHITE, BLACK, (uint8_t*)"Zero Cal", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(10, 30, CYAN, BLACK, (uint8_t*)"Step 1/5:", 12, 0);
    Show_Str(10, 45, WHITE, BLACK, (uint8_t*)"Turn OFF RF", 16, 0);
    Show_Str(10, 65, WHITE, BLACK, (uint8_t*)"No signal input", 12, 0);

    if (g_calibration_state.sample_count > 0) {
        Show_Str(10, 80, YELLOW, BLACK, (uint8_t*)"Sampling...", 12, 0);
        char progress[16];
        sprintf(progress, "%d/32 ", g_calibration_state.sample_count);
        Show_Str(100, 80, YELLOW, BLACK, (uint8_t*)progress, 12, 0);
    } else {
        Show_Str(10, 80, GREEN, BLACK, (uint8_t*)"Ready to sample", 12, 0);
    }

    // 显示ADC调试信息
    Display_ADCInfo(122, 30);

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

    Show_Str(10, 30, CYAN, BLACK, (uint8_t*)"Step 2/5:", 12, 0);

    // 显示当前校准通道和点
    if (g_calibration_state.current_channel == 0) {
        sprintf(str_buffer, "FWD Point %d/20 ", g_calibration_state.current_power_point + 1);
    } else {
        sprintf(str_buffer, "REF Point %d/20 ", g_calibration_state.current_power_point + 1);
    }
    Show_Str(10, 45, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    // 显示目标功率，支持kW单位
    if (g_calibration_state.target_power >= 1000.0f) {
        sprintf(str_buffer, "Target: %.1fkW", g_calibration_state.target_power / 1000.0f);
    } else {
        sprintf(str_buffer, "Target: %.0fW", g_calibration_state.target_power);
    }
    Show_Str(10, 60, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    if (g_calibration_state.is_stable) {
        Show_Str(10, 75, GREEN, BLACK, (uint8_t*)"STABLE - Ready", 12, 0);
    } else {
        Show_Str(10, 75, YELLOW, BLACK, (uint8_t*)"Set power & wait", 12, 0);
    }

    if (g_calibration_state.sample_count > 0) {
        Show_Str(10, 90, YELLOW, BLACK, (uint8_t*)"Sampling...", 12, 0);
    }

    // 显示ADC调试信息
    Display_ADCInfo(122, 30);

    Show_Str(5, 105, GRAY, BLACK, (uint8_t*)"DOWN:Next OK:Cal", 12, 0);
    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back", 12, 0);
}

/**
 * @brief 显示频段增益修正
 */
void Interface_DisplayCalBand(void)
{
    char str_buffer[32];

    Show_Str(35, 5, WHITE, BLACK, (uint8_t*)"Band Cal", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(10, 30, CYAN, BLACK, (uint8_t*)"Step 3/5:", 12, 0);

    // 显示当前频段
    sprintf(str_buffer, "Band: %s (%.1fMHz)",
            band_names[g_calibration_state.current_band],
            band_frequencies[g_calibration_state.current_band]);
    Show_Str(10, 45, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    // 显示进度
    sprintf(str_buffer, "Progress: %d/11 ", g_calibration_state.current_band + 1);
    Show_Str(10, 60, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    if (g_calibration_state.is_stable) {
        Show_Str(10, 75, GREEN, BLACK, (uint8_t*)"STABLE - Ready", 12, 0);
    } else {
        Show_Str(10, 75, YELLOW, BLACK, (uint8_t*)"Set freq & power", 12, 0);
    }

    if (g_calibration_state.sample_count > 0) {
        Show_Str(10, 90, YELLOW, BLACK, (uint8_t*)"Sampling...", 12, 0);
    }

    // 显示ADC调试信息
    Display_ADCInfo(122, 30);

    Show_Str(5, 105, GRAY, BLACK, (uint8_t*)"DOWN:Next OK:Cal", 12, 0);
    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back", 12, 0);
}

/**
 * @brief 显示反射通道定标
 */
void Interface_DisplayCalReflect(void)
{
    Show_Str(25, 5, WHITE, BLACK, (uint8_t*)"Reflect Cal", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(10, 30, CYAN, BLACK, (uint8_t*)"Step 4/5:", 12, 0);
    Show_Str(10, 45, WHITE, BLACK, (uint8_t*)"Connect 25ohm", 12, 0);
    Show_Str(10, 60, WHITE, BLACK, (uint8_t*)"(SWR=2.0 std)", 12, 0);

    if (g_calibration_state.is_stable) {
        Show_Str(10, 75, GREEN, BLACK, (uint8_t*)"STABLE - Ready", 12, 0);
    } else {
        Show_Str(10, 75, YELLOW, BLACK, (uint8_t*)"Waiting stable..", 12, 0);
    }

    if (g_calibration_state.sample_count > 0) {
        Show_Str(10, 90, YELLOW, BLACK, (uint8_t*)"Sampling...", 12, 0);
    }

    // 显示ADC调试信息
    Display_ADCInfo(122, 30);

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"OK:Cal UP:Back", 12, 0);
}

/**
 * @brief 显示频率计微调
 */
void Interface_DisplayCalFreq(void)
{
    char str_buffer[32];

    Show_Str(30, 5, WHITE, BLACK, (uint8_t*)"Freq Cal", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(10, 30, CYAN, BLACK, (uint8_t*)"Step 5/5:", 12, 0);
    Show_Str(10, 45, WHITE, BLACK, (uint8_t*)"Input 10.000MHz", 12, 0);

    // 显示当前频率读数
    FreqResult_t freq_result;
    if (FreqCounter_GetResult(&freq_result) == 0) {
        sprintf(str_buffer, "Read: %.3fMHz", freq_result.frequency_display);
        Show_Str(10, 60, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);
    } else {
        Show_Str(10, 60, GRAY, BLACK, (uint8_t*)"Read: -- MHz", 12, 0);
    }

    // 显示微调系数
    sprintf(str_buffer, "Trim: %.6f", g_calibration_data.freq_trim);
    Show_Str(10, 75, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    // 显示ADC调试信息
    Display_ADCInfo(122, 30);

    Show_Str(5, 105, GRAY, BLACK, (uint8_t*)"DOWN:Adj OK:Save", 12, 0);
    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back", 12, 0);
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
        sprintf(str_buffer, "%.2f %s", g_power_result.forward_power,
                g_power_result.forward_unit == POWER_UNIT_KW ? "kW" : "W");
        Show_Str(5, 45, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //正向功率数值
    } else {
        Show_Str(5, 45, GRAY, BLACK, (uint8_t*)"-- W", 12, 0); //正向功率无效显示
    }

    Show_Str(5, 60, CYAN, BLACK, (uint8_t*)"Reflect:", 12, 0); //反射功率标签
    if (g_power_result.is_valid) {
        sprintf(str_buffer, "%.2f %s", g_power_result.reflected_power,
                g_power_result.reflected_unit == POWER_UNIT_KW ? "kW" : "W");
        Show_Str(5, 75, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //反射功率数值
    } else {
        Show_Str(5, 75, GRAY, BLACK, (uint8_t*)"-- W", 12, 0); //反射功率无效显示
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
        sprintf(str_buffer, "%.2f", g_rf_params.vswr);
        Show_Str(85, 45, vswr_color, BLACK, (uint8_t*)str_buffer, 12, 0); //驻波比数值(带颜色)
    } else {
        Show_Str(85, 45, GRAY, BLACK, (uint8_t*)"--", 12, 0); //驻波比无效显示
    }

    Show_Str(85, 60, CYAN, BLACK, (uint8_t*)"Refl.Coef:", 12, 0); //反射系数标签
    if (g_rf_params.is_valid) {
        sprintf(str_buffer, "%.3f", g_rf_params.reflection_coeff);
        Show_Str(85, 75, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //反射系数数值
    } else {
        Show_Str(85, 75, GRAY, BLACK, (uint8_t*)"--", 12, 0); //反射系数无效显示
    }

    // 底部显示传输效率和频率
    LCD_DrawLine(0, 95, LCD_W - 1, 95); //底部分割线

    Show_Str(5, 100, CYAN, BLACK, (uint8_t*)"Efficiency:", 12, 0); //传输效率标签
    if (g_rf_params.is_valid) {
        sprintf(str_buffer, "%.1f%%", g_rf_params.transmission_eff);
        Show_Str(70, 100, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //传输效率数值
    } else {
        Show_Str(70, 100, GRAY, BLACK, (uint8_t*)"--%", 12, 0); //传输效率无效显示
    }

    Show_Str(5, 115, CYAN, BLACK, (uint8_t*)"Freq:", 12, 0); //频率标签
    FreqResult_t freq_result;
    if (FreqCounter_GetResult(&freq_result) == 0) {
        sprintf(str_buffer, "%.2f %s", freq_result.frequency_display,
                FreqCounter_GetUnitString(freq_result.unit));
        Show_Str(40, 115, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //频率数值
    } else {
        Show_Str(40, 115, GRAY, BLACK, (uint8_t*)"-- Hz", 12, 0); //频率无效显示
    }

    // 显示操作提示
    Show_Str(110, 115, GRAY, BLACK, (uint8_t*)"OK:Menu", 12, 0); //操作提示
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
 * @brief 标定界面显示
 */
void Interface_DisplayStandard(void)
{
    Show_Str(50, 5, WHITE, BLACK, (uint8_t*)"Settings", 16, 0); //标定设置标题
    LCD_DrawLine(0, 25, LCD_W - 1, 25); //标题分割线

    Show_Str(10, 35, CYAN, BLACK, (uint8_t*)"Power Cal:", 16, 0); //功率校准标签
    Show_Str(10, 55, WHITE, BLACK, (uint8_t*)"Forward Slope: 1.00", 12, 0); //正向功率斜率
    Show_Str(10, 70, WHITE, BLACK, (uint8_t*)"Reflect Slope: 1.00", 12, 0); //反射功率斜率
    Show_Str(10, 85, WHITE, BLACK, (uint8_t*)"Zero Offset: 0.00V", 12, 0); //零点偏移

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back", 12, 0); //返回提示
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
    sprintf(str_buffer, "Alarm Enable: %s", g_interface_manager.alarm_enabled ? "ON " : "OFF");
    Show_Str(15, 55, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //报警使能状态

    // 显示VSWR阈值（带选中指示）
    if (g_interface_manager.alarm_selected_item == 1) {
        Show_Str(5, 70, GREEN, BLACK, (uint8_t*)">", 12, 0); //选中指示符
    } else {
        Show_Str(5, 70, BLACK, BLACK, (uint8_t*)" ", 12, 0); //清除指示符
    }
    sprintf(str_buffer, "VSWR Limit: %.1f", g_interface_manager.vswr_alarm_threshold);
    Show_Str(15, 70, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //驻波比报警阈值

    Show_Str(10, 85, YELLOW, BLACK, (uint8_t*)"Buzzer when exceed", 12, 0); //超限蜂鸣器提示

    Show_Str(5, 100, GRAY, BLACK, (uint8_t*)"DOWN:Switch OK:Edit", 12, 0); //操作提示
    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back", 12, 0); //返回提示
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
    Show_Str(5, 115, YELLOW, BLACK, (uint8_t*)"UP:Back", 12, 0); //返回提示
    
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
    Show_Str(10, 85, WHITE, BLACK, (uint8_t*)"VSWR: 1.0-10.0", 12, 0);//驻波比
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
        sprintf(progress_str, "%d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);  // 移动到原进度条位置
        HAL_Delay(50);
    }

    // 步骤1：频率计初始化
    Show_Str(20, 80, YELLOW, BLACK, (uint8_t*)"FreqCounter Init", 12, 0); //频率计初始化提示

    // 进度显示 (10-30%)
    for (i = 10; i <= 30; i++) {
        sprintf(progress_str, "%d%%", i);
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
    Show_Str(130, 80, GREEN, BLACK, (uint8_t*)"OK", 12, 0); //步骤1完成标记
    HAL_IWDG_Refresh(&hiwdg);  // 步骤1完成后喂狗

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
            HAL_Delay(1000);  // 停止运行，不喂狗让系统复位
        }
    }
    Show_Str(130, 95, GREEN, BLACK, (uint8_t*)"OK", 12, 0);
    HAL_IWDG_Refresh(&hiwdg);  // 步骤2完成后喂狗

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
            HAL_Delay(1000);  // 停止运行，不喂狗让系统复位
        }
    }
    Show_Str(130, 110, GREEN, BLACK, (uint8_t*)"OK", 12, 0);
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
    HAL_IWDG_Refresh(&hiwdg);  // 启动序列完成后最后一次喂狗

    // 显示完成信息
    Show_Str(35, 110, WHITE, BLACK, (uint8_t*)"Boot Complete!", 16, 0);
    HAL_Delay(1000);

    // 清屏准备进入主界面
    LCD_Clear(BLACK);
}
