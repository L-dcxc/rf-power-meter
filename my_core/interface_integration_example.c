/**
 * @file interface_integration_example.c
 * @brief �������ϵͳ����ʾ��
 * @author �������
 * @date 2025-01-10
 * 
 * ����ļ�չʾ�������main.c�м��ɽ������ϵͳ��Ƶ�ʼ�
 * ע�⣺����ļ�����Ϊ�ο���ʵ��ʹ��ʱ��Ҫ�����븴�Ƶ�main.c����Ӧλ��
 */

#include "interface_manager.h"
#include "frequency_counter.h"
#include "main.h"

/**
 * @brief ��main.c����Ҫ��ӵ���������ʾ��
 */

/*
// ��main.c�������ͷ�ļ�������
#include "frequency_counter.h"
#include "interface_manager.h"
*/

/*
// ��main()�����ĳ�ʼ��������ӣ�
*/
void System_Init_Example(void)
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
    
    // ��ʼ��LCD
    LCD_Init();
    LCD_SetBacklight(8000);  // ���ñ�������Ϊ80%
    LCD_Clear(BLACK);
    
    // ��ʼ��Ƶ�ʼ�
    if (FreqCounter_Init() != 0) {
        // Ƶ�ʼƳ�ʼ��ʧ�ܴ���
        Show_Str(10, 50, RED, BLACK, (uint8_t*)"Ƶ�ʼƳ�ʼ��ʧ��!", 16, 0);
        while(1);  // ֹͣ����
    }
    
    if (FreqCounter_Start() != 0) {
        // Ƶ�ʼ�����ʧ�ܴ���
        Show_Str(10, 70, RED, BLACK, (uint8_t*)"Ƶ�ʼ�����ʧ��!", 16, 0);
        while(1);  // ֹͣ����
    }
    
    // ��ʼ�����������
    if (InterfaceManager_Init() != 0) {
        // �����������ʼ��ʧ�ܴ���
        Show_Str(10, 90, RED, BLACK, (uint8_t*)"�����ʼ��ʧ��!", 16, 0);
        while(1);  // ֹͣ����
    }
    
    // ��ʾ�����ɹ���Ϣ
    Show_Str(30, 50, GREEN, BLACK, (uint8_t*)"ϵͳ�����ɹ�", 16, 0);
    HAL_Delay(1000);
    LCD_Clear(BLACK);
}

/*
// ��main()��������ѭ������ӣ�
*/
void Main_Loop_Example(void)
{
    static uint32_t last_adc_time = 0;
    static uint32_t last_calc_time = 0;
    uint32_t current_time = HAL_GetTick();
    
    // ÿ100ms��ȡһ��ADC (���ʼ��)
    if (current_time - last_adc_time >= 100) {
        last_adc_time = current_time;
        
        // �������ADC��ȡ����
        // ʾ������ȡPA2��PA3��ADCֵ
        float forward_voltage = 0.0f;   // ��ADC��ȡ�����ʵ�ѹ
        float reflected_voltage = 0.0f; // ��ADC��ȡ���书�ʵ�ѹ
        
        // ����ѹת��Ϊ����ֵ (�������Ӳ���궨)
        float forward_power = forward_voltage * 10.0f;    // ʾ��ת����ʽ
        float reflected_power = reflected_voltage * 10.0f; // ʾ��ת����ʽ
        
        // ���¹������ݵ����������
        InterfaceManager_UpdatePower(forward_power, reflected_power);
    }
    
    // ÿ200ms����һ����Ƶ����
    if (current_time - last_calc_time >= 200) {
        last_calc_time = current_time;
        
        // ������Ƶ����
        if (g_power_result.is_valid && g_power_result.forward_power > 0.01f) {
            // ���㷴��ϵ�� �� = ��(Pr/Pi)
            float reflection_coeff = sqrtf(g_power_result.reflected_power / g_power_result.forward_power);
            
            // ����פ���� VSWR = (1 + |��|) / (1 - |��|)
            float vswr = (1.0f + reflection_coeff) / (1.0f - reflection_coeff);
            if (vswr < 1.0f) vswr = 1.0f;  // VSWR��СֵΪ1
            if (vswr > 10.0f) vswr = 10.0f; // �������ֵ
            
            // ���㴫��Ч�� �� = 1 - Pr/Pi
            float transmission_eff = (1.0f - g_power_result.reflected_power / g_power_result.forward_power) * 100.0f;
            if (transmission_eff < 0.0f) transmission_eff = 0.0f;
            if (transmission_eff > 100.0f) transmission_eff = 100.0f;
            
            // ������Ƶ���������������
            InterfaceManager_UpdateRFParams(vswr, reflection_coeff, transmission_eff);
        }
    }
    
    // ������������ (������ʾ���ºͰ�������)
    InterfaceManager_Process();
    
    // ���Ƶ�ʼ��½��
    if (FreqCounter_IsNewResult()) {
        // Ƶ�ʽ�����ڽ�����ʾ���Զ���ȡ
        FreqCounter_ClearNewResultFlag();
    }
    
    // ������ʱ
    HAL_Delay(10);
}

/*
// ��stm32f1xx_it.c������жϻص�������
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

/**
 * @brief ������main.c����ʾ��
 */
/*
#include "main.h"
#include "frequency_counter.h"
#include "interface_manager.h"
#include <math.h>

int main(void)
{
    // ϵͳ��ʼ��
    HAL_Init();
    SystemClock_Config();
    
    // �����ʼ��
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
    
    // ��ʼ��LCD
    LCD_Init();
    LCD_SetBacklight(8000);
    LCD_Clear(BLACK);
    
    // ��ʼ��Ƶ�ʼ�
    FreqCounter_Init();
    FreqCounter_Start();
    
    // ��ʼ�����������
    InterfaceManager_Init();
    
    // ����PWM (�������)
    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
    
    // ��ѭ��
    while (1)
    {
        static uint32_t last_adc_time = 0;
        static uint32_t last_calc_time = 0;
        uint32_t current_time = HAL_GetTick();
        
        // ADC�ɼ��͹��ʼ���
        if (current_time - last_adc_time >= 100) {
            last_adc_time = current_time;
            
            // ����ADCת��
            HAL_ADC_Start(&hadc1);
            
            // ��ȡPA2 (������)
            HAL_ADC_PollForConversion(&hadc1, 10);
            uint32_t adc_forward = HAL_ADC_GetValue(&hadc1);
            
            // ��ȡPA3 (���书��) 
            HAL_ADC_PollForConversion(&hadc1, 10);
            uint32_t adc_reflected = HAL_ADC_GetValue(&hadc1);
            
            HAL_ADC_Stop(&hadc1);
            
            // ת��Ϊ��ѹֵ (3.3V�ο���ѹ��12λADC)
            float forward_voltage = (float)adc_forward * 3.3f / 4095.0f;
            float reflected_voltage = (float)adc_reflected * 3.3f / 4095.0f;
            
            // ת��Ϊ����ֵ (�������Ӳ���궨)
            float forward_power = forward_voltage * 10.0f;    // ʾ����10W/V
            float reflected_power = reflected_voltage * 10.0f;
            
            // ���¹�������
            InterfaceManager_UpdatePower(forward_power, reflected_power);
        }
        
        // ��Ƶ��������
        if (current_time - last_calc_time >= 200) {
            last_calc_time = current_time;
            
            if (g_power_result.is_valid && g_power_result.forward_power > 0.01f) {
                float reflection_coeff = sqrtf(g_power_result.reflected_power / g_power_result.forward_power);
                float vswr = (1.0f + reflection_coeff) / (1.0f - reflection_coeff);
                float transmission_eff = (1.0f - g_power_result.reflected_power / g_power_result.forward_power) * 100.0f;
                
                InterfaceManager_UpdateRFParams(vswr, reflection_coeff, transmission_eff);
            }
        }
        
        // ���洦��
        InterfaceManager_Process();
        
        // Ƶ�ʼƴ���
        if (FreqCounter_IsNewResult()) {
            FreqCounter_ClearNewResultFlag();
        }
        
        // ι��
        HAL_IWDG_Refresh(&hiwdg);
        
        HAL_Delay(10);
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
