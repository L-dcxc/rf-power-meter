/**
 * @file interface_manager.h
 * @brief ��Ƶ���ʼƽ������ϵͳͷ�ļ�
 * @author ����Ѽ
 * @date 2025-08-10
 * 
 * ����˵����
 * 1. ������������ʾ�����ʡ�פ���ȡ�Ƶ�ʵȲ�����
 * 2. �������ò˵�ϵͳ
 * 3. ��������Ӧ�ͽ����л�
 * 4. �ṩ��ɫ��ʾ����
 */

#ifndef __INTERFACE_MANAGER_H
#define __INTERFACE_MANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

/* ������Ҫ��ͷ�ļ� */
#include "main.h"
#include "ST7567A.h"
#include "frequency_counter.h"

/* �궨�� */
#define MAX_MENU_ITEMS      6       // ���˵�������
#define DISPLAY_UPDATE_MS   100     // ��ʾ���¼��(ms)

/* ��������ö�� */
typedef enum {
    INTERFACE_MAIN = 0,         // ������
    INTERFACE_MENU,             // ���ò˵�
    INTERFACE_CALIBRATION,      // У׼����
    INTERFACE_CAL_CONFIRM,      // У׼ȷ��ҳ
    INTERFACE_CAL_ZERO,         // ���У׼
    INTERFACE_CAL_POWER,        // ����б��У׼
    INTERFACE_CAL_BAND,         // Ƶ����������
    INTERFACE_CAL_REFLECT,      // ����ͨ������
    INTERFACE_CAL_FREQ,         // Ƶ�ʼ�΢��
    INTERFACE_CAL_COMPLETE,     // У׼���
    INTERFACE_STANDARD,         // �궨����
    INTERFACE_ALARM,            // ���ޱ���
    INTERFACE_BRIGHTNESS,       // ��ʾ����
    INTERFACE_ABOUT             // �����豸
} InterfaceIndex_t;

/* ����ö�� */
typedef enum {
    KEY_NONE = 0,               // �ް���
    KEY_UP,                     // ����/���ؼ� (PB12)
    KEY_OK,                     // ȷ�ϼ� (PB13)
    KEY_DOWN                    // ����/�л��� (PB14)
} KeyValue_t;

/* ����״̬ö�� */
typedef enum {
    KEY_STATE_IDLE = 0,         // ����״̬
    KEY_STATE_PRESSED,          // ����״̬
    KEY_STATE_LONG_PRESSED      // ����״̬
} KeyState_t;

/* VSWR��ʾ��ɫö�� */
typedef enum {
    VSWR_COLOR_GREEN = 0,       // ��ɫ (��1.5)
    VSWR_COLOR_YELLOW,          // ��ɫ (1.5-2.0)
    VSWR_COLOR_RED              // ��ɫ (>2.0)
} VSWRColor_t;

/* ���ʵ�λö�� */
typedef enum {
    POWER_UNIT_W = 0,           // ����
    POWER_UNIT_KW               // ǧ��
} PowerUnit_t;

/* ���ʲ�������ṹ�� */
typedef struct {
    float forward_power;        // ������
    float reflected_power;      // ���书��
    PowerUnit_t forward_unit;   // �����ʵ�λ
    PowerUnit_t reflected_unit; // ���书�ʵ�λ
    uint8_t is_valid;          // �����Ƿ���Ч
} PowerResult_t;

/* ��Ƶ�����ṹ�� */
typedef struct {
    float vswr;                 // פ����
    float reflection_coeff;     // ����ϵ��
    float transmission_eff;     // ����Ч��(%)
    VSWRColor_t vswr_color;     // VSWR��ɫ��ʾ
    uint8_t is_valid;          // �����Ƿ���Ч
} RFParams_t;

/* �˵���ṹ�� */
typedef struct {
    uint8_t index;              // �˵�����
    uint8_t parent;             // ���˵�����
    uint8_t next;               // ��һ���˵�����
    void (*display_func)(void); // ��ʾ����ָ��
    const char* title;          // �˵�����
} MenuItem_t;

/* ������״̬�ṹ�� */
typedef struct {
    uint8_t is_active;          // �������Ƿ񼤻�
    uint16_t duration_count;    // ʣ�����ʱ��(ms)
} BuzzerState_t;

/* У׼����ö�� */
typedef enum {
    CAL_STEP_CONFIRM = 0,       // ȷ�Ͻ���У׼
    CAL_STEP_ZERO,              // ���У׼
    CAL_STEP_POWER,             // ����б��У׼
    CAL_STEP_BAND,              // Ƶ����������
    CAL_STEP_REFLECT,           // ����ͨ������
    CAL_STEP_FREQ,              // Ƶ�ʼ�΢��
    CAL_STEP_COMPLETE           // У׼���
} CalibrationStep_t;

/* У׼���ݽṹ�� */
/* ����У׼��ṹ�� */
typedef struct {
    float power;                // ����ֵ(W)
    float voltage;              // ��Ӧ��ѹ(V)
} PowerCalPoint_t;

typedef struct {
    float forward_offset;       // ���������ƫ��(V)
    float reflected_offset;     // ���书�����ƫ��(V)
    PowerCalPoint_t fwd_table[5];    // ������У׼��(100W-500W)
    PowerCalPoint_t ref_table[5];    // ���书��У׼��
    uint8_t fwd_points;         // ����У׼����(���5��)
    uint8_t ref_points;         // ����У׼����(���5��)
    float band_gain_fwd[11];    // ��Ƶ��������������ϵ��
    float band_gain_ref[11];    // ��Ƶ�η�����������ϵ��
    float freq_trim;            // Ƶ�ʼ�΢��ϵ��
    uint8_t is_calibrated;      // У׼��ɱ�־
} CalibrationData_t;

/* У׼״̬�ṹ�� */
typedef struct {
    CalibrationStep_t current_step;     // ��ǰУ׼����
    uint8_t current_band;               // ��ǰƵ������(0-10)
    uint8_t current_power_point;        // ��ǰ����У׼��(0-4)
    uint8_t current_channel;            // ��ǰУ׼ͨ��(0=����,1=����)
    uint8_t sample_count;               // ��������
    float sample_sum_fwd;               // �����ʲ����ۼ�
    float sample_sum_ref;               // ���书�ʲ����ۼ�
    uint8_t is_stable;                  // �����ȶ���־
    uint16_t stable_count;              // �ȶ�������
    float target_power;                 // Ŀ�깦��ֵ
    uint8_t power_cal_mode;             // ����У׼ģʽ(0=����,1=����)
} CalibrationState_t;

/* ���������״̬�ṹ�� */
typedef struct {
    InterfaceIndex_t current_interface;     // ��ǰ����
    uint8_t menu_cursor;                    // �˵����λ��
    uint8_t brightness_level;               // ���ȵȼ�(0-10)
    uint8_t alarm_enabled;                  // ����ʹ��
    float vswr_alarm_threshold;             // VSWR������ֵ
    uint8_t alarm_selected_item;            // ��������ѡ���� (0=����, 1=��ֵ)
    uint32_t last_update_time;              // �ϴθ���ʱ��
    uint8_t need_refresh;                   // ��Ҫˢ�±�־
} InterfaceManager_t;

/* ȫ�ֱ������� */
extern InterfaceManager_t g_interface_manager;
extern PowerResult_t g_power_result;
extern RFParams_t g_rf_params;
extern BuzzerState_t g_buzzer_state;
extern CalibrationData_t g_calibration_data;
extern CalibrationState_t g_calibration_state;

/* �������� */

/**
 * @brief �����������ʼ��
 * @param ��
 * @retval 0: �ɹ�, -1: ʧ��
 */
int8_t InterfaceManager_Init(void);

/**
 * @brief �����������ѭ������
 * @param ��
 * @retval ��
 * @note ��main������ѭ���е���
 */
void InterfaceManager_Process(void);

/**
 * @brief ����������
 * @param key: ����ֵ
 * @param state: ����״̬
 * @retval ��
 */
void InterfaceManager_HandleKey(KeyValue_t key, KeyState_t state);

/**
 * @brief �л���ָ������
 * @param interface: Ŀ���������
 * @retval ��
 */
void InterfaceManager_SwitchTo(InterfaceIndex_t interface);

/**
 * @brief ǿ��ˢ�µ�ǰ����
 * @param ��
 * @retval ��
 */
void InterfaceManager_ForceRefresh(void);

/**
 * @brief ���¹�������
 * @param forward_power: ������
 * @param reflected_power: ���书��
 * @retval ��
 */
void InterfaceManager_UpdatePower(float forward_power, float reflected_power);

/**
 * @brief ������Ƶ����
 * @param vswr: פ����
 * @param reflection_coeff: ����ϵ��
 * @param transmission_eff: ����Ч��
 * @retval ��
 */
void InterfaceManager_UpdateRFParams(float vswr, float reflection_coeff, float transmission_eff);

/**
 * @brief ��ȡ����ֵ
 * @param ��
 * @retval ����ֵ
 */
KeyValue_t InterfaceManager_GetKey(void);

/**
 * @brief ���ñ�������
 * @param level: ���ȵȼ�(0-10)
 * @retval ��
 */
void InterfaceManager_SetBrightness(uint8_t level);

/**
 * @brief ����������
 * @param duration_ms: ����ʱ��(ms)
 * @retval ��
 */
void InterfaceManager_Beep(uint16_t duration_ms);

/**
 * @brief ��������ʱ����������TIM3�ж��е��ã�
 * @param ��
 * @retval ��
 */
void InterfaceManager_BuzzerProcess(void);

/* ������ʾ�������� */
void Interface_DisplayMain(void);           // ��������ʾ
void Interface_DisplayMenu(void);           // �˵�������ʾ
void Interface_DisplayCalibration(void);    // У׼������ʾ
void Interface_DisplayCalConfirm(void);     // У׼ȷ��ҳ��ʾ
void Interface_DisplayCalZero(void);        // ���У׼��ʾ
void Interface_DisplayCalPower(void);       // ����б��У׼��ʾ
void Interface_DisplayCalBand(void);        // Ƶ������������ʾ
void Interface_DisplayCalReflect(void);     // ����ͨ��������ʾ
void Interface_DisplayCalFreq(void);        // Ƶ�ʼ�΢����ʾ
void Interface_DisplayCalComplete(void);    // У׼�����ʾ
void Interface_DisplayStandard(void);       // �궨������ʾ
void Interface_DisplayAlarm(void);          // �������ý�����ʾ
void Interface_DisplayBrightness(void);     // �������ý�����ʾ
void Interface_DisplayAbout(void);          // ���ڽ�����ʾ

/* У׼���ܺ������� */
void Calibration_Init(void);                // У׼ϵͳ��ʼ��
void Calibration_LoadFromEEPROM(void);      // ��EEPROM����У׼����
void Calibration_SaveToEEPROM(void);        // ����У׼���ݵ�EEPROM
void Calibration_StartStep(CalibrationStep_t step);  // ��ʼУ׼����
void Calibration_ProcessSample(void);       // ����У׼����
uint8_t Calibration_GetBandIndex(float frequency);   // ��ȡƵ������
float Calibration_CalculatePowerFromTable(float voltage, PowerCalPoint_t* table, uint8_t points);  // �����㹦��
float Calibration_ApplyCorrection(float raw_voltage, uint8_t is_forward, float frequency);  // Ӧ��У׼����

// ϵͳ��������
void System_BootSequence(void);             // ϵͳ�������н���

#ifdef __cplusplus
}
#endif

#endif /* __INTERFACE_MANAGER_H */
