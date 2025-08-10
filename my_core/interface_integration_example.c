/**
 * @file interface_integration_example.c
 * @brief 界面管理系统集成示例
 * @author 你的名字
 * @date 2025-01-10
 * 
 * 这个文件展示了如何在main.c中集成界面管理系统和频率计
 * 注意：这个文件仅作为参考，实际使用时需要将代码复制到main.c的相应位置
 */

#include "interface_manager.h"
#include "frequency_counter.h"
#include "main.h"

/**
 * @brief 在main.c中需要添加的完整集成示例
 */

/*
// 在main.c顶部添加头文件包含：
#include "frequency_counter.h"
#include "interface_manager.h"
*/

/*
// 在main()函数的初始化部分添加：
*/
void System_Init_Example(void)
{
    // HAL库初始化
    HAL_Init();
    
    // 系统时钟配置
    SystemClock_Config();
    
    // 外设初始化
    MX_GPIO_Init();
    MX_TIM2_Init();
    MX_TIM3_Init();
    MX_TIM4_Init();
    MX_SPI1_Init();
    MX_USART1_UART_Init();
    MX_ADC1_Init();
    MX_I2C2_Init();
    
    // 初始化LCD
    LCD_Init();
    LCD_SetBacklight(8000);  // 设置背光亮度为80%
    LCD_Clear(BLACK);
    
    // 初始化频率计
    if (FreqCounter_Init() != 0) {
        // 频率计初始化失败处理
        Show_Str(10, 50, RED, BLACK, (uint8_t*)"频率计初始化失败!", 16, 0);
        while(1);  // 停止运行
    }
    
    if (FreqCounter_Start() != 0) {
        // 频率计启动失败处理
        Show_Str(10, 70, RED, BLACK, (uint8_t*)"频率计启动失败!", 16, 0);
        while(1);  // 停止运行
    }
    
    // 初始化界面管理器
    if (InterfaceManager_Init() != 0) {
        // 界面管理器初始化失败处理
        Show_Str(10, 90, RED, BLACK, (uint8_t*)"界面初始化失败!", 16, 0);
        while(1);  // 停止运行
    }
    
    // 显示启动成功信息
    Show_Str(30, 50, GREEN, BLACK, (uint8_t*)"系统启动成功", 16, 0);
    HAL_Delay(1000);
    LCD_Clear(BLACK);
}

/*
// 在main()函数的主循环中添加：
*/
void Main_Loop_Example(void)
{
    static uint32_t last_adc_time = 0;
    static uint32_t last_calc_time = 0;
    uint32_t current_time = HAL_GetTick();
    
    // 每100ms读取一次ADC (功率检测)
    if (current_time - last_adc_time >= 100) {
        last_adc_time = current_time;
        
        // 这里添加ADC读取代码
        // 示例：读取PA2和PA3的ADC值
        float forward_voltage = 0.0f;   // 从ADC读取正向功率电压
        float reflected_voltage = 0.0f; // 从ADC读取反射功率电压
        
        // 将电压转换为功率值 (根据你的硬件标定)
        float forward_power = forward_voltage * 10.0f;    // 示例转换公式
        float reflected_power = reflected_voltage * 10.0f; // 示例转换公式
        
        // 更新功率数据到界面管理器
        InterfaceManager_UpdatePower(forward_power, reflected_power);
    }
    
    // 每200ms计算一次射频参数
    if (current_time - last_calc_time >= 200) {
        last_calc_time = current_time;
        
        // 计算射频参数
        if (g_power_result.is_valid && g_power_result.forward_power > 0.01f) {
            // 计算反射系数 Γ = √(Pr/Pi)
            float reflection_coeff = sqrtf(g_power_result.reflected_power / g_power_result.forward_power);
            
            // 计算驻波比 VSWR = (1 + |Γ|) / (1 - |Γ|)
            float vswr = (1.0f + reflection_coeff) / (1.0f - reflection_coeff);
            if (vswr < 1.0f) vswr = 1.0f;  // VSWR最小值为1
            if (vswr > 10.0f) vswr = 10.0f; // 限制最大值
            
            // 计算传输效率 η = 1 - Pr/Pi
            float transmission_eff = (1.0f - g_power_result.reflected_power / g_power_result.forward_power) * 100.0f;
            if (transmission_eff < 0.0f) transmission_eff = 0.0f;
            if (transmission_eff > 100.0f) transmission_eff = 100.0f;
            
            // 更新射频参数到界面管理器
            InterfaceManager_UpdateRFParams(vswr, reflection_coeff, transmission_eff);
        }
    }
    
    // 处理界面管理器 (包括显示更新和按键处理)
    InterfaceManager_Process();
    
    // 检查频率计新结果
    if (FreqCounter_IsNewResult()) {
        // 频率结果会在界面显示中自动获取
        FreqCounter_ClearNewResultFlag();
    }
    
    // 短暂延时
    HAL_Delay(10);
}

/*
// 在stm32f1xx_it.c中添加中断回调函数：
*/
void HAL_TIM_PeriodElapsedCallback_Example(TIM_HandleTypeDef *htim)
{
    // TIM4中断回调 (1秒定时)
    if (htim->Instance == TIM4) {
        FreqCounter_TIM4_Callback(htim);
    }
    
    // TIM2溢出中断回调
    if (htim->Instance == TIM2) {
        FreqCounter_TIM2_Callback(htim);
    }
}

/**
 * @brief 完整的main.c集成示例
 */
/*
#include "main.h"
#include "frequency_counter.h"
#include "interface_manager.h"
#include <math.h>

int main(void)
{
    // 系统初始化
    HAL_Init();
    SystemClock_Config();
    
    // 外设初始化
    MX_GPIO_Init();
    MX_DMA_Init();
    MX_TIM2_Init();
    MX_TIM3_Init();
    MX_TIM4_Init();
    MX_SPI1_Init();
    MX_USART1_UART_Init();
    MX_ADC1_Init();
    MX_I2C2_Init();
    MX_IWDG_Init();
    
    // 初始化LCD
    LCD_Init();
    LCD_SetBacklight(8000);
    LCD_Clear(BLACK);
    
    // 初始化频率计
    FreqCounter_Init();
    FreqCounter_Start();
    
    // 初始化界面管理器
    InterfaceManager_Init();
    
    // 启动PWM (背光控制)
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
    
    // 主循环
    while (1)
    {
        static uint32_t last_adc_time = 0;
        static uint32_t last_calc_time = 0;
        uint32_t current_time = HAL_GetTick();
        
        // ADC采集和功率计算
        if (current_time - last_adc_time >= 100) {
            last_adc_time = current_time;
            
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
            float forward_voltage = (float)adc_forward * 3.3f / 4095.0f;
            float reflected_voltage = (float)adc_reflected * 3.3f / 4095.0f;
            
            // 转换为功率值 (根据你的硬件标定)
            float forward_power = forward_voltage * 10.0f;    // 示例：10W/V
            float reflected_power = reflected_voltage * 10.0f;
            
            // 更新功率数据
            InterfaceManager_UpdatePower(forward_power, reflected_power);
        }
        
        // 射频参数计算
        if (current_time - last_calc_time >= 200) {
            last_calc_time = current_time;
            
            if (g_power_result.is_valid && g_power_result.forward_power > 0.01f) {
                float reflection_coeff = sqrtf(g_power_result.reflected_power / g_power_result.forward_power);
                float vswr = (1.0f + reflection_coeff) / (1.0f - reflection_coeff);
                float transmission_eff = (1.0f - g_power_result.reflected_power / g_power_result.forward_power) * 100.0f;
                
                InterfaceManager_UpdateRFParams(vswr, reflection_coeff, transmission_eff);
            }
        }
        
        // 界面处理
        InterfaceManager_Process();
        
        // 频率计处理
        if (FreqCounter_IsNewResult()) {
            FreqCounter_ClearNewResultFlag();
        }
        
        // 喂狗
        HAL_IWDG_Refresh(&hiwdg);
        
        HAL_Delay(10);
    }
}

// 在stm32f1xx_it.c中添加：
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4) {
        FreqCounter_TIM4_Callback(htim);
    }
    if (htim->Instance == TIM2) {
        FreqCounter_TIM2_Callback(htim);
    }
}
*/
