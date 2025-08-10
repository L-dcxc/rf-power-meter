/**
 * @file frequency_counter.c
 * @brief Ƶ�ʼƹ���ģ��ʵ���ļ�
 * @author ����Ѽ
 * @date 2025-08-10
 * 
 * ʵ��ԭ��
 * 1. TIM2����Ϊ�ⲿʱ��ģʽ��PA0���������ֱ����Ϊ����ʱ��
 * 2. TIM4����Ϊ1�붨ʱ����ÿ�봥��һ���ж�
 * 3. ��TIM4�ж��ж�ȡTIM2�ļ���ֵ������1���ڵ���������ΪƵ��
 * 4. ����TIM2���ܵ���������ͨ������������в���
 */

#include "frequency_counter.h"

/* ȫ�ֱ������� */
FreqCounter_t g_freq_counter = {0};     // Ƶ�ʼ�״̬�ṹ��
FreqResult_t g_freq_result = {0};       // Ƶ�ʲ�������ṹ��

/**
 * @brief Ƶ�ʼƳ�ʼ��
 * @param ��
 * @retval 0: �ɹ�, -1: ʧ��
 */
int8_t FreqCounter_Init(void)
{
    // ��������״̬����
    g_freq_counter.tim2_count = 0;
    g_freq_counter.tim2_overflow_count = 0;
    g_freq_counter.last_frequency = 0;
    g_freq_counter.measurement_ready = 0;
    g_freq_counter.is_running = 0;
    
    // ����������
    g_freq_result.frequency_hz = 0;
    g_freq_result.frequency_display = 0.0f;
    g_freq_result.unit = FREQ_UNIT_HZ;
    g_freq_result.is_valid = 0;
    
    // ����TIM2������
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    
    // ����TIM4������  
    __HAL_TIM_SET_COUNTER(&htim4, 0);
    
    return 0;
}

/**
 * @brief ����Ƶ�ʼƲ���
 * @param ��
 * @retval 0: �ɹ�, -1: ʧ��
 */
int8_t FreqCounter_Start(void)
{
    // ����Ѿ������У���ֹͣ
    if (g_freq_counter.is_running) {
        FreqCounter_Stop();
    }
    
    // ���³�ʼ��״̬
    FreqCounter_Init();
    
    // ����TIM2 (�ⲿʱ�Ӽ���)
    if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK) {
        return -1;
    }
    
    // ����TIM4 (1�붨ʱ��)
    if (HAL_TIM_Base_Start_IT(&htim4) != HAL_OK) {
        HAL_TIM_Base_Stop_IT(&htim2);  // ���TIM4����ʧ�ܣ�ֹͣTIM2
        return -1;
    }
    
    // ��������״̬
    g_freq_counter.is_running = 1;
    
    return 0;
}

/**
 * @brief ֹͣƵ�ʼƲ���
 * @param ��
 * @retval 0: �ɹ�, -1: ʧ��
 */
int8_t FreqCounter_Stop(void)
{
    // ֹͣTIM2
    HAL_TIM_Base_Stop_IT(&htim2);
    
    // ֹͣTIM4
    HAL_TIM_Base_Stop_IT(&htim4);
    
    // �������״̬
    g_freq_counter.is_running = 0;
    
    return 0;
}

/**
 * @brief ��ȡ���µ�Ƶ�ʲ������
 * @param result: ָ�����ṹ���ָ��
 * @retval 0: �ɹ���ȡ, -1: ����Ч����
 */
int8_t FreqCounter_GetResult(FreqResult_t *result)
{
    // ����������
    if (result == NULL) {
        return -1;
    }
    
    // ����Ƿ�����Ч����
    if (!g_freq_result.is_valid) {
        return -1;
    }
    
    // ���ƽ��
    *result = g_freq_result;
    
    return 0;
}

/**
 * @brief ��ȡԭʼƵ��ֵ(Hz)
 * @param ��
 * @retval Ƶ��ֵ(Hz), 0��ʾ��Ч
 */
uint32_t FreqCounter_GetFrequencyHz(void)
{
    if (g_freq_result.is_valid) {
        return g_freq_result.frequency_hz;
    }
    return 0;
}

/**
 * @brief ����Ƿ����µĲ������
 * @param ��
 * @retval 1: ���½��, 0: ���½��
 */
uint8_t FreqCounter_IsNewResult(void)
{
    return g_freq_counter.measurement_ready;
}

/**
 * @brief ����½����־
 * @param ��
 * @retval ��
 */
void FreqCounter_ClearNewResultFlag(void)
{
    g_freq_counter.measurement_ready = 0;
}

/**
 * @brief Ƶ��ֵת��Ϊ���ʵ���ʾ��λ
 * @param frequency_hz: ����Ƶ��(Hz)
 * @param result: �������ṹ��ָ��
 * @retval ��
 */
void FreqCounter_ConvertUnit(uint32_t frequency_hz, FreqResult_t *result)
{
    result->frequency_hz = frequency_hz;
    
    if (frequency_hz >= 1000000) {
        // >= 1MHz��ʹ��MHz��λ
        result->frequency_display = (float)frequency_hz / 1000000.0f;
        result->unit = FREQ_UNIT_MHZ;
    }
    else if (frequency_hz >= 1000) {
        // >= 1KHz��ʹ��KHz��λ
        result->frequency_display = (float)frequency_hz / 1000.0f;
        result->unit = FREQ_UNIT_KHZ;
    }
    else {
        // < 1KHz��ʹ��Hz��λ
        result->frequency_display = (float)frequency_hz;
        result->unit = FREQ_UNIT_HZ;
    }
    
    result->is_valid = 1;
}

/**
 * @brief TIM4�жϻص�����(1�붨ʱ)
 * @param htim: ��ʱ�����ָ��
 * @retval ��
 * @note ÿ�����һ�Σ���ȡTIM2����ֵ������Ƶ��
 */
void FreqCounter_TIM4_Callback(TIM_HandleTypeDef *htim)
{
    // ȷ����TIM4�ж�
    if (htim->Instance != TIM4) {
        return;
    }

    // ��ȡTIM2��ǰ����ֵ
    uint32_t current_count = __HAL_TIM_GET_COUNTER(&htim2);

    // �����ܵ������� = ������� * ������ֵ + ��ǰ����ֵ
    uint32_t total_pulses = g_freq_counter.tim2_overflow_count * FREQ_COUNTER_TIM2_MAX + current_count;

    // ����Ԥ��Ƶϵ��(��ǰΪ1������Ƶ)
    uint32_t frequency = total_pulses * FREQ_COUNTER_PRESCALER;

    // ����Ƶ�ʽ��
    FreqCounter_ConvertUnit(frequency, &g_freq_result);

    // ���浽״̬�ṹ��
    g_freq_counter.last_frequency = frequency;
    g_freq_counter.measurement_ready = 1;  // �����½����־

    // ���ü����������������׼����һ�β���
    __HAL_TIM_SET_COUNTER(&htim2, 0);
    g_freq_counter.tim2_overflow_count = 0;
    g_freq_counter.tim2_count = 0;
}

/**
 * @brief TIM2����жϻص�����
 * @param htim: ��ʱ�����ָ��
 * @retval ��
 * @note ��TIM2�������ʱ���ã���¼�������
 */
void FreqCounter_TIM2_Callback(TIM_HandleTypeDef *htim)
{
    // ȷ����TIM2�ж�
    if (htim->Instance != TIM2) {
        return;
    }

    // �����������
    g_freq_counter.tim2_overflow_count++;

    // ��ֹ�����������(��ȫ����)
    if (g_freq_counter.tim2_overflow_count > FREQ_COUNTER_MAX_OVERFLOW) {
        // �������������࣬�������쳣��������ü���
        g_freq_counter.tim2_overflow_count = 0;
        __HAL_TIM_SET_COUNTER(&htim2, 0);
    }
}

/**
 * @brief ��ȡƵ�ʼ�����״̬
 * @param ��
 * @retval 1: ��������, 0: ��ֹͣ
 */
uint8_t FreqCounter_IsRunning(void)
{
    return g_freq_counter.is_running;
}

/**
 * @brief ��ȡƵ�ʵ�λ�ַ���
 * @param unit: Ƶ�ʵ�λö��
 * @retval ��λ�ַ���ָ��
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
