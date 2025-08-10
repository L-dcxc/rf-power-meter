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

/* ���������״̬�ṹ�� */
typedef struct {
    InterfaceIndex_t current_interface;     // ��ǰ����
    uint8_t menu_cursor;                    // �˵����λ��
    uint8_t brightness_level;               // ���ȵȼ�(0-10)
    uint8_t alarm_enabled;                  // ����ʹ��
    float vswr_alarm_threshold;             // VSWR������ֵ
    uint32_t last_update_time;              // �ϴθ���ʱ��
    uint8_t need_refresh;                   // ��Ҫˢ�±�־
} InterfaceManager_t;

/* ȫ�ֱ������� */
extern InterfaceManager_t g_interface_manager;
extern PowerResult_t g_power_result;
extern RFParams_t g_rf_params;

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

/* ������ʾ�������� */
void Interface_DisplayMain(void);           // ��������ʾ
void Interface_DisplayMenu(void);           // �˵�������ʾ
void Interface_DisplayCalibration(void);    // У׼������ʾ
void Interface_DisplayStandard(void);       // �궨������ʾ
void Interface_DisplayAlarm(void);          // �������ý�����ʾ
void Interface_DisplayBrightness(void);     // �������ý�����ʾ
void Interface_DisplayAbout(void);          // ���ڽ�����ʾ

// ϵͳ��������
void System_BootSequence(void);             // ϵͳ�������н���

#ifdef __cplusplus
}
#endif

#endif /* __INTERFACE_MANAGER_H */
