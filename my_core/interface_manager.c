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
    .brightness_level = 8,
    .alarm_enabled = 1,
    .vswr_alarm_threshold = 3.0f,
    .alarm_selected_item = 0,
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
#define MAX_CAL_FREQ    100.0f  // 最大标定频率(MHz)
#define FREQ_STEP       1.0f    // 频率调整步长(MHz)

/* 功率校准点定义 (0W-2kW范围，20个校准点) */
static const float power_cal_points[20] = {
    100.0f, 200.0f, 300.0f, 400.0f, 500.0f,      // 100W-500W
    600.0f, 700.0f, 800.0f, 900.0f, 1000.0f,     // 600W-1000W
    1100.0f, 1200.0f, 1300.0f, 1400.0f, 1500.0f, // 1100W-1500W
    1600.0f, 1700.0f, 1800.0f, 1900.0f, 2000.0f  // 1600W-2000W
};

#define CAL_STABLE_ADC_REL_TOL   0.05f
#define CAL_STABLE_ADC_ABS_TOL   0.01f
#define CAL_STABLE_ADC_COUNT     10
#define CAL_STABLE_FREQ_REL_TOL  0.001f
#define CAL_STABLE_FREQ_ABS_TOL  5.0f
#define CAL_STABLE_FREQ_COUNT    2
#define CAL_SAMPLE_TOTAL_COUNT   10

static uint8_t Calibration_IsValueStable(float current, float last, float rel_tol, float abs_tol)
{
    float ref = fabs(current);
    float last_abs = fabs(last);
    float threshold;

    if (last_abs > ref) {
        ref = last_abs;
    }

    threshold = ref * rel_tol;
    if (threshold < abs_tol) {
        threshold = abs_tol;
    }

    return (uint8_t)(fabs(current - last) <= threshold);
}

static float Calibration_NormalizeFrequency(float frequency)
{
    if (isnan(frequency) || isinf(frequency) ||
        frequency < MIN_CAL_FREQ || frequency > MAX_CAL_FREQ) {
        return 14.0f;
    }

    return frequency;
}

/**
 * @brief 界面管理器初始化
 */
int8_t InterfaceManager_Init(void)
{
    // 初始化EEPROM
    BL24C16_Init();

    // 初始化界面管理器状态
    g_interface_manager.current_interface = INTERFACE_MAIN;

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
    uint8_t saved_buzzer_enabled;
    if (BL24C16_Read(0x0006, &saved_buzzer_enabled, 1) == EEPROM_OK && saved_buzzer_enabled <= 1) {
        g_interface_manager.buzzer_enabled = saved_buzzer_enabled;
    } else {
        g_interface_manager.buzzer_enabled = 1;    // 默认开启蜂鸣器
    }
    g_interface_manager.alarm_selected_item = 0;  // 默认选中报警开关
    g_interface_manager.need_refresh = 1;

    // 从EEPROM读取Modbus从站地址，失败则使用默认值
    uint8_t saved_modbus_addr;
    if (BL24C16_Read(0x0007, &saved_modbus_addr, 1) == EEPROM_OK && saved_modbus_addr >= 1) {
        g_interface_manager.modbus_address = saved_modbus_addr;
    } else {
        g_interface_manager.modbus_address = 1;  // 默认从站地址1
    }
    
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
 * @brief 切换到指定界面
 */
void InterfaceManager_SwitchTo(InterfaceIndex_t interface)
{
    if (interface != g_interface_manager.current_interface) {
        g_interface_manager.current_interface = interface;
        g_interface_manager.need_refresh = 1;

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
    g_calibration_data.cal_frequency = Calibration_NormalizeFrequency(g_calibration_data.cal_frequency);

    // 重置校准状态
    g_calibration_state.current_step = CAL_STEP_CONFIRM;
    g_calibration_state.cal_frequency = g_calibration_data.cal_frequency;
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
    g_calibration_state.cal_frequency = Calibration_NormalizeFrequency(g_calibration_state.cal_frequency);
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
 * @brief 处理校准采样
 */
void Calibration_ProcessSample(void)
{
    static float last_fwd = 0.0f;
    static float last_ref = 0.0f;
    static float last_freq_hz = 0.0f;
    static uint8_t adc_tracking_valid = 0;
    static uint8_t freq_tracking_valid = 0;
    static CalibrationStep_t tracked_step = CAL_STEP_COMPLETE;
    static uint8_t tracked_channel = 0xFF;
    static float tracked_target_power = -1.0f;
    static float tracked_cal_frequency = -1.0f;

    // 获取当前ADC电压值
    float current_fwd, current_ref;
    Calibration_ReadADCVoltage(&current_fwd, &current_ref);

    if (g_calibration_state.current_step != tracked_step ||
        (g_calibration_state.current_step == CAL_STEP_POWER &&
         (g_calibration_state.current_channel != tracked_channel ||
          fabs(g_calibration_state.target_power - tracked_target_power) > 0.01f)) ||
        (g_calibration_state.current_step == CAL_STEP_BAND &&
         fabs(g_calibration_state.cal_frequency - tracked_cal_frequency) > 0.001f)) {
        tracked_step = g_calibration_state.current_step;
        tracked_channel = g_calibration_state.current_channel;
        tracked_target_power = g_calibration_state.target_power;
        tracked_cal_frequency = g_calibration_state.cal_frequency;
        adc_tracking_valid = 0;
        freq_tracking_valid = 0;
        g_calibration_state.stable_count = 0;
        g_calibration_state.is_stable = 0;
    }

    // 稳定性检测（变化小于5%认为稳定）
    if (g_calibration_state.current_step == CAL_STEP_BAND) {
        FreqResult_t freq_result;

        if (FreqCounter_GetResult(&freq_result) == 0 && freq_result.is_valid && freq_result.frequency_hz > 0) {
            if (FreqCounter_IsNewResult()) {
                float current_freq_hz = (float)freq_result.frequency_hz * 16.0f * g_calibration_data.freq_trim;

                if (!freq_tracking_valid) {
                    g_calibration_state.stable_count = 1;
                    g_calibration_state.is_stable = 0;
                    freq_tracking_valid = 1;
                } else if (Calibration_IsValueStable(current_freq_hz, last_freq_hz,
                                                     CAL_STABLE_FREQ_REL_TOL, CAL_STABLE_FREQ_ABS_TOL)) {
                    g_calibration_state.stable_count++;
                    if (g_calibration_state.stable_count >= CAL_STABLE_FREQ_COUNT) {
                        g_calibration_state.is_stable = 1;
                    }
                } else {
                    g_calibration_state.stable_count = 1;
                    g_calibration_state.is_stable = 0;
                }

                last_freq_hz = current_freq_hz;
            }
        } else {
            freq_tracking_valid = 0;
            g_calibration_state.stable_count = 0;
            g_calibration_state.is_stable = 0;
        }
    } else {
        uint8_t stable_now = 0;

        if (!adc_tracking_valid) {
            g_calibration_state.stable_count = 1;
            g_calibration_state.is_stable = 0;
            adc_tracking_valid = 1;
        } else {
            if (g_calibration_state.current_step == CAL_STEP_ZERO) {
                stable_now = (uint8_t)(
                    Calibration_IsValueStable(current_fwd, last_fwd, CAL_STABLE_ADC_REL_TOL, CAL_STABLE_ADC_ABS_TOL) &&
                    Calibration_IsValueStable(current_ref, last_ref, CAL_STABLE_ADC_REL_TOL, CAL_STABLE_ADC_ABS_TOL));
            } else if (g_calibration_state.current_step == CAL_STEP_POWER) {
                if (g_calibration_state.current_channel == 0) {
                    stable_now = Calibration_IsValueStable(current_fwd, last_fwd, CAL_STABLE_ADC_REL_TOL, CAL_STABLE_ADC_ABS_TOL);
                } else {
                    stable_now = Calibration_IsValueStable(current_ref, last_ref, CAL_STABLE_ADC_REL_TOL, CAL_STABLE_ADC_ABS_TOL);
                }
            }

            if (stable_now) {
                g_calibration_state.stable_count++;
                if (g_calibration_state.stable_count >= CAL_STABLE_ADC_COUNT) {
                    g_calibration_state.is_stable = 1;
                }
            } else {
                g_calibration_state.stable_count = 1;
                g_calibration_state.is_stable = 0;
            }
        }

        last_fwd = current_fwd;
        last_ref = current_ref;
    }

    // 如果正在采样，累计数据
    if (g_calibration_state.sample_count > 0) {
        g_calibration_state.sample_sum_fwd += current_fwd;
        g_calibration_state.sample_sum_ref += current_ref;
        g_calibration_state.sample_count++;

        // 采样完成（10次）
        if (g_calibration_state.sample_count >= (CAL_SAMPLE_TOTAL_COUNT + 1)) {
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
