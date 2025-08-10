/**
 * @file frequency_counter_example.c
 * @brief Ƶ�ʼ�ʹ��ʾ������
 * @author �������
 * @date 2025-01-10
 * 
 * ����ļ�չʾ�������main.c��ʹ��Ƶ�ʼ�ģ��
 * ע�⣺����ļ�����Ϊ�ο���ʵ��ʹ��ʱ��Ҫ�����븴�Ƶ�main.c����Ӧλ��
 */

#include "frequency_counter.h"
#include "main.h"

/**
 * @brief ��main.c����Ҫ��ӵĴ���ʾ��
 */

/* 
 * 1. ��main.c�������ͷ�ļ�������
 * #include "frequency_counter.h"
 */

/*
 * 2. ��main()�����ĳ�ʼ��������ӣ�
 */
void FreqCounter_MainInit_Example(void)
{
    // ��ʼ��Ƶ�ʼ�
    if (FreqCounter_Init() == 0) {
        // ��ʼ���ɹ�������Ƶ�ʲ���
        if (FreqCounter_Start() == 0) {
            // �����ɹ�
            printf("Ƶ�ʼ������ɹ�\r\n");
        } else {
            // ����ʧ��
            printf("Ƶ�ʼ�����ʧ��\r\n");
        }
    } else {
        // ��ʼ��ʧ��
        printf("Ƶ�ʼƳ�ʼ��ʧ��\r\n");
    }
}

/*
 * 3. ��main()��������ѭ������ӣ�
 */
void FreqCounter_MainLoop_Example(void)
{
    static uint32_t last_tick = 0;
    
    // ÿ100ms���һ��Ƶ�ʲ������
    if (HAL_GetTick() - last_tick >= 100) {
        last_tick = HAL_GetTick();
        
        // ����Ƿ����µĲ������
        if (FreqCounter_IsNewResult()) {
            FreqResult_t freq_result;
            
            // ��ȡ�������
            if (FreqCounter_GetResult(&freq_result) == 0) {
                // ��ӡƵ�ʽ��
                printf("Ƶ��: %.2f %s\r\n", 
                       freq_result.frequency_display,
                       FreqCounter_GetUnitString(freq_result.unit));
                
                // Ҳ���Ի�ȡԭʼHzֵ
                uint32_t freq_hz = FreqCounter_GetFrequencyHz();
                printf("ԭʼƵ��: %lu Hz\r\n", freq_hz);
            }
            
            // ����½����־
            FreqCounter_ClearNewResultFlag();
        }
    }
}

/*
 * 4. ��stm32f1xx_it.c�е��жϴ���������ӻص���
 * 
 * ��HAL_TIM_PeriodElapsedCallback��������ӣ�
 */
void HAL_TIM_PeriodElapsedCallback_Example(TIM_HandleTypeDef *htim)
{
    // TIM4�жϻص� (1�붨ʱ)
    if (htim->Instance == TIM4) {
        FreqCounter_TIM4_Callback(htim);
    }
    
    // TIM2����жϻص�
    if (htim->Instance == TIM2) {
        FreqCounter_TIM2_Callback(htim);
    }
}

/*
 * 5. �����ҪֹͣƵ�ʲ�����
 */
void FreqCounter_Stop_Example(void)
{
    if (FreqCounter_Stop() == 0) {
        printf("Ƶ�ʼ���ֹͣ\r\n");
    }
}

/*
 * 6. �����Ҫ��������Ƶ�ʲ�����
 */
void FreqCounter_Restart_Example(void)
{
    // ��ֹͣ
    FreqCounter_Stop();
    
    // ��ʱһ��
    HAL_Delay(10);
    
    // ��������
    if (FreqCounter_Start() == 0) {
        printf("Ƶ�ʼ����������ɹ�\r\n");
    }
}

/*
 * 7. ���Ƶ�ʼ�����״̬��
 */
void FreqCounter_CheckStatus_Example(void)
{
    if (FreqCounter_IsRunning()) {
        printf("Ƶ�ʼ���������\r\n");
    } else {
        printf("Ƶ�ʼ���ֹͣ\r\n");
    }
}

/**
 * @brief ������main.c����ʾ��
 * @note ����һ�������ļ���ʾ����չʾ�����main.c��ʹ��Ƶ�ʼ�
 */
/*
// ��main.c�е�����ʾ����

#include "main.h"
#include "frequency_counter.h"  // �������

int main(void)
{
    // HAL���ʼ��
    HAL_Init();
    
    // ϵͳʱ������
    SystemClock_Config();
    
    // �����ʼ��
    MX_GPIO_Init();
    MX_TIM2_Init();
    MX_TIM3_Init();
    MX_TIM4_Init();
    MX_SPI1_Init();
    MX_USART1_UART_Init();
    MX_ADC1_Init();
    MX_I2C2_Init();
    
    // ��ʼ��Ƶ�ʼ�
    FreqCounter_Init();
    FreqCounter_Start();
    
    // ��ѭ��
    while (1)
    {
        // ���Ƶ�ʲ������
        if (FreqCounter_IsNewResult()) {
            FreqResult_t result;
            if (FreqCounter_GetResult(&result) == 0) {
                // ����Ƶ�ʽ�������������ʾ
                // ������Ե��������ʾ����
            }
            FreqCounter_ClearNewResultFlag();
        }
        
        // ������ѭ������...
        
        HAL_Delay(10);  // 10ms��ʱ
    }
}

// ��stm32f1xx_it.c����ӣ�
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
