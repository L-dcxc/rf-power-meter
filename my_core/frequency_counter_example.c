/**
 * @file frequency_counter_example.c
 * @brief 频率计使用示例代码
 * @author 你的名字
 * @date 2025-01-10
 * 
 * 这个文件展示了如何在main.c中使用频率计模块
 * 注意：这个文件仅作为参考，实际使用时需要将代码复制到main.c的相应位置
 */

#include "frequency_counter.h"
#include "main.h"

/**
 * @brief 在main.c中需要添加的代码示例
 */

/* 
 * 1. 在main.c顶部添加头文件包含：
 * #include "frequency_counter.h"
 */

/*
 * 2. 在main()函数的初始化部分添加：
 */
void FreqCounter_MainInit_Example(void)
{
    // 初始化频率计
    if (FreqCounter_Init() == 0) {
        // 初始化成功，启动频率测量
        if (FreqCounter_Start() == 0) {
            // 启动成功
            printf("频率计启动成功\r\n");
        } else {
            // 启动失败
            printf("频率计启动失败\r\n");
        }
    } else {
        // 初始化失败
        printf("频率计初始化失败\r\n");
    }
}

/*
 * 3. 在main()函数的主循环中添加：
 */
void FreqCounter_MainLoop_Example(void)
{
    static uint32_t last_tick = 0;
    
    // 每100ms检查一次频率测量结果
    if (HAL_GetTick() - last_tick >= 100) {
        last_tick = HAL_GetTick();
        
        // 检查是否有新的测量结果
        if (FreqCounter_IsNewResult()) {
            FreqResult_t freq_result;
            
            // 获取测量结果
            if (FreqCounter_GetResult(&freq_result) == 0) {
                // 打印频率结果
                printf("频率: %.2f %s\r\n", 
                       freq_result.frequency_display,
                       FreqCounter_GetUnitString(freq_result.unit));
                
                // 也可以获取原始Hz值
                uint32_t freq_hz = FreqCounter_GetFrequencyHz();
                printf("原始频率: %lu Hz\r\n", freq_hz);
            }
            
            // 清除新结果标志
            FreqCounter_ClearNewResultFlag();
        }
    }
}

/*
 * 4. 在stm32f1xx_it.c中的中断处理函数中添加回调：
 * 
 * 在HAL_TIM_PeriodElapsedCallback函数中添加：
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

/*
 * 5. 如果需要停止频率测量：
 */
void FreqCounter_Stop_Example(void)
{
    if (FreqCounter_Stop() == 0) {
        printf("频率计已停止\r\n");
    }
}

/*
 * 6. 如果需要重新启动频率测量：
 */
void FreqCounter_Restart_Example(void)
{
    // 先停止
    FreqCounter_Stop();
    
    // 延时一下
    HAL_Delay(10);
    
    // 重新启动
    if (FreqCounter_Start() == 0) {
        printf("频率计重新启动成功\r\n");
    }
}

/*
 * 7. 检查频率计运行状态：
 */
void FreqCounter_CheckStatus_Example(void)
{
    if (FreqCounter_IsRunning()) {
        printf("频率计正在运行\r\n");
    } else {
        printf("频率计已停止\r\n");
    }
}

/**
 * @brief 完整的main.c集成示例
 * @note 这是一个完整的集成示例，展示如何在main.c中使用频率计
 */
/*
// 在main.c中的完整示例：

#include "main.h"
#include "frequency_counter.h"  // 添加这行

int main(void)
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
    
    // 初始化频率计
    FreqCounter_Init();
    FreqCounter_Start();
    
    // 主循环
    while (1)
    {
        // 检查频率测量结果
        if (FreqCounter_IsNewResult()) {
            FreqResult_t result;
            if (FreqCounter_GetResult(&result) == 0) {
                // 处理频率结果，比如更新显示
                // 这里可以调用你的显示函数
            }
            FreqCounter_ClearNewResultFlag();
        }
        
        // 其他主循环代码...
        
        HAL_Delay(10);  // 10ms延时
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
