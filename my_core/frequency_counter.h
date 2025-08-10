/**
 * @file frequency_counter.h
 * @brief Ƶ�ʼƹ���ģ��ͷ�ļ�
 * @author �������
 * @date 2025-01-10
 * 
 * ����˵����
 * 1. ʹ��TIM2�ⲿʱ��ģʽ����PA0���������
 * 2. ʹ��TIM4ÿ�봥��һ�ζ�ȡ����ֵ
 * 3. ֧��1Hz-100MHzƵ�ʲ���
 * 4. �Զ�����ʱ��������
 */

#ifndef __FREQUENCY_COUNTER_H
#define __FREQUENCY_COUNTER_H

#ifdef __cplusplus
extern "C" {
#endif

/* ������Ҫ��ͷ�ļ� */
#include "main.h"
#include "tim.h"

/* �궨�� */
#define FREQ_COUNTER_MAX_OVERFLOW   1000    // ����������
#define FREQ_COUNTER_TIM2_MAX       65536   // TIM2������ֵ(16λ��ʱ��)
#define FREQ_COUNTER_PRESCALER      1       // Ԥ��Ƶϵ��(��ǰ����Ƶ)

/* Ƶ�ʵ�λö�� */
typedef enum {
    FREQ_UNIT_HZ = 0,       // Hz
    FREQ_UNIT_KHZ,          // KHz  
    FREQ_UNIT_MHZ           // MHz
} FreqUnit_t;

/* Ƶ�ʲ�������ṹ�� */
typedef struct {
    uint32_t frequency_hz;      // Ƶ��ֵ(Hz)
    float frequency_display;    // ��ʾ��Ƶ��ֵ(���ݵ�λ�Զ�ת��)
    FreqUnit_t unit;           // Ƶ�ʵ�λ
    uint8_t is_valid;          // ��������Ƿ���Ч(1=��Ч, 0=��Ч)
} FreqResult_t;

/* Ƶ�ʼ�״̬�ṹ�� */
typedef struct {
    uint32_t tim2_count;           // TIM2��ǰ����ֵ
    uint32_t tim2_overflow_count;  // TIM2�������
    uint32_t last_frequency;       // �ϴβ�����Ƶ��
    uint8_t measurement_ready;     // �������׼����־
    uint8_t is_running;           // Ƶ�ʼ�����״̬
} FreqCounter_t;

/* ȫ�ֱ������� */
extern FreqCounter_t g_freq_counter;
extern FreqResult_t g_freq_result;

/* �������� */

/**
 * @brief Ƶ�ʼƳ�ʼ��
 * @param ��
 * @retval 0: �ɹ�, -1: ʧ��
 * @note ��ʼ����ʱ������ر�����������������
 */
int8_t FreqCounter_Init(void);

/**
 * @brief ����Ƶ�ʼƲ���
 * @param ��
 * @retval 0: �ɹ�, -1: ʧ��
 * @note ����TIM2��TIM4����ʼƵ�ʲ���
 */
int8_t FreqCounter_Start(void);

/**
 * @brief ֹͣƵ�ʼƲ���
 * @param ��
 * @retval 0: �ɹ�, -1: ʧ��
 * @note ֹͣTIM2��TIM4��ֹͣƵ�ʲ���
 */
int8_t FreqCounter_Stop(void);

/**
 * @brief ��ȡ���µ�Ƶ�ʲ������
 * @param result: ָ�����ṹ���ָ��
 * @retval 0: �ɹ���ȡ, -1: ����Ч����
 * @note ��������һ�ε�Ƶ�ʲ������
 */
int8_t FreqCounter_GetResult(FreqResult_t *result);

/**
 * @brief ��ȡԭʼƵ��ֵ(Hz)
 * @param ��
 * @retval Ƶ��ֵ(Hz), 0��ʾ��Ч
 * @note ֱ�ӷ���Hz��λ��Ƶ��ֵ
 */
uint32_t FreqCounter_GetFrequencyHz(void);

/**
 * @brief ����Ƿ����µĲ������
 * @param ��
 * @retval 1: ���½��, 0: ���½��
 * @note �����ж��Ƿ���Ҫ������ʾ
 */
uint8_t FreqCounter_IsNewResult(void);

/**
 * @brief ����½����־
 * @param ��
 * @retval ��
 * @note ��ȡ�������ã�����½����־
 */
void FreqCounter_ClearNewResultFlag(void);

/**
 * @brief TIM4�жϻص�����(1�붨ʱ)
 * @param htim: ��ʱ�����ָ��
 * @retval ��
 * @note ��TIM4�ж��е��ã����ڶ�ȡTIM2����ֵ������Ƶ��
 */
void FreqCounter_TIM4_Callback(TIM_HandleTypeDef *htim);

/**
 * @brief TIM2����жϻص�����
 * @param htim: ��ʱ�����ָ��  
 * @retval ��
 * @note ��TIM2����ж��е��ã����ڼ�¼�������
 */
void FreqCounter_TIM2_Callback(TIM_HandleTypeDef *htim);

/**
 * @brief Ƶ��ֵת��Ϊ���ʵ���ʾ��λ
 * @param frequency_hz: ����Ƶ��(Hz)
 * @param result: �������ṹ��ָ��
 * @retval ��
 * @note �Զ�ѡ����ʵĵ�λ(Hz/KHz/MHz)������ʾ
 */
void FreqCounter_ConvertUnit(uint32_t frequency_hz, FreqResult_t *result);

/**
 * @brief ��ȡƵ�ʼ�����״̬
 * @param ��
 * @retval 1: ��������, 0: ��ֹͣ
 */
uint8_t FreqCounter_IsRunning(void);

/**
 * @brief ��ȡƵ�ʵ�λ�ַ���
 * @param unit: Ƶ�ʵ�λö��
 * @retval ��λ�ַ���ָ��
 * @note ������ʾʱ��ȡ��λ�ַ���
 */
const char* FreqCounter_GetUnitString(FreqUnit_t unit);

#ifdef __cplusplus
}
#endif

#endif /* __FREQUENCY_COUNTER_H */
