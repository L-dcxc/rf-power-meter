/**
 * @file interface_manager.h
 * @brief 射频功率计界面管理系统头文件
 * @author 蝙蝠鸭
 * @date 2025-08-10
 * 
 * 功能说明：
 * 1. 管理主界面显示（功率、驻波比、频率等参数）
 * 2. 管理设置菜单系统
 * 3. 处理按键响应和界面切换
 * 4. 提供颜色警示功能
 */

#ifndef __INTERFACE_MANAGER_H
#define __INTERFACE_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

/* 包含必要的头文件 */
#include "main.h"
#include "ST7567A.h"
#include "frequency_counter.h"

/* 宏定义 */
#define MAX_MENU_ITEMS      6       // 最大菜单项数量
#define DISPLAY_UPDATE_MS   100     // 显示更新间隔(ms)

/* 界面索引枚举 */
typedef enum {
    INTERFACE_MAIN = 0,         // 主界面
    INTERFACE_MENU,             // 设置菜单
    INTERFACE_CALIBRATION,      // 校准功能
    INTERFACE_CAL_CONFIRM,      // 校准确认页
    INTERFACE_CAL_ZERO,         // 零点校准
    INTERFACE_CAL_POWER,        // 功率斜率校准
    INTERFACE_CAL_BAND,         // 频段增益修正
    INTERFACE_CAL_REFLECT,      // 反射通道定标
    INTERFACE_CAL_FREQ,         // 频率计微调
    INTERFACE_CAL_COMPLETE,     // 校准完成
    INTERFACE_STANDARD,         // 标定设置
    INTERFACE_ALARM,            // 超限报警
    INTERFACE_BRIGHTNESS,       // 显示亮度
    INTERFACE_ABOUT             // 关于设备
} InterfaceIndex_t;

/* 按键枚举 */
typedef enum {
    KEY_NONE = 0,               // 无按键
    KEY_UP,                     // 向上/返回键 (PB12)
    KEY_OK,                     // 确认键 (PB13)
    KEY_DOWN                    // 向下/切换键 (PB14)
} KeyValue_t;

/* 按键状态枚举 */
typedef enum {
    KEY_STATE_IDLE = 0,         // 空闲状态
    KEY_STATE_PRESSED,          // 按下状态
    KEY_STATE_LONG_PRESSED      // 长按状态
} KeyState_t;

/* VSWR警示颜色枚举 */
typedef enum {
    VSWR_COLOR_GREEN = 0,       // 绿色 (≤1.5)
    VSWR_COLOR_YELLOW,          // 黄色 (1.5-2.0)
    VSWR_COLOR_RED              // 红色 (>2.0)
} VSWRColor_t;

/* 功率单位枚举 */
typedef enum {
    POWER_UNIT_W = 0,           // 瓦特
    POWER_UNIT_KW               // 千瓦
} PowerUnit_t;

/* 功率测量结果结构体 */
typedef struct {
    float forward_power;        // 正向功率
    float reflected_power;      // 反射功率
    PowerUnit_t forward_unit;   // 正向功率单位
    PowerUnit_t reflected_unit; // 反射功率单位
    uint8_t is_valid;          // 数据是否有效
} PowerResult_t;

/* 射频参数结构体 */
typedef struct {
    float vswr;                 // 驻波比
    float reflection_coeff;     // 反射系数
    float transmission_eff;     // 传输效率(%)
    VSWRColor_t vswr_color;     // VSWR颜色警示
    uint8_t is_valid;          // 数据是否有效
} RFParams_t;

/* 菜单项结构体 */
typedef struct {
    uint8_t index;              // 菜单索引
    uint8_t parent;             // 父菜单索引
    uint8_t next;               // 下一个菜单索引
    void (*display_func)(void); // 显示函数指针
    const char* title;          // 菜单标题
} MenuItem_t;

/* 蜂鸣器状态结构体 */
typedef struct {
    uint8_t is_active;          // 蜂鸣器是否激活
    uint16_t duration_count;    // 剩余持续时间(ms)
} BuzzerState_t;

/* 校准步骤枚举 */
typedef enum {
    CAL_STEP_CONFIRM = 0,       // 确认进入校准
    CAL_STEP_ZERO,              // 零点校准
    CAL_STEP_POWER,             // 功率斜率校准
    CAL_STEP_BAND,              // 频段增益修正
    CAL_STEP_REFLECT,           // 反射通道定标
    CAL_STEP_FREQ,              // 频率计微调
    CAL_STEP_COMPLETE           // 校准完成
} CalibrationStep_t;

/* 校准数据结构体 */
/* 功率校准点结构体 */
typedef struct {
    float power;                // 功率值(W)
    float voltage;              // 对应电压(V)
} PowerCalPoint_t;

typedef struct {
    float forward_offset;       // 正向功率零点偏移(V)
    float reflected_offset;     // 反射功率零点偏移(V)
    PowerCalPoint_t fwd_table[5];    // 正向功率校准表(100W-500W)
    PowerCalPoint_t ref_table[5];    // 反射功率校准表
    uint8_t fwd_points;         // 正向校准点数(最多5个)
    uint8_t ref_points;         // 反射校准点数(最多5个)
    float band_gain_fwd[11];    // 各频段正向增益修正系数
    float band_gain_ref[11];    // 各频段反射增益修正系数
    float freq_trim;            // 频率计微调系数
    uint8_t is_calibrated;      // 校准完成标志
} CalibrationData_t;

/* 校准状态结构体 */
typedef struct {
    CalibrationStep_t current_step;     // 当前校准步骤
    uint8_t current_band;               // 当前频段索引(0-10)
    uint8_t current_power_point;        // 当前功率校准点(0-4)
    uint8_t current_channel;            // 当前校准通道(0=正向,1=反射)
    uint8_t sample_count;               // 采样计数
    float sample_sum_fwd;               // 正向功率采样累计
    float sample_sum_ref;               // 反射功率采样累计
    uint8_t is_stable;                  // 读数稳定标志
    uint16_t stable_count;              // 稳定计数器
    float target_power;                 // 目标功率值
    uint8_t power_cal_mode;             // 功率校准模式(0=正向,1=反射)
} CalibrationState_t;

/* 界面管理器状态结构体 */
typedef struct {
    InterfaceIndex_t current_interface;     // 当前界面
    uint8_t menu_cursor;                    // 菜单光标位置
    uint8_t brightness_level;               // 亮度等级(0-10)
    uint8_t alarm_enabled;                  // 报警使能
    float vswr_alarm_threshold;             // VSWR报警阈值
    uint8_t alarm_selected_item;            // 报警设置选中项 (0=开关, 1=阈值)
    uint32_t last_update_time;              // 上次更新时间
    uint8_t need_refresh;                   // 需要刷新标志
} InterfaceManager_t;

/* 全局变量声明 */
extern InterfaceManager_t g_interface_manager;
extern PowerResult_t g_power_result;
extern RFParams_t g_rf_params;
extern BuzzerState_t g_buzzer_state;
extern CalibrationData_t g_calibration_data;
extern CalibrationState_t g_calibration_state;

/* 函数声明 */

/**
 * @brief 界面管理器初始化
 * @param 无
 * @retval 0: 成功, -1: 失败
 */
int8_t InterfaceManager_Init(void);

/**
 * @brief 界面管理器主循环处理
 * @param 无
 * @retval 无
 * @note 在main函数主循环中调用
 */
void InterfaceManager_Process(void);

/**
 * @brief 按键处理函数
 * @param key: 按键值
 * @param state: 按键状态
 * @retval 无
 */
void InterfaceManager_HandleKey(KeyValue_t key, KeyState_t state);

/**
 * @brief 切换到指定界面
 * @param interface: 目标界面索引
 * @retval 无
 */
void InterfaceManager_SwitchTo(InterfaceIndex_t interface);

/**
 * @brief 强制刷新当前界面
 * @param 无
 * @retval 无
 */
void InterfaceManager_ForceRefresh(void);

/**
 * @brief 更新功率数据
 * @param forward_power: 正向功率
 * @param reflected_power: 反射功率
 * @retval 无
 */
void InterfaceManager_UpdatePower(float forward_power, float reflected_power);

/**
 * @brief 更新射频参数
 * @param vswr: 驻波比
 * @param reflection_coeff: 反射系数
 * @param transmission_eff: 传输效率
 * @retval 无
 */
void InterfaceManager_UpdateRFParams(float vswr, float reflection_coeff, float transmission_eff);

/**
 * @brief 获取按键值
 * @param 无
 * @retval 按键值
 */
KeyValue_t InterfaceManager_GetKey(void);

/**
 * @brief 设置背光亮度
 * @param level: 亮度等级(0-10)
 * @retval 无
 */
void InterfaceManager_SetBrightness(uint8_t level);

/**
 * @brief 触发蜂鸣器
 * @param duration_ms: 持续时间(ms)
 * @retval 无
 */
void InterfaceManager_Beep(uint16_t duration_ms);

/**
 * @brief 蜂鸣器定时处理函数（在TIM3中断中调用）
 * @param 无
 * @retval 无
 */
void InterfaceManager_BuzzerProcess(void);

/* 界面显示函数声明 */
void Interface_DisplayMain(void);           // 主界面显示
void Interface_DisplayMenu(void);           // 菜单界面显示
void Interface_DisplayCalibration(void);    // 校准界面显示
void Interface_DisplayCalConfirm(void);     // 校准确认页显示
void Interface_DisplayCalZero(void);        // 零点校准显示
void Interface_DisplayCalPower(void);       // 功率斜率校准显示
void Interface_DisplayCalBand(void);        // 频段增益修正显示
void Interface_DisplayCalReflect(void);     // 反射通道定标显示
void Interface_DisplayCalFreq(void);        // 频率计微调显示
void Interface_DisplayCalComplete(void);    // 校准完成显示
void Interface_DisplayStandard(void);       // 标定界面显示
void Interface_DisplayAlarm(void);          // 报警设置界面显示
void Interface_DisplayBrightness(void);     // 亮度设置界面显示
void Interface_DisplayAbout(void);          // 关于界面显示

/* 校准功能函数声明 */
void Calibration_Init(void);                // 校准系统初始化
void Calibration_LoadFromEEPROM(void);      // 从EEPROM加载校准数据
void Calibration_SaveToEEPROM(void);        // 保存校准数据到EEPROM
void Calibration_StartStep(CalibrationStep_t step);  // 开始校准步骤
void Calibration_ProcessSample(void);       // 处理校准采样
uint8_t Calibration_GetBandIndex(float frequency);   // 获取频段索引
float Calibration_CalculatePowerFromTable(float voltage, PowerCalPoint_t* table, uint8_t points);  // 查表计算功率
float Calibration_ApplyCorrection(float raw_voltage, uint8_t is_forward, float frequency);  // 应用校准修正

// 系统启动界面
void System_BootSequence(void);             // 系统启动序列界面

#ifdef __cplusplus
}
#endif

#endif /* __INTERFACE_MANAGER_H */
