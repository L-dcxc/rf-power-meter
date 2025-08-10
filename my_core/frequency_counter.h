/**
 * @file frequency_counter.h
 * @brief 频率计功能模块头文件
 * @author 你的名字
 * @date 2025-01-10
 * 
 * 功能说明：
 * 1. 使用TIM2外部时钟模式计数PA0输入的脉冲
 * 2. 使用TIM4每秒触发一次读取计数值
 * 3. 支持1Hz-100MHz频率测量
 * 4. 自动处理定时器溢出情况
 */

#ifndef __FREQUENCY_COUNTER_H
#define __FREQUENCY_COUNTER_H

#ifdef __cplusplus
extern "C" {
#endif

/* 包含必要的头文件 */
#include "main.h"
#include "tim.h"

/* 宏定义 */
#define FREQ_COUNTER_MAX_OVERFLOW   1000    // 最大溢出次数
#define FREQ_COUNTER_TIM2_MAX       65536   // TIM2最大计数值(16位定时器)
#define FREQ_COUNTER_PRESCALER      1       // 预分频系数(当前不分频)

/* 频率单位枚举 */
typedef enum {
    FREQ_UNIT_HZ = 0,       // Hz
    FREQ_UNIT_KHZ,          // KHz  
    FREQ_UNIT_MHZ           // MHz
} FreqUnit_t;

/* 频率测量结果结构体 */
typedef struct {
    uint32_t frequency_hz;      // 频率值(Hz)
    float frequency_display;    // 显示用频率值(根据单位自动转换)
    FreqUnit_t unit;           // 频率单位
    uint8_t is_valid;          // 测量结果是否有效(1=有效, 0=无效)
} FreqResult_t;

/* 频率计状态结构体 */
typedef struct {
    uint32_t tim2_count;           // TIM2当前计数值
    uint32_t tim2_overflow_count;  // TIM2溢出次数
    uint32_t last_frequency;       // 上次测量的频率
    uint8_t measurement_ready;     // 测量结果准备标志
    uint8_t is_running;           // 频率计运行状态
} FreqCounter_t;

/* 全局变量声明 */
extern FreqCounter_t g_freq_counter;
extern FreqResult_t g_freq_result;

/* 函数声明 */

/**
 * @brief 频率计初始化
 * @param 无
 * @retval 0: 成功, -1: 失败
 * @note 初始化定时器和相关变量，但不启动测量
 */
int8_t FreqCounter_Init(void);

/**
 * @brief 启动频率计测量
 * @param 无
 * @retval 0: 成功, -1: 失败
 * @note 启动TIM2和TIM4，开始频率测量
 */
int8_t FreqCounter_Start(void);

/**
 * @brief 停止频率计测量
 * @param 无
 * @retval 0: 成功, -1: 失败
 * @note 停止TIM2和TIM4，停止频率测量
 */
int8_t FreqCounter_Stop(void);

/**
 * @brief 获取最新的频率测量结果
 * @param result: 指向结果结构体的指针
 * @retval 0: 成功获取, -1: 无有效数据
 * @note 返回最新一次的频率测量结果
 */
int8_t FreqCounter_GetResult(FreqResult_t *result);

/**
 * @brief 获取原始频率值(Hz)
 * @param 无
 * @retval 频率值(Hz), 0表示无效
 * @note 直接返回Hz单位的频率值
 */
uint32_t FreqCounter_GetFrequencyHz(void);

/**
 * @brief 检查是否有新的测量结果
 * @param 无
 * @retval 1: 有新结果, 0: 无新结果
 * @note 用于判断是否需要更新显示
 */
uint8_t FreqCounter_IsNewResult(void);

/**
 * @brief 清除新结果标志
 * @param 无
 * @retval 无
 * @note 读取结果后调用，清除新结果标志
 */
void FreqCounter_ClearNewResultFlag(void);

/**
 * @brief TIM4中断回调函数(1秒定时)
 * @param htim: 定时器句柄指针
 * @retval 无
 * @note 在TIM4中断中调用，用于读取TIM2计数值并计算频率
 */
void FreqCounter_TIM4_Callback(TIM_HandleTypeDef *htim);

/**
 * @brief TIM2溢出中断回调函数
 * @param htim: 定时器句柄指针  
 * @retval 无
 * @note 在TIM2溢出中断中调用，用于记录溢出次数
 */
void FreqCounter_TIM2_Callback(TIM_HandleTypeDef *htim);

/**
 * @brief 频率值转换为合适的显示单位
 * @param frequency_hz: 输入频率(Hz)
 * @param result: 输出结果结构体指针
 * @retval 无
 * @note 自动选择合适的单位(Hz/KHz/MHz)进行显示
 */
void FreqCounter_ConvertUnit(uint32_t frequency_hz, FreqResult_t *result);

/**
 * @brief 获取频率计运行状态
 * @param 无
 * @retval 1: 正在运行, 0: 已停止
 */
uint8_t FreqCounter_IsRunning(void);

/**
 * @brief 获取频率单位字符串
 * @param unit: 频率单位枚举
 * @retval 单位字符串指针
 * @note 用于显示时获取单位字符串
 */
const char* FreqCounter_GetUnitString(FreqUnit_t unit);

#ifdef __cplusplus
}
#endif

#endif /* __FREQUENCY_COUNTER_H */
