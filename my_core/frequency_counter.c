/**
 * @file frequency_counter.c
 * @brief 频率计功能模块实现文件
 * @author 蝙蝠鸭
 * @date 2025-08-10
 * 
 * 实现原理：
 * 1. TIM2配置为外部时钟模式，PA0输入的脉冲直接作为计数时钟
 * 2. TIM4配置为1秒定时器，每秒触发一次中断
 * 3. 在TIM4中断中读取TIM2的计数值，计算1秒内的脉冲数即为频率
 * 4. 考虑TIM2可能的溢出情况，通过溢出计数进行补偿
 */

#include "frequency_counter.h"

/* 全局变量定义 */
FreqCounter_t g_freq_counter = {0};     // 频率计状态结构体
FreqResult_t g_freq_result = {0};       // 频率测量结果结构体

/**
 * @brief 频率计初始化
 * @param 无
 * @retval 0: 成功, -1: 失败
 */
int8_t FreqCounter_Init(void)
{
    // 清零所有状态变量
    g_freq_counter.tim2_count = 0;
    g_freq_counter.tim2_overflow_count = 0;
    g_freq_counter.last_frequency = 0;
    g_freq_counter.measurement_ready = 0;
    g_freq_counter.is_running = 0;
    
    // 清零测量结果
    g_freq_result.frequency_hz = 0;
    g_freq_result.frequency_display = 0.0f;
    g_freq_result.unit = FREQ_UNIT_HZ;
    g_freq_result.is_valid = 0;
    
    // 重置TIM2计数器
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    
    // 重置TIM4计数器  
    __HAL_TIM_SET_COUNTER(&htim4, 0);
    
    return 0;
}

/**
 * @brief 启动频率计测量
 * @param 无
 * @retval 0: 成功, -1: 失败
 */
int8_t FreqCounter_Start(void)
{
    // 如果已经在运行，先停止
    if (g_freq_counter.is_running) {
        FreqCounter_Stop();
    }
    
    // 重新初始化状态
    FreqCounter_Init();
    
    // 启动TIM2 (外部时钟计数)
    if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK) {
        return -1;
    }
    
    // 启动TIM4 (1秒定时器)
    if (HAL_TIM_Base_Start_IT(&htim4) != HAL_OK) {
        HAL_TIM_Base_Stop_IT(&htim2);  // 如果TIM4启动失败，停止TIM2
        return -1;
    }
    
    // 设置运行状态
    g_freq_counter.is_running = 1;
    
    return 0;
}

/**
 * @brief 停止频率计测量
 * @param 无
 * @retval 0: 成功, -1: 失败
 */
int8_t FreqCounter_Stop(void)
{
    // 停止TIM2
    HAL_TIM_Base_Stop_IT(&htim2);
    
    // 停止TIM4
    HAL_TIM_Base_Stop_IT(&htim4);
    
    // 清除运行状态
    g_freq_counter.is_running = 0;
    
    return 0;
}

/**
 * @brief 获取最新的频率测量结果
 * @param result: 指向结果结构体的指针
 * @retval 0: 成功获取, -1: 无有效数据
 */
int8_t FreqCounter_GetResult(FreqResult_t *result)
{
    // 检查输入参数
    if (result == NULL) {
        return -1;
    }
    
    // 检查是否有有效数据
    if (!g_freq_result.is_valid) {
        return -1;
    }
    
    // 复制结果
    *result = g_freq_result;
    
    return 0;
}

/**
 * @brief 获取原始频率值(Hz)
 * @param 无
 * @retval 频率值(Hz), 0表示无效
 */
uint32_t FreqCounter_GetFrequencyHz(void)
{
    if (g_freq_result.is_valid) {
        return g_freq_result.frequency_hz;
    }
    return 0;
}

/**
 * @brief 检查是否有新的测量结果
 * @param 无
 * @retval 1: 有新结果, 0: 无新结果
 */
uint8_t FreqCounter_IsNewResult(void)
{
    return g_freq_counter.measurement_ready;
}

/**
 * @brief 清除新结果标志
 * @param 无
 * @retval 无
 */
void FreqCounter_ClearNewResultFlag(void)
{
    g_freq_counter.measurement_ready = 0;
}

/**
 * @brief 频率值转换为合适的显示单位
 * @param frequency_hz: 输入频率(Hz)
 * @param result: 输出结果结构体指针
 * @retval 无
 */
void FreqCounter_ConvertUnit(uint32_t frequency_hz, FreqResult_t *result)
{
    result->frequency_hz = frequency_hz;
    
    if (frequency_hz >= 1000000) {
        // >= 1MHz，使用MHz单位
        result->frequency_display = (float)frequency_hz / 1000000.0f;
        result->unit = FREQ_UNIT_MHZ;
    }
    else if (frequency_hz >= 1000) {
        // >= 1KHz，使用KHz单位
        result->frequency_display = (float)frequency_hz / 1000.0f;
        result->unit = FREQ_UNIT_KHZ;
    }
    else {
        // < 1KHz，使用Hz单位
        result->frequency_display = (float)frequency_hz;
        result->unit = FREQ_UNIT_HZ;
    }
    
    result->is_valid = 1;
}

/**
 * @brief TIM4中断回调函数(1秒定时)
 * @param htim: 定时器句柄指针
 * @retval 无
 * @note 每秒调用一次，读取TIM2计数值并计算频率
 */
void FreqCounter_TIM4_Callback(TIM_HandleTypeDef *htim)
{
    // 确认是TIM4中断
    if (htim->Instance != TIM4) {
        return;
    }

    // 读取TIM2当前计数值
    uint32_t current_count = __HAL_TIM_GET_COUNTER(&htim2);

    // 计算总的脉冲数 = 溢出次数 * 最大计数值 + 当前计数值
    uint32_t total_pulses = g_freq_counter.tim2_overflow_count * FREQ_COUNTER_TIM2_MAX + current_count;

    // 考虑预分频系数(当前为1，不分频)
    uint32_t frequency = total_pulses * FREQ_COUNTER_PRESCALER;

    // 更新频率结果
    FreqCounter_ConvertUnit(frequency, &g_freq_result);

    // 保存到状态结构体
    g_freq_counter.last_frequency = frequency;
    g_freq_counter.measurement_ready = 1;  // 设置新结果标志

    // 重置计数器和溢出计数，准备下一次测量
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    g_freq_counter.tim2_overflow_count = 0;
    g_freq_counter.tim2_count = 0;
}

/**
 * @brief TIM2溢出中断回调函数
 * @param htim: 定时器句柄指针
 * @retval 无
 * @note 当TIM2计数溢出时调用，记录溢出次数
 */
void FreqCounter_TIM2_Callback(TIM_HandleTypeDef *htim)
{
    // 确认是TIM2中断
    if (htim->Instance != TIM2) {
        return;
    }

    // 增加溢出计数
    g_freq_counter.tim2_overflow_count++;

    // 防止溢出计数过大(安全保护)
    if (g_freq_counter.tim2_overflow_count > FREQ_COUNTER_MAX_OVERFLOW) {
        // 如果溢出次数过多，可能是异常情况，重置计数
        g_freq_counter.tim2_overflow_count = 0;
        __HAL_TIM_SET_COUNTER(&htim2, 0);
    }
}

/**
 * @brief 获取频率计运行状态
 * @param 无
 * @retval 1: 正在运行, 0: 已停止
 */
uint8_t FreqCounter_IsRunning(void)
{
    return g_freq_counter.is_running;
}

/**
 * @brief 获取频率单位字符串
 * @param unit: 频率单位枚举
 * @retval 单位字符串指针
 */
const char* FreqCounter_GetUnitString(FreqUnit_t unit)
{
    switch (unit) {
        case FREQ_UNIT_HZ:
            return "Hz";
        case FREQ_UNIT_KHZ:
            return "KHz";
        case FREQ_UNIT_MHZ:
            return "MHz";
        default:
            return "Hz";
    }
}
