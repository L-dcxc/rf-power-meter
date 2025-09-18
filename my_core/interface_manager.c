/**
 * @file interface_manager.c
 * @brief ��Ƶ���ʼƽ������ϵͳʵ���ļ�
 * @author ����Ѽ
 * @date 2025-08-10
 */

#include "interface_manager.h"
#include "gpio.h"
#include "tim.h"
#include "adc.h"
#include "eeprom_24c16.h"
#include "frequency_counter.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "stm32f1xx_it.h"
// �ⲿ��������
extern IWDG_HandleTypeDef hiwdg;
extern ADC_HandleTypeDef hadc1;

/* ȫ�ֱ������� */
InterfaceManager_t g_interface_manager = {
    .current_interface = INTERFACE_MAIN,
    .menu_cursor = 0,
    .brightness_level = 8,
    .alarm_enabled = 1,
    .vswr_alarm_threshold = 3.0f,
    .alarm_selected_item = 0,
    .vswr_digit_position = 0,
    .last_update_time = 0,
    .need_refresh = 1,
    .key_press_start_time = 0,
    .key_long_press_detected = 0,
    .interface_first_enter = 1
};
PowerResult_t g_power_result = {
    .forward_power = 0.0f,
    .reflected_power = 0.0f,
    .forward_unit = POWER_UNIT_W,
    .reflected_unit = POWER_UNIT_W,
    .is_valid = 0
};
RFParams_t g_rf_params = {
    .vswr = 1.0f,
    .reflection_coeff = 0.0f,
    .transmission_eff = 100.0f,
    .vswr_color = VSWR_COLOR_GREEN,
    .is_valid = 0
};

/* ������״̬ȫ�ֱ��� */
BuzzerState_t g_buzzer_state = {
    .is_active = 0,
    .duration_count = 0
};

/* У׼����ȫ�ֱ��� */
CalibrationData_t g_calibration_data = {
    .forward_offset = 0.0f,
    .reflected_offset = 0.0f,
    .fwd_table = {
        // ��ʼ��20��У׼���Ĭ��ֵ
        {100.0f, 1.0f}, {200.0f, 2.0f}, {300.0f, 3.0f}, {400.0f, 4.0f}, {500.0f, 5.0f},
        {600.0f, 6.0f}, {700.0f, 7.0f}, {800.0f, 8.0f}, {900.0f, 9.0f}, {1000.0f, 10.0f},
        {1100.0f, 11.0f}, {1200.0f, 12.0f}, {1300.0f, 13.0f}, {1400.0f, 14.0f}, {1500.0f, 15.0f},
        {1600.0f, 16.0f}, {1700.0f, 17.0f}, {1800.0f, 18.0f}, {1900.0f, 19.0f}, {2000.0f, 20.0f}
    },
    .ref_table = {
        // ���书��У׼���ʼֵ
        {100.0f, 0.1f}, {200.0f, 0.2f}, {300.0f, 0.3f}, {400.0f, 0.4f}, {500.0f, 0.5f},
        {600.0f, 0.6f}, {700.0f, 0.7f}, {800.0f, 0.8f}, {900.0f, 0.9f}, {1000.0f, 1.0f},
        {1100.0f, 1.1f}, {1200.0f, 1.2f}, {1300.0f, 1.3f}, {1400.0f, 1.4f}, {1500.0f, 1.5f},
        {1600.0f, 1.6f}, {1700.0f, 1.7f}, {1800.0f, 1.8f}, {1900.0f, 1.9f}, {2000.0f, 2.0f}
    },
    .fwd_points = 0,            // ��ʼ��У׼��
    .ref_points = 0,            // ��ʼ��У׼��
    .freq_gain_fwd = 1.0f,      // Ƶ����������ϵ��(����)
    .freq_gain_ref = 1.0f,      // Ƶ����������ϵ��(����)
    .cal_frequency = 14.0f,     // �궨Ƶ��(MHz)
    .freq_trim = 1.0f,
    .is_calibrated = 0
};

/* У׼״̬ȫ�ֱ��� */
CalibrationState_t g_calibration_state = {
    .current_step = CAL_STEP_CONFIRM,
    .cal_frequency = 14.0f,     // Ĭ�ϱ궨Ƶ��14MHz
    .current_power_point = 0,
    .current_channel = 0,
    .sample_count = 0,
    .sample_sum_fwd = 0.0f,
    .sample_sum_ref = 0.0f,
    .is_stable = 0,
    .stable_count = 0,
    .target_power = 100.0f,     // Ĭ��Ŀ�깦��100W
    .power_cal_mode = 0,        // Ĭ��������У׼
    .sample_completed = 0       // ����δ���
};

/* Ƶ�ʱ궨��Χ���� */
#define MIN_CAL_FREQ    1.0f    // ��С�궨Ƶ��(MHz)
#define MAX_CAL_FREQ    50.0f   // ���궨Ƶ��(MHz)
#define FREQ_STEP       1.0f    // Ƶ�ʵ�������(MHz)

/* ����У׼�㶨�� (0W-2kW��Χ��20��У׼��) */
static const float power_cal_points[20] = {
    100.0f, 200.0f, 300.0f, 400.0f, 500.0f,      // 100W-500W
    600.0f, 700.0f, 800.0f, 900.0f, 1000.0f,     // 600W-1000W
    1100.0f, 1200.0f, 1300.0f, 1400.0f, 1500.0f, // 1100W-1500W
    1600.0f, 1700.0f, 1800.0f, 1900.0f, 2000.0f  // 1600W-2000W
};



/* �˵����� */
const MenuItem_t menu_table[MAX_MENU_ITEMS] = {
    {INTERFACE_CALIBRATION, INTERFACE_MENU, INTERFACE_STANDARD, Interface_DisplayCalibration, "Calibration"},
    {INTERFACE_STANDARD, INTERFACE_MENU, INTERFACE_ALARM, Interface_DisplayStandard, "Settings"},
    {INTERFACE_ALARM, INTERFACE_MENU, INTERFACE_BRIGHTNESS, Interface_DisplayAlarm, "Alarm Limit"},
    {INTERFACE_BRIGHTNESS, INTERFACE_MENU, INTERFACE_ABOUT, Interface_DisplayBrightness, "Brightness"},
    {INTERFACE_ABOUT, INTERFACE_MENU, INTERFACE_CALIBRATION, Interface_DisplayAbout, "About"},
    {INTERFACE_MENU, INTERFACE_MAIN, INTERFACE_CALIBRATION, Interface_DisplayMenu, "Settings Menu"}
};

/**
 * @brief �����������ʼ��
 */
int8_t InterfaceManager_Init(void)
{
    // ��ʼ��EEPROM
    BL24C16_Init();

    // ��ʼ�����������״̬
    g_interface_manager.current_interface = INTERFACE_MAIN;
    g_interface_manager.menu_cursor = 0;

    // ��EEPROM��ȡ����ֵ��ʧ����ʹ��Ĭ��ֵ
    uint8_t saved_brightness;
    if (BL24C16_Read(0x0000, &saved_brightness, 1) == EEPROM_OK && saved_brightness >= 1 && saved_brightness <= 10) {
        g_interface_manager.brightness_level = saved_brightness;
    } else {
        g_interface_manager.brightness_level = 8;  // Ĭ��80%����
    }

    // ��EEPROM��ȡ�������ã�ʧ����ʹ��Ĭ��ֵ
    uint8_t saved_alarm_enabled;
    float saved_vswr_threshold;
    if (BL24C16_Read(0x0001, &saved_alarm_enabled, 1) == EEPROM_OK && saved_alarm_enabled <= 1) {
        g_interface_manager.alarm_enabled = saved_alarm_enabled;
    } else {
        g_interface_manager.alarm_enabled = 1;     // Ĭ��ʹ�ܱ���
    }
    if (BL24C16_Read(0x0002, (uint8_t*)&saved_vswr_threshold, sizeof(float)) == EEPROM_OK &&
        saved_vswr_threshold >= 1.0f && saved_vswr_threshold <= 999.9f) {
        g_interface_manager.vswr_alarm_threshold = saved_vswr_threshold;
    } else {
        g_interface_manager.vswr_alarm_threshold = 3.0f;  // Ĭ��VSWR������ֵ
    }
    g_interface_manager.alarm_selected_item = 0;  // Ĭ��ѡ�б�������
    g_interface_manager.last_update_time = 0;
    g_interface_manager.need_refresh = 1;
    
    // ��ʼ����������
    g_power_result.forward_power = 0.0f;
    g_power_result.reflected_power = 0.0f;
    g_power_result.forward_unit = POWER_UNIT_W;
    g_power_result.reflected_unit = POWER_UNIT_W;
    g_power_result.is_valid = 0;
    
    // ��ʼ����Ƶ����
    g_rf_params.vswr = 1.0f;
    g_rf_params.reflection_coeff = 0.0f;
    g_rf_params.transmission_eff = 100.0f;
    g_rf_params.vswr_color = VSWR_COLOR_GREEN;
    g_rf_params.is_valid = 0;
    
    // ���ó�ʼ��������
    InterfaceManager_SetBrightness(g_interface_manager.brightness_level);

    // ��ʼ��У׼ϵͳ
    Calibration_Init();

    return 0;
}

/**
 * @brief �����������ѭ������
 */
void InterfaceManager_Process(void)
{
    uint32_t current_time = HAL_GetTick();
    
    // ����Ƿ���Ҫ������ʾ
    if (current_time - g_interface_manager.last_update_time >= DISPLAY_UPDATE_MS || 
        g_interface_manager.need_refresh) {
        
        g_interface_manager.last_update_time = current_time;
        g_interface_manager.need_refresh = 0;
        
        // ���ݵ�ǰ���������Ӧ����ʾ����
        switch (g_interface_manager.current_interface) {
            case INTERFACE_MAIN:
                Interface_DisplayMain();
                break;
            case INTERFACE_MENU:
                Interface_DisplayMenu();
                break;
            case INTERFACE_CALIBRATION:
                Interface_DisplayCalibration();
                break;
            case INTERFACE_CAL_STEP_SELECT:
                Interface_DisplayCalStepSelect();
                break;
            case INTERFACE_CAL_CONFIRM:
                Interface_DisplayCalConfirm();
                break;
            case INTERFACE_CAL_ZERO:
                Interface_DisplayCalZero();
                break;
            case INTERFACE_CAL_POWER:
                Interface_DisplayCalPower();
                break;
            case INTERFACE_CAL_BAND:
                Interface_DisplayCalBand();
                break;

            case INTERFACE_CAL_COMPLETE:
                Interface_DisplayCalComplete();
                break;
            case INTERFACE_STANDARD:
                Interface_DisplayStandard();
                break;
            case INTERFACE_ALARM:
                Interface_DisplayAlarm();
                break;
            case INTERFACE_BRIGHTNESS:
                Interface_DisplayBrightness();
                break;
            case INTERFACE_ABOUT:
                Interface_DisplayAbout();
                break;
            default:
                g_interface_manager.current_interface = INTERFACE_MAIN;
                break;
        }
    }
    
    // ����У׼�����������У׼ģʽ�У�
    if (g_interface_manager.current_interface >= INTERFACE_CAL_ZERO &&
        g_interface_manager.current_interface <= INTERFACE_CAL_BAND) {
        Calibration_ProcessSample();
    }

    // ������
    KeyValue_t key = InterfaceManager_GetKey();
    if (key != KEY_NONE) {
        InterfaceManager_HandleKey(key, KEY_STATE_PRESSED);
        InterfaceManager_Beep(50);  // ����������
    }
}

/**
 * @brief ����������
 */
void InterfaceManager_HandleLongPress(uint8_t key)
{
    if (key == 1) {  // UP������
        switch (g_interface_manager.current_interface) {
            case INTERFACE_CAL_POWER:
                // ���ʱ궨���泤��UP���������궨
                Calibration_StartStep(CAL_STEP_ZERO);
                InterfaceManager_SwitchTo(INTERFACE_CAL_ZERO);
                InterfaceManager_Beep(200);  // ����������
                break;

            case INTERFACE_CAL_BAND://������
                // // Ƶ�ʱ궨���泤��UP���������궨
                // Calibration_StartStep(CAL_STEP_ZERO);
                // InterfaceManager_SwitchTo(INTERFACE_CAL_ZERO);
                // InterfaceManager_Beep(200);  // ����������
                break;

            default:
                // �������治������
                break;
        }
    }
}

/**
 * @brief ����������
 */
void InterfaceManager_HandleKey(KeyValue_t key, KeyState_t state)
{
    switch (g_interface_manager.current_interface) {
        case INTERFACE_MAIN:
            // �����水������
            if (key == KEY_OK) {
                InterfaceManager_SwitchTo(INTERFACE_MENU);
            }
            break;
            
        case INTERFACE_MENU:
            // �˵����水������
            if (key == KEY_UP) {
                // ����������
                InterfaceManager_SwitchTo(INTERFACE_MAIN);
            } else if (key == KEY_DOWN) {
                // �л��˵���
                g_interface_manager.menu_cursor = (g_interface_manager.menu_cursor + 1) % (MAX_MENU_ITEMS - 1);
                g_interface_manager.need_refresh = 1;
            } else if (key == KEY_OK) {
                // ����ѡ�еĲ˵���
                InterfaceIndex_t target;
                switch (g_interface_manager.menu_cursor) {
                    case 0: target = INTERFACE_CALIBRATION; break;
                    case 1: target = INTERFACE_STANDARD; break;
                    case 2: target = INTERFACE_ALARM; break;
                    case 3: target = INTERFACE_BRIGHTNESS; break;
                    case 4: target = INTERFACE_ABOUT; break;
                    default: target = INTERFACE_CALIBRATION; break;
                }
                InterfaceManager_SwitchTo(target);
            }
            break;
            
        case INTERFACE_STANDARD:
            // ���ý��水������
            if (key == KEY_UP) {
                // ���ز˵�
                InterfaceManager_SwitchTo(INTERFACE_MENU);
            }
            break;

        case INTERFACE_BRIGHTNESS:
            // ���Ƚ��水������
            if (key == KEY_UP) {
                // ���ز˵�
                InterfaceManager_SwitchTo(INTERFACE_MENU);
            } else if (key == KEY_DOWN) {
                // �������� (1-10ѭ��)
                uint8_t new_level = g_interface_manager.brightness_level + 1;
                if (new_level > 10) {
                    new_level = 1;  // ѭ�����������
                    LCD_Fill(11, 76, 138, 84, BLACK);
                }
                InterfaceManager_SetBrightness(new_level);
                g_interface_manager.need_refresh = 1;  // ����ˢ����ʾ
            } else if (key == KEY_OK) {
                // ȷ�ϵ�ǰ�������ã����浽EEPROM
                BL24C16_Write(0x0000, &g_interface_manager.brightness_level, 1);
                // ���ز˵�
                InterfaceManager_SwitchTo(INTERFACE_MENU);
            }
            break;

        case INTERFACE_ALARM:
            // ���ޱ������水������
            if (key == KEY_UP) {
                // ���ز˵�
                InterfaceManager_SwitchTo(INTERFACE_MENU);
            } else if (key == KEY_DOWN) {
                // �л�ѡ����������� �� ��λ �� ʮλ �� ��λ �� С��λ �� �������أ�
                g_interface_manager.alarm_selected_item =
                    (g_interface_manager.alarm_selected_item + 1) % 5;
                g_interface_manager.need_refresh = 1;
            } else if (key == KEY_OK) {
                // ���ڵ�ǰѡ�е���Ŀ
                if (g_interface_manager.alarm_selected_item == 0) {
                    // �л���������
                    g_interface_manager.alarm_enabled = !g_interface_manager.alarm_enabled;
                } else {
                    // ����VSWR��ֵ�ĸ���λ�����޸����㾫�����⣩
                    float current_value = g_interface_manager.vswr_alarm_threshold;
                    // ת��Ϊ�������⸡�㾫������
                    int total_tenths = (int)(current_value * 10.0f + 0.5f);  // �������뵽0.1

                    int hundreds = (total_tenths / 1000) % 10;
                    int tens = (total_tenths / 100) % 10;
                    int ones = (total_tenths / 10) % 10;
                    int decimal = total_tenths % 10;

                    switch (g_interface_manager.alarm_selected_item) {
                        case 1: // ��λ
                            hundreds = (hundreds + 1) % 10;
                            break;
                        case 2: // ʮλ
                            tens = (tens + 1) % 10;
                            break;
                        case 3: // ��λ
                            ones = (ones + 1) % 10;
                            break;
                        case 4: // С��λ
                            decimal = (decimal + 1) % 10;
                            break;
                    }

                    // ���¼���VSWRֵ��ʹ������������⾫�����⣩
                    int new_total_tenths = hundreds * 1000 + tens * 100 + ones * 10 + decimal;
                    float new_value = (float)new_total_tenths / 10.0f;

                    // ���Ʒ�Χ 1.0-999.9
                    if (new_value < 1.0f) new_value = 1.0f;
                    if (new_value > 999.9f) new_value = 999.9f;

                    g_interface_manager.vswr_alarm_threshold = new_value;
                }
                // ���汨�����õ�EEPROM
                BL24C16_Write(0x0001, &g_interface_manager.alarm_enabled, 1);
                BL24C16_Write(0x0002, (uint8_t*)&g_interface_manager.vswr_alarm_threshold, sizeof(float));
                g_interface_manager.need_refresh = 1;
            }
            break;

        case INTERFACE_CALIBRATION:
            // У׼��ڽ��水������
            if (key == KEY_UP) {
                InterfaceManager_SwitchTo(INTERFACE_MENU);
            } else if (key == KEY_OK) {
                // ���벽��ѡ�����
                g_calibration_state.selected_step = 0;  // Ĭ��ѡ�������궨
                InterfaceManager_SwitchTo(INTERFACE_CAL_STEP_SELECT);
            }
            break;

        case INTERFACE_CAL_STEP_SELECT:
            // ����ѡ����水������
            if (key == KEY_UP) {
                InterfaceManager_SwitchTo(INTERFACE_CALIBRATION);
            } else if (key == KEY_DOWN) {
                // �л�ѡ���� (0��1��2��3��0ѭ��)
                g_calibration_state.selected_step = (g_calibration_state.selected_step + 1) % 4;
                g_interface_manager.need_refresh = 1;
            } else if (key == KEY_OK) {
                // ��ʼѡ�еı궨����
                switch (g_calibration_state.selected_step) {
                    case 0:  // �����궨
                        g_calibration_state.is_single_step = 0;
                        Calibration_StartStep(CAL_STEP_CONFIRM);
                        InterfaceManager_SwitchTo(INTERFACE_CAL_CONFIRM);
                        break;
                    case 1:  // ���궨
                        g_calibration_state.is_single_step = 1;
                        Calibration_StartStep(CAL_STEP_ZERO);
                        InterfaceManager_SwitchTo(INTERFACE_CAL_ZERO);
                        break;
                    case 2:  // ���ʱ궨
                        g_calibration_state.is_single_step = 1;
                        Calibration_InitPowerStep();  // ��ʼ�����ʱ궨״̬
                        Calibration_StartStep(CAL_STEP_POWER);
                        InterfaceManager_SwitchTo(INTERFACE_CAL_POWER);
                        break;
                    case 3:  // Ƶ�ʱ궨
                        g_calibration_state.is_single_step = 1;
                        Calibration_InitBandStep();  // ��ʼ��Ƶ�ʱ궨״̬
                        Calibration_StartStep(CAL_STEP_BAND);
                        InterfaceManager_SwitchTo(INTERFACE_CAL_BAND);
                        break;
                }
            }
            break;

        case INTERFACE_CAL_CONFIRM:
            // У׼ȷ��ҳ��������
            if (key == KEY_UP) {
                InterfaceManager_SwitchTo(INTERFACE_CAL_STEP_SELECT);
            } else if (key == KEY_OK) {
                // ȷ�Ͽ�ʼУ׼
                Calibration_StartStep(CAL_STEP_ZERO);
                InterfaceManager_SwitchTo(INTERFACE_CAL_ZERO);
            }
            break;

        case INTERFACE_CAL_ZERO:
            // ���У׼��������
            if (key == KEY_UP) {
                InterfaceManager_SwitchTo(INTERFACE_CAL_CONFIRM);
            } else if (key == KEY_OK) {
                // ��ʼ������
                if (g_calibration_state.sample_count == 0) {
                    g_calibration_state.sample_count = 1;  // ��ʼ����
                    g_calibration_state.sample_sum_fwd = 0.0f;
                    g_calibration_state.sample_sum_ref = 0.0f;
                    g_interface_manager.need_refresh = 1;
                }
            }
            break;

        case INTERFACE_CAL_POWER:
            // ����У׼��������
            if (key == KEY_UP) {
                // UP������һ��У׼��
                if (g_calibration_state.current_channel == 0) {  // ������У׼
                    if (g_calibration_state.current_power_point > 0) {
                        g_calibration_state.current_power_point--;
                        g_calibration_state.target_power = power_cal_points[g_calibration_state.current_power_point];
                    }
                } else {  // ���书��У׼
                    if (g_calibration_state.current_power_point > 0) {
                        g_calibration_state.current_power_point--;
                        g_calibration_state.target_power = power_cal_points[g_calibration_state.current_power_point];
                    } else {
                        // �ӷ����0�㷵�ص��������һ��
                        g_calibration_state.current_channel = 0;
                        g_calibration_state.current_power_point = 19;
                        g_calibration_state.target_power = power_cal_points[19];
                    }
                }
                g_interface_manager.need_refresh = 1;
            } else if (key == KEY_DOWN) {
                // �л�����һ��У׼���ͨ��
                if (g_calibration_state.current_channel == 0) {  // ������У׼
                    if (g_calibration_state.current_power_point < 19) {  // 0-19��20����
                        g_calibration_state.current_power_point++;
                        g_calibration_state.target_power = power_cal_points[g_calibration_state.current_power_point];
                    } else {
                        // �л������书��У׼
                        g_calibration_state.current_channel = 1;
                        g_calibration_state.current_power_point = 0;
                        g_calibration_state.target_power = power_cal_points[0];
                    }
                } else {  // ���书��У׼
                    if (g_calibration_state.current_power_point < 19) {  // 0-19��20����
                        g_calibration_state.current_power_point++;
                        g_calibration_state.target_power = power_cal_points[g_calibration_state.current_power_point];
                    } else {
                        // ��ɹ���У׼
                        if (g_calibration_state.is_single_step) {
                            // �����궨��ɣ����ز���ѡ�����
                            InterfaceManager_SwitchTo(INTERFACE_CAL_STEP_SELECT);
                        } else {
                            // �����궨��������һ��
                            Calibration_InitBandStep();  // ��ʼ��Ƶ�ʱ궨״̬
                            Calibration_StartStep(CAL_STEP_BAND);
                            InterfaceManager_SwitchTo(INTERFACE_CAL_BAND);
                        }
                        return;
                    }
                }
                g_interface_manager.need_refresh = 1;
            } else if (key == KEY_OK) {
                // ��ʼ��ǰ��Ĳ���
                if (g_calibration_state.sample_count == 0 && g_calibration_state.is_stable) {
                    g_calibration_state.sample_count = 1;  // ��ʼ����
                    g_calibration_state.sample_sum_fwd = 0.0f;
                    g_calibration_state.sample_sum_ref = 0.0f;
                    g_interface_manager.need_refresh = 1;
                }
            }
            // ����UP���������궨������ʵ��
            break;

        case INTERFACE_CAL_BAND:
            // Ƶ�ʱ궨��������
            if (key == KEY_UP) {
                // ���ع���У׼ʱ����״̬
                Calibration_StartStep(CAL_STEP_POWER);
                InterfaceManager_SwitchTo(INTERFACE_CAL_POWER);
            } else if (key == KEY_DOWN) {
                // �����궨Ƶ�� (+1MHz��ѭ��)
                g_calibration_state.cal_frequency += FREQ_STEP;
                if (g_calibration_state.cal_frequency > MAX_CAL_FREQ) {
                    g_calibration_state.cal_frequency = MIN_CAL_FREQ;  // ѭ������Сֵ
                }
                g_interface_manager.need_refresh = 1;
            } else if (key == KEY_OK) {
                // ��ʼƵ�ʱ궨����
                if (g_calibration_state.sample_count == 0 && g_calibration_state.is_stable) {
                    g_calibration_state.sample_count = 1;  // ��ʼ����
                    g_calibration_state.sample_sum_fwd = 0.0f;
                    g_calibration_state.sample_sum_ref = 0.0f;
                    g_interface_manager.need_refresh = 1;
                }
            }
            break;





        case INTERFACE_CAL_COMPLETE:
            // У׼��ɰ�������
            if (key == KEY_UP) {
                InterfaceManager_SwitchTo(INTERFACE_MENU);
            } else if (key == KEY_OK) {
                InterfaceManager_SwitchTo(INTERFACE_MAIN);
            }
            break;

        default:
            // �������水������
            if (key == KEY_UP) {
                // ���ز˵�
                InterfaceManager_SwitchTo(INTERFACE_MENU);
            }
            break;
    }
}

/**
 * @brief �л���ָ������
 */
void InterfaceManager_SwitchTo(InterfaceIndex_t interface)
{
    if (interface != g_interface_manager.current_interface) {
        g_interface_manager.current_interface = interface;
        g_interface_manager.need_refresh = 1;
        g_interface_manager.interface_first_enter = 1;  // �����״ν����־
        LCD_Clear(BLACK);  // ����

        // �����л�ʱ�����⴦��
        if (interface == INTERFACE_ALARM) {
            g_interface_manager.alarm_selected_item = 0;  // ����Ϊѡ�б�������
        }
    }
}

/**
 * @brief ǿ��ˢ�µ�ǰ����
 */
void InterfaceManager_ForceRefresh(void)
{
    g_interface_manager.need_refresh = 1;
}

/**
 * @brief ���¹�������
 */
void InterfaceManager_UpdatePower(float forward_power, float reflected_power)
{
    // ������ʾΪ����W (0-2000W��Χ)
    g_power_result.forward_power = forward_power;
    g_power_result.forward_unit = POWER_UNIT_W;

    g_power_result.reflected_power = reflected_power;
    g_power_result.reflected_unit = POWER_UNIT_W;
    
    g_power_result.is_valid = 1;
    
    // ����������棬�����Ҫˢ��
    if (g_interface_manager.current_interface == INTERFACE_MAIN) {
        g_interface_manager.need_refresh = 1;
    }
}

/**
 * @brief ������Ƶ����
 */
void InterfaceManager_UpdateRFParams(float vswr, float reflection_coeff, float transmission_eff)
{
    g_rf_params.vswr = vswr;
    g_rf_params.reflection_coeff = reflection_coeff;
    g_rf_params.transmission_eff = transmission_eff;
    
    // ����VSWRֵ������ɫ��ʾ
    if (vswr >= 999.0f) {
        g_rf_params.vswr_color = VSWR_COLOR_RED;  // �������ʾ��ɫ
    } else if (vswr <= 1.5f) {
        g_rf_params.vswr_color = VSWR_COLOR_GREEN;
    } else if (vswr <= 2.0f) {
        g_rf_params.vswr_color = VSWR_COLOR_YELLOW;
    } else {
        g_rf_params.vswr_color = VSWR_COLOR_RED;
    }
    
    g_rf_params.is_valid = 1;
    
    // ����Ƿ���Ҫ�������궨���治������
    if (g_interface_manager.alarm_enabled &&
        vswr > g_interface_manager.vswr_alarm_threshold &&
        g_interface_manager.current_interface != INTERFACE_CAL_ZERO &&
        g_interface_manager.current_interface != INTERFACE_CAL_POWER &&
        g_interface_manager.current_interface != INTERFACE_CAL_BAND) {
        InterfaceManager_Beep(500);  // ��������
    }
    
    // ����������棬�����Ҫˢ��
    if (g_interface_manager.current_interface == INTERFACE_MAIN) {
        g_interface_manager.need_refresh = 1;
    }
}

/**
 * @brief ��ȡ����ֵ
 */
KeyValue_t InterfaceManager_GetKey(void)
{
    // �Ӷ�ʱ���жϻ�ȡ����ֵ
    uint8_t key = GetKeyValue();

    // ת������ֵ��1=UP, 2=DOWN, 3=OK
    switch(key) {
        case 1: return KEY_UP;
        case 2: return KEY_DOWN;
        case 3: return KEY_OK;
        default: return KEY_NONE;
    }
}

/**
 * @brief ���ñ�������
 */
void InterfaceManager_SetBrightness(uint8_t level)
{
    if (level > 10) level = 10;
    if (level < 1) level = 1;
    g_interface_manager.brightness_level = level;

    // ����PWMռ�ձ� (10%-100%)��Period=499
    // level=1ʱԼ10%ռ�ձ�(50), level=10ʱ100%ռ�ձ�(499)
    uint16_t duty = (level * 449) / 10 + 50;  // 50-499��Χ
    if (duty > 499) duty = 499;  // �������ֵ

    // ����TIM3 CH3��PWMռ�ձ� (PB0�������)
    LCD_SetBacklight(duty-1);
}

/**
 * @brief ��������������������ʽ��
 */
void InterfaceManager_Beep(uint16_t duration_ms)
{
    // ���÷�����״̬
    g_buzzer_state.is_active = 1;
    g_buzzer_state.duration_count = duration_ms;

    // �������������� (PB4)
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
}

/**
 * @brief ��������ʱ����������TIM3�ж��е��ã�
 * @note TIM3������200Hz��ÿ5ms�ж�һ�Σ�������ÿ�μ�5ms
 */
void InterfaceManager_BuzzerProcess(void)
{
    if (g_buzzer_state.is_active) {
        if (g_buzzer_state.duration_count > 0) {
            // ÿ�μ�5����Ϊ������5ms�ж�һ�Σ�200Hz��
            if (g_buzzer_state.duration_count >= 5) {
                g_buzzer_state.duration_count -= 5;
            } else {
                g_buzzer_state.duration_count = 0;
            }
        } else {
            // ʱ�䵽���رշ�����
            g_buzzer_state.is_active = 0;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);
        }
    }
}

/* ========== У׼����ʵ�� ========== */

/**
 * @brief У׼ϵͳ��ʼ��
 */
void Calibration_Init(void)
{
    // ��EEPROM����У׼����
    Calibration_LoadFromEEPROM();

    // ����У׼״̬
    g_calibration_state.current_step = CAL_STEP_CONFIRM;
    g_calibration_state.cal_frequency = 14.0f;
    g_calibration_state.current_power_point = 0;
    g_calibration_state.current_channel = 0;
    g_calibration_state.sample_count = 0;
    g_calibration_state.sample_sum_fwd = 0.0f;
    g_calibration_state.sample_sum_ref = 0.0f;
    g_calibration_state.is_stable = 0;
    g_calibration_state.stable_count = 0;
    g_calibration_state.target_power = 100.0f;
    g_calibration_state.power_cal_mode = 0;
    g_calibration_state.sample_completed = 0;
}

/**
 * @brief ��EEPROM����У׼����
 *
 * EEPROM��ַ���� (֧��0W-2kW���ʷ�Χ):
 * 0x0010-0x0017: ���ƫ������ (8�ֽ�)
 * 0x0020-0x011F: ������У׼�� (20���8�ֽ�=160�ֽ�)
 * 0x0120-0x021F: ���书��У׼�� (20���8�ֽ�=160�ֽ�)
 * 0x0220-0x0221: У׼���� (2�ֽ�)
 * 0x0300-0x030B: Ƶ���������� (3��4�ֽ�=12�ֽ�)
 * 0x0400-0x0407: Ƶ��΢���ͱ�־ (8�ֽ�)
 */
void Calibration_LoadFromEEPROM(void)
{
    // ��ȡ���ƫ��
    if (BL24C16_Read(0x0010, (uint8_t*)&g_calibration_data.forward_offset, sizeof(float)) != EEPROM_OK) {
        g_calibration_data.forward_offset = 0.0f;
    }
    if (BL24C16_Read(0x0014, (uint8_t*)&g_calibration_data.reflected_offset, sizeof(float)) != EEPROM_OK) {
        g_calibration_data.reflected_offset = 0.0f;
    }

    // ��ȡ������У׼�� (20���㣬ÿ����8�ֽڣ���160�ֽ�)
    // �ֿ��ȡ�Ա��ⵥ�ζ�ȡ��������
    for (int i = 0; i < 20; i++) {
        uint16_t addr = 0x0020 + i * sizeof(PowerCalPoint_t);
        if (BL24C16_Read(addr, (uint8_t*)&g_calibration_data.fwd_table[i], sizeof(PowerCalPoint_t)) != EEPROM_OK) {
            // ʹ��Ĭ��ֵ (0V=0W, 2V=2kW�����Թ�ϵ)
            g_calibration_data.fwd_table[i].power = power_cal_points[i];
            g_calibration_data.fwd_table[i].voltage = power_cal_points[i] / 1000.0f;  // 1000W/Vϵ��
        }
    }

    // ��ȡ���书��У׼�� (��ַ��0x0120��ʼ��������������ͻ)
    for (int i = 0; i < 20; i++) {
        uint16_t addr = 0x0120 + i * sizeof(PowerCalPoint_t);
        if (BL24C16_Read(addr, (uint8_t*)&g_calibration_data.ref_table[i], sizeof(PowerCalPoint_t)) != EEPROM_OK) {
            // ʹ��Ĭ��ֵ (0V=0W, 2V=2kW�����Թ�ϵ)
            g_calibration_data.ref_table[i].power = power_cal_points[i];
            g_calibration_data.ref_table[i].voltage = power_cal_points[i] / 1000.0f;  // 1000W/Vϵ��
        }
    }

    // ��ȡУ׼���� (������ַ������У׼���ͻ)
    if (BL24C16_Read(0x0220, &g_calibration_data.fwd_points, 1) != EEPROM_OK) {
        g_calibration_data.fwd_points = 0;
    }
    if (BL24C16_Read(0x0221, &g_calibration_data.ref_points, 1) != EEPROM_OK) {
        g_calibration_data.ref_points = 0;
    }

    // ��ȡƵ����������ϵ��
    if (BL24C16_Read(0x0300, (uint8_t*)&g_calibration_data.freq_gain_fwd, sizeof(float)) != EEPROM_OK) {
        g_calibration_data.freq_gain_fwd = 1.0f;
    }
    if (BL24C16_Read(0x0304, (uint8_t*)&g_calibration_data.freq_gain_ref, sizeof(float)) != EEPROM_OK) {
        g_calibration_data.freq_gain_ref = 1.0f;
    }
    if (BL24C16_Read(0x0308, (uint8_t*)&g_calibration_data.cal_frequency, sizeof(float)) != EEPROM_OK) {
        g_calibration_data.cal_frequency = 14.0f;  // Ĭ��14MHz
    }

    // ��֤Ƶ������������Ч��
    if (g_calibration_data.freq_gain_fwd < 0.1f || g_calibration_data.freq_gain_fwd > 10.0f ||
        isnan(g_calibration_data.freq_gain_fwd) || isinf(g_calibration_data.freq_gain_fwd)) {
        g_calibration_data.freq_gain_fwd = 1.0f;

    }
    if (g_calibration_data.freq_gain_ref < 0.1f || g_calibration_data.freq_gain_ref > 10.0f ||
        isnan(g_calibration_data.freq_gain_ref) || isinf(g_calibration_data.freq_gain_ref)) {
        g_calibration_data.freq_gain_ref = 1.0f;

    }

    // ��ȡƵ��΢�� (������ַ�����ͻ)
    if (BL24C16_Read(0x0400, (uint8_t*)&g_calibration_data.freq_trim, sizeof(float)) != EEPROM_OK) {
        g_calibration_data.freq_trim = 1.0f;
    }

    // ��֤freq_trim������Ч�ԣ���ֹEEPROM��
    if (g_calibration_data.freq_trim < 0.1f || g_calibration_data.freq_trim > 10.0f ||
        isnan(g_calibration_data.freq_trim) || isinf(g_calibration_data.freq_trim)) {
        g_calibration_data.freq_trim = 1.0f;  // ����Ϊ��ȫĬ��ֵ

    }

    // ��ȡУ׼��ɱ�־
    if (BL24C16_Read(0x0404, &g_calibration_data.is_calibrated, 1) != EEPROM_OK) {
        g_calibration_data.is_calibrated = 0;
    }

    // һ��������𻵵�EEPROM���ݣ���ʱ�޸���
    static uint8_t eeprom_cleared = 0;
    if (!eeprom_cleared) {

        g_calibration_data.freq_trim = 1.0f;
        g_calibration_data.freq_gain_fwd = 1.0f;
        g_calibration_data.freq_gain_ref = 1.0f;
        g_calibration_data.cal_frequency = 14.0f;

        // ������ȷ�����ݵ�EEPROM
        BL24C16_Write(0x0400, (uint8_t*)&g_calibration_data.freq_trim, sizeof(float));
        BL24C16_Write(0x0300, (uint8_t*)&g_calibration_data.freq_gain_fwd, sizeof(float));
        BL24C16_Write(0x0304, (uint8_t*)&g_calibration_data.freq_gain_ref, sizeof(float));
        BL24C16_Write(0x0308, (uint8_t*)&g_calibration_data.cal_frequency, sizeof(float));

        eeprom_cleared = 1;

    }


}

/**
 * @brief ����У׼���ݵ�EEPROM
 */
void Calibration_SaveToEEPROM(void)
{
    // �������ƫ��
    BL24C16_Write(0x0010, (uint8_t*)&g_calibration_data.forward_offset, sizeof(float));
    BL24C16_Write(0x0014, (uint8_t*)&g_calibration_data.reflected_offset, sizeof(float));

    // ����������У׼�� (�ֿ鱣����ȷ���ɿ���)
    for (int i = 0; i < 20; i++) {
        uint16_t addr = 0x0020 + i * sizeof(PowerCalPoint_t);
        BL24C16_Write(addr, (uint8_t*)&g_calibration_data.fwd_table[i], sizeof(PowerCalPoint_t));
        HAL_Delay(5);  // д������ȷ��EEPROMд�����
    }

    // ���淴�书��У׼�� (��ַ��0x0120��ʼ)
    for (int i = 0; i < 20; i++) {
        uint16_t addr = 0x0120 + i * sizeof(PowerCalPoint_t);
        BL24C16_Write(addr, (uint8_t*)&g_calibration_data.ref_table[i], sizeof(PowerCalPoint_t));
        HAL_Delay(5);  // д������ȷ��EEPROMд�����
    }

    // ����У׼���� (ʹ���µ�ַ)
    BL24C16_Write(0x0220, &g_calibration_data.fwd_points, 1);
    BL24C16_Write(0x0221, &g_calibration_data.ref_points, 1);

    // ����Ƶ����������ϵ��
    BL24C16_Write(0x0300, (uint8_t*)&g_calibration_data.freq_gain_fwd, sizeof(float));
    BL24C16_Write(0x0304, (uint8_t*)&g_calibration_data.freq_gain_ref, sizeof(float));
    BL24C16_Write(0x0308, (uint8_t*)&g_calibration_data.cal_frequency, sizeof(float));

    // ����Ƶ��΢�� (ʹ���µ�ַ)
    BL24C16_Write(0x0400, (uint8_t*)&g_calibration_data.freq_trim, sizeof(float));

    // ����У׼��ɱ�־
    g_calibration_data.is_calibrated = 1;
    BL24C16_Write(0x0404, &g_calibration_data.is_calibrated, 1);
}



/**
 * @brief �����㹦�ʣ����Բ�ֵ���Ż�֧��0W-2kW��Χ��
 */
float Calibration_CalculatePowerFromTable(float voltage, PowerCalPoint_t* table, uint8_t points)
{
    if (points == 0) {
        return 0.0f;  // ��У׼����
    }

    // ����0W�������ѹ�ӽ�0ʱ����0����
    if (voltage <= 0.001f) {  // 1mV������Ϊ��0����
        return 0.0f;
    }

    // �����ѹС�ڵ�һ��У׼�㣬ʹ�õ�һ�����б������
    if (voltage <= table[0].voltage) {
        if (points >= 2) {
            float slope = (table[1].power - table[0].power) / (table[1].voltage - table[0].voltage);
            float extrapolated_power = table[0].power + slope * (voltage - table[0].voltage);
            return (extrapolated_power > 0.0f) ? extrapolated_power : 0.0f;  // ȷ�������ظ�����
        } else {
            return table[0].power * (voltage / table[0].voltage);  // ���Ա���
        }
    }

    // �����ѹ�������һ���㣬ʹ������������б�����ƣ�֧��2kW���ϣ�
    if (voltage >= table[points-1].voltage) {
        if (points >= 2) {
            float slope = (table[points-1].power - table[points-2].power) /
                         (table[points-1].voltage - table[points-2].voltage);
            return table[points-1].power + slope * (voltage - table[points-1].voltage);
        } else {
            return table[points-1].power * (voltage / table[points-1].voltage);
        }
    }

    // �ڱ��Χ�ڣ��ҵ���Ӧ����������Բ�ֵ
    for (uint8_t i = 0; i < points - 1; i++) {
        if (voltage >= table[i].voltage && voltage <= table[i+1].voltage) {
            float ratio = (voltage - table[i].voltage) / (table[i+1].voltage - table[i].voltage);
            return table[i].power + ratio * (table[i+1].power - table[i].power);
        }
    }

    return 0.0f;  // ��Ӧ�õ�������
}

/**
 * @brief Ӧ��У׼�������µĲ��ʽ��
 */
float Calibration_ApplyCorrection(float raw_voltage, uint8_t is_forward, float frequency)
{
    if (!g_calibration_data.is_calibrated) {
        // δУ׼ʱʹ�ü�����ת�� (0V=0W, 2V=2kW)
        return (raw_voltage - (is_forward ? g_calibration_data.forward_offset : g_calibration_data.reflected_offset)) * 1000.0f;
    }

    // ȥ�����ƫ��
    float corrected_voltage = raw_voltage - (is_forward ? g_calibration_data.forward_offset : g_calibration_data.reflected_offset);
    if (corrected_voltage < 0.0f) {
        corrected_voltage = 0.0f;
    }

    // �����㹦��
    float base_power;
    if (is_forward) {
        base_power = Calibration_CalculatePowerFromTable(corrected_voltage, g_calibration_data.fwd_table, g_calibration_data.fwd_points);
    } else {
        base_power = Calibration_CalculatePowerFromTable(corrected_voltage, g_calibration_data.ref_table, g_calibration_data.ref_points);
    }

    // Ӧ��Ƶ����������
    float final_power = base_power * (is_forward ? g_calibration_data.freq_gain_fwd : g_calibration_data.freq_gain_ref);

    return (final_power > 0.0f) ? final_power : 0.0f;
}

/**
 * @brief ���㹦�ʣ��򻯽ӿڣ��Զ���ȡ��ǰƵ�ʣ�
 */
float Calibration_CalculatePower(float voltage, uint8_t is_forward)
{
    // ��ʱ���ԣ�ֱ�ӵ��ò����������Ƶ������
    if (!g_calibration_data.is_calibrated) {
        // δУ׼ʱʹ�ü�����ת�� (0V=0W, 2V=2kW)
        return (voltage - (is_forward ? g_calibration_data.forward_offset : g_calibration_data.reflected_offset)) * 1000.0f;
    }

    // ȥ�����ƫ��
    float corrected_voltage = voltage - (is_forward ? g_calibration_data.forward_offset : g_calibration_data.reflected_offset);
    if (corrected_voltage < 0.0f) {
        corrected_voltage = 0.0f;
    }

    // ֱ�Ӳ����㹦�ʣ���ʱ��Ӧ��Ƶ����������
    float base_power;
    if (is_forward) {
        base_power = Calibration_CalculatePowerFromTable(corrected_voltage, g_calibration_data.fwd_table, g_calibration_data.fwd_points);
    } else {
        base_power = Calibration_CalculatePowerFromTable(corrected_voltage, g_calibration_data.ref_table, g_calibration_data.ref_points);
    }

    return (base_power > 0.0f) ? base_power : 0.0f;
}

/**
 * @brief ��ʼУ׼����
 */
void Calibration_StartStep(CalibrationStep_t step)
{
    g_calibration_state.current_step = step;
    g_calibration_state.sample_count = 0;
    g_calibration_state.sample_sum_fwd = 0.0f;
    g_calibration_state.sample_sum_ref = 0.0f;
    g_calibration_state.sample_completed = 0;  // ������ɱ�־
    g_calibration_state.is_stable = 0;
    g_calibration_state.stable_count = 0;

    g_interface_manager.need_refresh = 1;
}

/**
 * @brief ��ʼ�����ʱ궨��ֻ�ڿ�ʼʱ����һ�Σ�
 */
void Calibration_InitPowerStep(void)
{
    g_calibration_state.current_power_point = 0;
    g_calibration_state.current_channel = 0;  // �������ʿ�ʼ
    g_calibration_state.target_power = power_cal_points[0];  // 100W
    g_calibration_state.power_cal_mode = 0;
}

/**
 * @brief ��ʼ��Ƶ�ʱ궨��ֻ�ڿ�ʼʱ����һ�Σ�
 */
void Calibration_InitBandStep(void)
{
    g_calibration_state.cal_frequency = 14.0f;  // Ĭ��14MHz
    g_calibration_state.target_power = 100.0f;  // Ƶ�ʱ궨ʹ�ù̶�����
}

/**
 * @brief ��ȡADC��ѹֵ
 */
static void Calibration_ReadADCVoltage(float* fwd_voltage, float* ref_voltage)
{
    // ����ADCת������ȡֵ
    extern ADC_HandleTypeDef hadc1;

    // ���ò���ȡChannel 2 (PA2������)
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_2;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint32_t adc_forward = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    // ���ò���ȡChannel 3 (PA3���书��)
    sConfig.Channel = ADC_CHANNEL_3;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint32_t adc_reflected = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    // ת��Ϊ��ѹֵ (2.5V�ο���ѹ��12λADC)
    *fwd_voltage = (float)adc_forward * 2.5f / 4095.0f;
    *ref_voltage = (float)adc_reflected * 2.5f / 4095.0f;
}

/**
 * @brief ��ʾADC������Ϣ������У׼���棩
 * @param x: X����λ��
 * @param y: Y������ʼλ��
 */
static void Display_ADCInfo(uint16_t x, uint16_t y)
{
    char str_buffer[16];
    uint32_t adc_forward, adc_reflected;
    float fwd_voltage, ref_voltage;

    // ����ADCת������ȡֵ
    extern ADC_HandleTypeDef hadc1;

    // ���ò���ȡChannel 2 (PA2������)
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_2;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    adc_forward = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    // ���ò���ȡChannel 3 (PA3���书��)
    sConfig.Channel = ADC_CHANNEL_3;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);

    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    adc_reflected = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    // ת��Ϊ��ѹֵ (2.5V�ο���ѹ��12λADC)
    fwd_voltage = (float)adc_forward * 2.5f / 4095.0f;
    ref_voltage = (float)adc_reflected * 2.5f / 4095.0f;

    // ��ʾADCԭʼֵ
    sprintf(str_buffer, "F:%4d", (int)adc_forward);
    Show_Str(x, y, GRAY, BLACK, (uint8_t*)str_buffer, 12, 0);

    sprintf(str_buffer, "R:%4d", (int)adc_reflected);
    Show_Str(x, y + 15, GRAY, BLACK, (uint8_t*)str_buffer, 12, 0);

    // ��ʾ��ѹֵ
    sprintf(str_buffer, "%5.2fV", fwd_voltage);
    Show_Str(x, y + 30, GRAY, BLACK, (uint8_t*)str_buffer, 12, 0);

    sprintf(str_buffer, "%5.2fV", ref_voltage);
    Show_Str(x, y + 45, GRAY, BLACK, (uint8_t*)str_buffer, 12, 0);
}

/**
 * @brief ����У׼����
 */
void Calibration_ProcessSample(void)
{
    static float last_fwd = 0.0f;
    static float last_ref = 0.0f;

    // ��ȡ��ǰADC��ѹֵ
    float current_fwd, current_ref;
    Calibration_ReadADCVoltage(&current_fwd, &current_ref);

    // �ȶ��Լ�⣨�仯С��5%��Ϊ�ȶ���
    if (fabs(current_fwd - last_fwd) < 0.05f * last_fwd &&
        fabs(current_ref - last_ref) < 0.05f * last_ref) {
        g_calibration_state.stable_count++;
        if (g_calibration_state.stable_count >= 10) {  // �ȶ�1�루100ms��10��
            g_calibration_state.is_stable = 1;
        }
    } else {
        g_calibration_state.stable_count = 0;
        g_calibration_state.is_stable = 0;
    }

    last_fwd = current_fwd;
    last_ref = current_ref;

    // ������ڲ������ۼ�����
    if (g_calibration_state.sample_count > 0) {
        g_calibration_state.sample_sum_fwd += current_fwd;
        g_calibration_state.sample_sum_ref += current_ref;
        g_calibration_state.sample_count++;

        // ������ɣ�10�Σ�
        if (g_calibration_state.sample_count >= 10) {
            // �޸���ʵ�ʲ���������sample_count-1�Σ���1��ʼ������
            uint8_t actual_samples = g_calibration_state.sample_count - 1;
            float avg_fwd = g_calibration_state.sample_sum_fwd / (float)actual_samples;
            float avg_ref = g_calibration_state.sample_sum_ref / (float)actual_samples;

            printf("Actual samples taken: %d\r\n", actual_samples);

            // ������ɱ�־����ʾ�����ʾ
            g_calibration_state.sample_completed = 1;
            g_interface_manager.need_refresh = 1;

            // ���ڵ�������������
            printf("\r\n=== Calibration Sample Complete ===\r\n");
            printf("Step: %d\r\n", g_calibration_state.current_step);
            printf("Average Forward: %.3fV\r\n", avg_fwd);
            printf("Average Reflected: %.3fV\r\n", avg_ref);

            // ���ݵ�ǰ���账��������
            switch (g_calibration_state.current_step) {
                case CAL_STEP_ZERO:
                    g_calibration_data.forward_offset = avg_fwd;
                    g_calibration_data.reflected_offset = avg_ref;

                    printf("Zero calibration: Fwd=%.3fV, Ref=%.3fV\r\n", avg_fwd, avg_ref);

                    // ����������ݵ�EEPROM
                    Calibration_SaveToEEPROM();

                    InterfaceManager_Beep(200);  // �ɹ���ʾ��

                    // �ӳٺ������һ���򷵻�ѡ�����
                    HAL_Delay(1000);  // ��ʾ�����ʾ1��
                    if (g_calibration_state.is_single_step) {
                        // �����궨��ɣ����ز���ѡ�����
                        InterfaceManager_SwitchTo(INTERFACE_CAL_STEP_SELECT);
                    } else {
                        // �����궨��������һ��������У׼
                        Calibration_InitPowerStep();  // ��ʼ�����ʱ궨״̬
                        Calibration_StartStep(CAL_STEP_POWER);
                        InterfaceManager_SwitchTo(INTERFACE_CAL_POWER);
                    }
                    break;

                case CAL_STEP_POWER:
                    {
                        // ��㹦��У׼
                        uint8_t point = g_calibration_state.current_power_point;
                        uint8_t channel = g_calibration_state.current_channel;

                        if (channel == 0) {  // ������У׼
                            float corrected_voltage = avg_fwd - g_calibration_data.forward_offset;
                            g_calibration_data.fwd_table[point].power = g_calibration_state.target_power;
                            g_calibration_data.fwd_table[point].voltage = corrected_voltage;
                            g_calibration_data.fwd_points = point + 1;

                            printf("Forward Power Cal P%d: %.0fW @ %.3fV (Raw=%.3fV, Offset=%.3fV)\r\n",
                                   point, g_calibration_state.target_power, corrected_voltage, avg_fwd, g_calibration_data.forward_offset);
                        } else {  // ���书��У׼
                            float corrected_voltage = avg_ref - g_calibration_data.reflected_offset;
                            g_calibration_data.ref_table[point].power = g_calibration_state.target_power;
                            g_calibration_data.ref_table[point].voltage = corrected_voltage;
                            g_calibration_data.ref_points = point + 1;

                            printf("Reflected Power Cal P%d: %.0fW @ %.3fV (Raw=%.3fV, Offset=%.3fV)\r\n",
                                   point, g_calibration_state.target_power, corrected_voltage, avg_ref, g_calibration_data.reflected_offset);
                        }
                        InterfaceManager_Beep(200);

                        // ���浱ǰ�궨�㵽EEPROM
                        Calibration_SaveToEEPROM();

                        // �ӳ���ʾ�����ʾ
                        HAL_Delay(800);  // ��ʾ�����ʾ0.8��

                        // �Զ�������һ��У׼��
                        if (channel == 0) {  // ������У׼
                            if (point < 19) {  // ���и��������
                                g_calibration_state.current_power_point++;
                                g_calibration_state.target_power = power_cal_points[g_calibration_state.current_power_point];
                                Calibration_StartStep(CAL_STEP_POWER);  // ���ò���״̬
                            } else {
                                // �л������书��У׼
                                g_calibration_state.current_channel = 1;
                                g_calibration_state.current_power_point = 0;
                                g_calibration_state.target_power = power_cal_points[0];
                                Calibration_StartStep(CAL_STEP_POWER);  // ���ò���״̬
                            }
                        } else {  // ���书��У׼
                            if (point < 19) {  // ���и��෴���
                                g_calibration_state.current_power_point++;
                                g_calibration_state.target_power = power_cal_points[g_calibration_state.current_power_point];
                                Calibration_StartStep(CAL_STEP_POWER);  // ���ò���״̬
                            } else {
                                // ��ɹ���У׼
                                if (g_calibration_state.is_single_step) {
                                    // �����궨��ɣ����ز���ѡ�����
                                    InterfaceManager_SwitchTo(INTERFACE_CAL_STEP_SELECT);
                                } else {
                                    // �����궨��������һ��
                                    Calibration_InitBandStep();  // ��ʼ��Ƶ�ʱ궨״̬
                                    Calibration_StartStep(CAL_STEP_BAND);
                                    InterfaceManager_SwitchTo(INTERFACE_CAL_BAND);
                                }
                                return;
                            }
                        }
                    }
                    break;

                case CAL_STEP_BAND:
                    {
                        // ʹ�ò��ʽ�����׼����
                        float base_fwd = Calibration_CalculatePowerFromTable(avg_fwd - g_calibration_data.forward_offset,
                                                                            g_calibration_data.fwd_table,
                                                                            g_calibration_data.fwd_points);
                        float base_ref = Calibration_CalculatePowerFromTable(avg_ref - g_calibration_data.reflected_offset,
                                                                            g_calibration_data.ref_table,
                                                                            g_calibration_data.ref_points);

                        // ����Ƶ����������ϵ��
                        if (base_fwd > 0.0f) {
                            g_calibration_data.freq_gain_fwd = g_calibration_state.target_power / base_fwd;
                        }
                        if (base_ref > 0.0f) {
                            g_calibration_data.freq_gain_ref = g_calibration_state.target_power / base_ref;
                        }

                        // ����궨Ƶ��
                        g_calibration_data.cal_frequency = g_calibration_state.cal_frequency;

                        // ����Ƶ��΢��ϵ��
                        extern FreqResult_t g_freq_result;
                        if (g_freq_result.is_valid) {
                            // ��ǰ������ԭʼƵ�ʣ�Hz����ʹ�õ�ǰ��΢��ϵ��
                            float current_measured_hz = (float)g_freq_result.frequency_hz * 16.0f * g_calibration_data.freq_trim;
                            // Ŀ��Ƶ�ʣ�Hz��
                            float target_freq_hz = g_calibration_state.cal_frequency * 1000000.0f;
                            // �����µ�΢��ϵ������ǰ΢�� �� (Ŀ��Ƶ�� / ��ǰ��ʾƵ��)
                            if (current_measured_hz > 0.0f) {
                                float correction_factor = target_freq_hz / current_measured_hz;
                                g_calibration_data.freq_trim = g_calibration_data.freq_trim * correction_factor;

                            }
                        }

                        InterfaceManager_Beep(200);

                        // �ӳ���ʾ�����ʾ
                        HAL_Delay(1000);  // ��ʾ�����ʾ1��

                        // Ƶ�ʱ궨��ɣ���������
                        Calibration_SaveToEEPROM();
                        if (g_calibration_state.is_single_step) {
                            // �����궨��ɣ����ز���ѡ�����
                            InterfaceManager_SwitchTo(INTERFACE_CAL_STEP_SELECT);
                        } else {
                            // �����궨���
                            Calibration_StartStep(CAL_STEP_COMPLETE);
                            InterfaceManager_SwitchTo(INTERFACE_CAL_COMPLETE);
                        }
                    }
                    break;



                default:
                    break;
            }

            // ���ò���״̬
            g_calibration_state.sample_count = 0;
            g_calibration_state.sample_sum_fwd = 0.0f;
            g_calibration_state.sample_sum_ref = 0.0f;
            g_interface_manager.need_refresh = 1;
        }
    }
}

/* ========== У׼������ʾ���� ========== */

/**
 * @brief ��ʾУ׼ȷ��ҳ
 */
void Interface_DisplayCalConfirm(void)
{
    Show_Str(30, 5, WHITE, BLACK, (uint8_t*)"Calibration", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(10, 30, YELLOW, BLACK, (uint8_t*)"WARNING:", 16, 0);
    Show_Str(10, 50, WHITE, BLACK, (uint8_t*)"This will reset", 12, 0);
    Show_Str(10, 65, WHITE, BLACK, (uint8_t*)"all cal data!", 12, 0);

    Show_Str(10, 80, CYAN, BLACK, (uint8_t*)"Prepare:", 12, 0);
    Show_Str(10, 95, WHITE, BLACK, (uint8_t*)"50ohm load ready", 12, 0);

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"OK:Start UP:Cancel", 12, 0);
}

/**
 * @brief ��ʾ���У׼
 */
void Interface_DisplayCalZero(void)
{
    // �״ν���ʱ��������ֹ����
    if (g_interface_manager.interface_first_enter) {
        g_interface_manager.interface_first_enter = 0;
        LCD_Clear(BLACK);
    }
    Show_Str(35, 5, WHITE, BLACK, (uint8_t*)"Zero Cal", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(10, 30, CYAN, BLACK, (uint8_t*)"Step 1/3:", 12, 0);
    Show_Str(10, 45, WHITE, BLACK, (uint8_t*)"Turn OFF RF", 16, 0);
    Show_Str(10, 65, WHITE, BLACK, (uint8_t*)"No signal input", 12, 0);

    if (g_calibration_state.sample_completed) {
        Show_Str(10, 92, GREEN, BLACK, (uint8_t*)"COMPLETED!      ", 12, 0);
    } else if (g_calibration_state.sample_count > 0) {
        Show_Str(10, 92, YELLOW, BLACK, (uint8_t*)"Sampling...     ", 12, 0);
        char progress[16];
        sprintf(progress, "%2d/10 ", g_calibration_state.sample_count);
        Show_Str(100, 92, YELLOW, BLACK, (uint8_t*)progress, 12, 0);
    } else {
        Show_Str(10, 92, GREEN, BLACK, (uint8_t*)"Ready to sample ", 12, 0);
    }

    // ��ʾADC������Ϣ
    Display_ADCInfo(115, 30);



    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"OK:Sample UP:Back", 12, 0);
}

/**
 * @brief ��ʾ����У׼
 */
void Interface_DisplayCalPower(void)
{
    char str_buffer[32];

    Show_Str(30, 5, WHITE, BLACK, (uint8_t*)"Power Cal", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(10, 30, CYAN, BLACK, (uint8_t*)"Step 2/3:", 12, 0);

    // ��ʾ��ǰУ׼ͨ���͵�
    if (g_calibration_state.current_channel == 0) {
        sprintf(str_buffer, "FWD Point %2d/20 ", g_calibration_state.current_power_point + 1);
    } else {
        sprintf(str_buffer, "REF Point %2d/20 ", g_calibration_state.current_power_point + 1);
    }
    Show_Str(10, 45, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    // ��ʾ��ǰУ׼����Ϣ
    const char* channel_name = (g_calibration_state.current_channel == 0) ? "Fwd" : "Ref";
    sprintf(str_buffer, "Point %2d/20 (%s)  ", g_calibration_state.current_power_point + 1, channel_name);
    Show_Str(10, 60, CYAN, BLACK, (uint8_t*)str_buffer, 12, 0);

    // ��ʾĿ�깦�ʣ�֧��kW��λ���̶���ȷ�ֹ����
    if (g_calibration_state.target_power >= 1000.0f) {
        sprintf(str_buffer, "Target: %4.1fkW  ", g_calibration_state.target_power / 1000.0f);
    } else {
        sprintf(str_buffer, "Target: %4.0fW   ", g_calibration_state.target_power);
    }
    Show_Str(10, 75, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    if (g_calibration_state.is_stable) {
        Show_Str(10, 90, GREEN, BLACK, (uint8_t*)"STABLE - Ready   ", 12, 0);
    } else {
        Show_Str(10, 90, YELLOW, BLACK, (uint8_t*)"Set power & wait ", 12, 0);
    }

    if (g_calibration_state.sample_completed) {
        Show_Str(10, 103, GREEN, BLACK, (uint8_t*)"COMPLETED!      ", 12, 0);
    } else if (g_calibration_state.sample_count > 0) {
        Show_Str(10, 103, YELLOW, BLACK, (uint8_t*)"Sampling...     ", 12, 0);
        char progress[16];
        sprintf(progress, "%2d/10 ", g_calibration_state.sample_count);
        Show_Str(100, 103, YELLOW, BLACK, (uint8_t*)progress, 12, 0);
    }

    // ��ʾADC������Ϣ
    Display_ADCInfo(119, 30);

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Prev DOWN:Next OK:Cal", 12, 0);
}

/**
 * @brief ��ʾƵ�ʱ궨
 */
void Interface_DisplayCalBand(void)
{
    char str_buffer[32];

    Show_Str(35, 5, WHITE, BLACK, (uint8_t*)"Freq Cal", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(10, 30, CYAN, BLACK, (uint8_t*)"Step 3/3:", 12, 0);

    // ��ʾ�궨Ƶ��
    sprintf(str_buffer, "Cal: %5.1f MHz    ", g_calibration_state.cal_frequency);
    Show_Str(10, 45, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    // ��ʾʵ�ʲ���Ƶ�ʣ�Ӧ��16��������΢����
    extern FreqResult_t g_freq_result;
    if (g_freq_result.is_valid) {
        // ������ʵƵ�ʣ�ԭʼֵ �� 16������ �� ΢��ϵ��
        float real_freq_hz = (float)g_freq_result.frequency_hz * 16.0f * g_calibration_data.freq_trim;

        if (real_freq_hz >= 1000000.0f) {  // >= 1MHz
            sprintf(str_buffer, "Meas: %5.6f MHz   ", real_freq_hz / 1000000.0f);
        } else if (real_freq_hz >= 1000.0f) {  // >= 1kHz
            sprintf(str_buffer, "Meas: %5.3f kHz   ", real_freq_hz / 1000.0f);
        } else {  // < 1kHz
            sprintf(str_buffer, "Meas: %5.0f Hz    ", real_freq_hz);
        }
    } else {
        sprintf(str_buffer, "Meas: No Signal   ");
    }
    Show_Str(10, 60, CYAN, BLACK, (uint8_t*)str_buffer, 12, 0);



    if (g_calibration_state.is_stable) {
        Show_Str(10, 75, GREEN, BLACK, (uint8_t*)"STABLE - Ready   ", 12, 0);
    } else {
        Show_Str(10, 75, YELLOW, BLACK, (uint8_t*)"Set freq & power ", 12, 0);
    }

    if (g_calibration_state.sample_completed) {
        Show_Str(10, 88, GREEN, BLACK, (uint8_t*)"COMPLETED!      ", 12, 0);
    } else if (g_calibration_state.sample_count > 0) {
        Show_Str(10, 88, YELLOW, BLACK, (uint8_t*)"Sampling...     ", 12, 0);
        char progress[16];
        sprintf(progress, "%2d/10 ", g_calibration_state.sample_count);
        Show_Str(100, 88, YELLOW, BLACK, (uint8_t*)progress, 12, 0);
    }

    // ��ʾADC������Ϣ
    //Display_ADCInfo(122, 30);

    Show_Str(5, 103, GRAY, BLACK, (uint8_t*)"DOWN:Freq(1-50MHz)   ", 12, 0);
    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"OK:Cal UP:Back       ", 12, 0);
}



/**
 * @brief ��ʾУ׼���
 */
void Interface_DisplayCalComplete(void)
{
    Show_Str(25, 5, WHITE, BLACK, (uint8_t*)"Cal Complete", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(50, 40, GREEN, BLACK, (uint8_t*)"SUCCESS!", 16, 0);
    Show_Str(10, 60, WHITE, BLACK, (uint8_t*)"All data saved", 12, 0);
    Show_Str(10, 75, WHITE, BLACK, (uint8_t*)"to EEPROM", 12, 0);

    Show_Str(10, 90, CYAN, BLACK, (uint8_t*)"Cal status: ON", 12, 0);

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"OK:Main UP:Menu", 12, 0);
}

/**
 * @brief ��������ʾ
 */
void Interface_DisplayMain(void)
{
    char str_buffer[32];
    uint16_t vswr_color;

    // ��ʾ����
    Show_Str(20, 5, WHITE, BLACK, (uint8_t*)"RF Power Meter", 16, 0); //��Ƶ���ʼƱ���

    // ���Ʒָ���
    LCD_DrawLine(0, 25, LCD_W - 1, 25); //ˮƽ�ָ���
    LCD_DrawLine(80, 25, 80, 95); //��ֱ�ָ���

    // �����ʾ������Ϣ
    Show_Str(5, 30, CYAN, BLACK, (uint8_t*)"Forward:", 12, 0); //�����ʱ�ǩ
    if (g_power_result.is_valid) {
        sprintf(str_buffer, "%5.0fW", g_power_result.forward_power);
        Show_Str(5, 45, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //��������ֵ
    } else {
        Show_Str(5, 45, GRAY, BLACK, (uint8_t*)"   --W", 12, 0); //��������Ч��ʾ
    }

    Show_Str(5, 60, CYAN, BLACK, (uint8_t*)"Reflect:", 12, 0); //���书�ʱ�ǩ
    if (g_power_result.is_valid) {
        sprintf(str_buffer, "%5.0fW", g_power_result.reflected_power);
        Show_Str(5, 75, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //���书����ֵ
    } else {
        Show_Str(5, 75, GRAY, BLACK, (uint8_t*)"   --W", 12, 0); //���书����Ч��ʾ
    }

    // �Ҳ���ʾ��Ƶ����
    Show_Str(85, 30, CYAN, BLACK, (uint8_t*)"VSWR:", 12, 0); //פ���ȱ�ǩ
    if (g_rf_params.is_valid) {
        // ����VSWRֵѡ����ɫ
        switch (g_rf_params.vswr_color) {
            case VSWR_COLOR_GREEN:  vswr_color = GREEN; break;
            case VSWR_COLOR_YELLOW: vswr_color = YELLOW; break;
            case VSWR_COLOR_RED:    vswr_color = RED; break;
            default:                vswr_color = WHITE; break;
        }

        // �������ʾ����
        //Show_Str(85, 45, BLACK, BLACK, (uint8_t*)"      ", 12, 0);

        // ���⴦����������������ͻط�ֹ��˸
        static uint8_t vswr_is_inf = 0;
        if (g_rf_params.vswr >= 999.0f) {
            vswr_is_inf = 1;  // ����������־
        } else if (g_rf_params.vswr <= 50.0f) {
            vswr_is_inf = 0;  // ֻ�е�VSWR����50���²������־
        }

        if (vswr_is_inf) {
            sprintf(str_buffer, "%6s", "INF");  // ��ʾ����󣬹̶����
        } else {
            sprintf(str_buffer, "%6.2f", g_rf_params.vswr);  // �̶����6�ַ�
        }
        Show_Str(85, 45, vswr_color, BLACK, (uint8_t*)str_buffer, 12, 0); //פ������ֵ(����ɫ)
    } else {
        Show_Str(85, 45, GRAY, BLACK, (uint8_t*)"    --", 12, 0); //פ������Ч��ʾ
    }

    Show_Str(85, 60, CYAN, BLACK, (uint8_t*)"Refl.Coef:", 12, 0); //����ϵ����ǩ
    if (g_rf_params.is_valid) {
        sprintf(str_buffer, "%5.3f", g_rf_params.reflection_coeff);
        Show_Str(85, 75, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //����ϵ����ֵ
    } else {
        Show_Str(85, 75, GRAY, BLACK, (uint8_t*)"   --", 12, 0); //����ϵ����Ч��ʾ
    }

    // �ײ���ʾ����Ч�ʺ�Ƶ��
    LCD_DrawLine(0, 95, LCD_W - 1, 95); //�ײ��ָ���

    Show_Str(5, 100, CYAN, BLACK, (uint8_t*)"Efficiency:", 12, 0); //����Ч�ʱ�ǩ
    if (g_rf_params.is_valid) {
        sprintf(str_buffer, "%5.1f%%", g_rf_params.transmission_eff);
        Show_Str(70, 100, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //����Ч����ֵ
    } else {
        Show_Str(70, 100, GRAY, BLACK, (uint8_t*)"   --%", 12, 0); //����Ч����Ч��ʾ
    }

    Show_Str(5, 115, CYAN, BLACK, (uint8_t*)"Freq:", 12, 0); //Ƶ�ʱ�ǩ
    FreqResult_t freq_result;
    if (FreqCounter_GetResult(&freq_result) == 0) {
        // ʹ��ԭʼfrequency_hzֵ��Ӧ��16��������Ƶ��΢��
        float freq_hz = (float)freq_result.frequency_hz * 16.0f * g_calibration_data.freq_trim;  // ʵ��Ƶ��(Hz)

        if (freq_hz >= 1000000.0f) {  // >= 1MHz
            sprintf(str_buffer, "%4.6f MHz  ", freq_hz / 1000000.0f);
        } else if (freq_hz >= 1000.0f) {  // >= 1kHz����ʾΪkHz��3λС��
            sprintf(str_buffer, "%4.3f kHz    ", freq_hz / 1000.0f);
        } else {  // < 1kHz
            sprintf(str_buffer, "%4.0f Hz     ", freq_hz);
        }
        Show_Str(40, 115, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //Ƶ����ֵ
    } else {
        Show_Str(40, 115, GRAY, BLACK, (uint8_t*)"      -- Hz", 12, 0); //Ƶ����Ч��ʾ
    }
}

/**
 * @brief �˵�������ʾ
 */
void Interface_DisplayMenu(void)
{
    char str_buffer[32];
    const char* menu_items[] = {
        "Calibration",
        "Settings",
        "Alarm Limit",
        "Brightness",
        "About"
    };

    // ��ʾ����
    Show_Str(40, 5, WHITE, BLACK, (uint8_t*)"Settings Menu", 16, 0); //���ò˵�����
    LCD_DrawLine(0, 25, LCD_W - 1, 25); //����ָ���

    // ��ʾ�˵���
    for (int i = 0; i < MAX_MENU_ITEMS - 1; i++) {
        uint16_t color = (i == g_interface_manager.menu_cursor) ? YELLOW : WHITE;
        uint16_t bg_color = (i == g_interface_manager.menu_cursor) ? BLUE : BLACK;

        sprintf(str_buffer, "> %s", menu_items[i]);
        Show_Str(10, 28 + i * 18, color, bg_color, (uint8_t*)str_buffer, 16, 0); //�˵���(��ǰѡ��Ϊ��ɫ������ɫ)
    }

    // ��ʾ������ʾ
    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back DN:Next OK:Enter", 12, 0); //������ʾ(��ע��)
}

/**
 * @brief У׼������ʾ�����ҳ�棩
 */
void Interface_DisplayCalibration(void)
{
    Show_Str(40, 5, WHITE, BLACK, (uint8_t*)"Calibration", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(10, 30, CYAN, BLACK, (uint8_t*)"Cal Status:", 12, 0);
    if (g_calibration_data.is_calibrated) {
        Show_Str(10, 45, GREEN, BLACK, (uint8_t*)"CALIBRATED", 16, 0);
    } else {
        Show_Str(10, 45, RED, BLACK, (uint8_t*)"NOT CALIBRATED", 16, 0);
    }

    Show_Str(10, 65, WHITE, BLACK, (uint8_t*)"5-Step Wizard:", 12, 0);
    Show_Str(10, 80, WHITE, BLACK, (uint8_t*)"Zero->Power->Band", 12, 0);
    Show_Str(10, 95, WHITE, BLACK, (uint8_t*)"->Reflect->Freq", 12, 0);

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"OK:Start UP:Back", 12, 0);
}

/**
 * @brief �궨����ѡ�������ʾ
 */
void Interface_DisplayCalStepSelect(void)
{
    // �״ν���ʱ��������ֹ����
    if (g_interface_manager.interface_first_enter) {
        g_interface_manager.interface_first_enter = 0;
        LCD_Clear(BLACK);
    }

    Show_Str(25, 5, WHITE, BLACK, (uint8_t*)"Cal Select", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    // ��ʾѡ���ǰѡ���������ʾ
    uint16_t sel_color = GREEN;
    uint16_t normal_color = WHITE;

    Show_Str(10, 35, (g_calibration_state.selected_step == 0) ? sel_color : normal_color,
             BLACK, (uint8_t*)"0. Full Cal", 12, 0);
    Show_Str(10, 50, (g_calibration_state.selected_step == 1) ? sel_color : normal_color,
             BLACK, (uint8_t*)"1. Zero Cal", 12, 0);
    Show_Str(10, 65, (g_calibration_state.selected_step == 2) ? sel_color : normal_color,
             BLACK, (uint8_t*)"2. Power Cal", 12, 0);
    Show_Str(10, 80, (g_calibration_state.selected_step == 3) ? sel_color : normal_color,
             BLACK, (uint8_t*)"3. Freq Cal", 12, 0);

    // ��ʾ��ǰ�궨״̬
    Show_Str(10, 100, CYAN, BLACK, (uint8_t*)"Status:", 12, 0);
    if (g_calibration_data.is_calibrated) {
        Show_Str(60, 100, GREEN, BLACK, (uint8_t*)"CALIBRATED", 12, 0);
    } else {
        Show_Str(60, 100, RED, BLACK, (uint8_t*)"NOT CAL", 12, 0);
    }

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back DOWN:Sel OK:Start", 12, 0);
}

/**
 * @brief �궨������ʾ
 */
void Interface_DisplayStandard(void)
{
    char str_buffer[32];

    Show_Str(40, 5, WHITE, BLACK, (uint8_t*)"Cal Debug", 16, 0); //У׼���Ա���
    LCD_DrawLine(0, 25, LCD_W - 1, 25); //����ָ���

    // ��ȡ��ǰADCֵ�͵�ѹ����ʵʱ����
    extern ADC_HandleTypeDef hadc1;
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = ADC_CHANNEL_2;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint32_t adc_fwd = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);

    float raw_voltage = (float)adc_fwd * 2.5f / 4095.0f;
    float corrected_voltage = raw_voltage - g_calibration_data.forward_offset;
    if (corrected_voltage < 0.0f) corrected_voltage = 0.0f;

    // ���ڵ������
    printf("\r\n=== Calibration Debug Info ===\r\n");
    printf("ADC Raw Value: %d\r\n", (int)adc_fwd);
    printf("Raw Voltage: %.3fV\r\n", raw_voltage);
    printf("Forward Offset: %.3fV\r\n", g_calibration_data.forward_offset);
    printf("Corrected Voltage: %.3fV\r\n", corrected_voltage);
    printf("Is Calibrated: %s\r\n", g_calibration_data.is_calibrated ? "YES" : "NO");
    printf("Forward Points: %d/20\r\n", g_calibration_data.fwd_points);
    printf("Reflected Points: %d/20\r\n", g_calibration_data.ref_points);

    // ��ʾ�������У׼����Ϣ
    if (g_calibration_data.fwd_points > 0) {
        float table_power = Calibration_CalculatePowerFromTable(corrected_voltage, g_calibration_data.fwd_table, g_calibration_data.fwd_points);
        printf("Table Lookup Result: %.0fW\r\n", table_power);

        // ��ʾǰ����У׼��
        printf("Calibration Points (First 5):\r\n");
        for (int i = 0; i < 5 && i < g_calibration_data.fwd_points; i++) {
            printf("  P%d: %.0fW @ %.3fV\r\n", i, g_calibration_data.fwd_table[i].power, g_calibration_data.fwd_table[i].voltage);
        }

        // ��ʾ��󼸸�У׼��
        if (g_calibration_data.fwd_points > 5) {
            printf("Calibration Points (Last 5):\r\n");
            int start = (g_calibration_data.fwd_points > 5) ? g_calibration_data.fwd_points - 5 : 0;
            for (int i = start; i < g_calibration_data.fwd_points; i++) {
                printf("  P%d: %.0fW @ %.3fV\r\n", i, g_calibration_data.fwd_table[i].power, g_calibration_data.fwd_table[i].voltage);
            }
        }

        // ����1.0V������У׼��
        printf("Points near 1.0V:\r\n");
        for (int i = 0; i < g_calibration_data.fwd_points; i++) {
            if (g_calibration_data.fwd_table[i].voltage >= 0.8f && g_calibration_data.fwd_table[i].voltage <= 1.2f) {
                printf("  P%d: %.0fW @ %.3fV (MATCH!)\r\n", i, g_calibration_data.fwd_table[i].power, g_calibration_data.fwd_table[i].voltage);
            }
        }
    } else {
        printf("No calibration data found!\r\n");
    }
    printf("==============================\r\n\r\n");

    // ��ʾʵʱ������Ϣ����Ļ
    sprintf(str_buffer, "ADC:%4d Raw:%4.3f", (int)adc_fwd, raw_voltage);
    Show_Str(10, 30, CYAN, BLACK, (uint8_t*)str_buffer, 12, 0);

    sprintf(str_buffer, "Off:%4.3f Cor:%4.3f", g_calibration_data.forward_offset, corrected_voltage);
    Show_Str(10, 45, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    sprintf(str_buffer, "Cal:%c Pts:%2d/20", g_calibration_data.is_calibrated ? 'Y' : 'N', g_calibration_data.fwd_points);
    Show_Str(10, 60, g_calibration_data.is_calibrated ? GREEN : RED, BLACK, (uint8_t*)str_buffer, 12, 0);

    if (g_calibration_data.fwd_points > 0) {
        float table_power = Calibration_CalculatePowerFromTable(corrected_voltage, g_calibration_data.fwd_table, g_calibration_data.fwd_points);
        sprintf(str_buffer, "Result:%5.0fW", table_power);
        Show_Str(10, 75, YELLOW, BLACK, (uint8_t*)str_buffer, 12, 0);
    } else {
        Show_Str(10, 75, GRAY, BLACK, (uint8_t*)"Result: No Cal", 12, 0);
    }

    Show_Str(10, 90, WHITE, BLACK, (uint8_t*)"Check Serial Output", 12, 0);
    Show_Str(10, 105, WHITE, BLACK, (uint8_t*)"for detailed info", 12, 0);

    Show_Str(10, 115, GRAY, BLACK, (uint8_t*)"UP:Back", 12, 0); //������ʾ
}

/**
 * @brief �������ý�����ʾ
 */
void Interface_DisplayAlarm(void)
{
    char str_buffer[32];

    Show_Str(40, 5, WHITE, BLACK, (uint8_t*)"Alarm Limit", 16, 0); //���ޱ�������
    LCD_DrawLine(0, 25, LCD_W - 1, 25); //����ָ���

    Show_Str(10, 35, CYAN, BLACK, (uint8_t*)"Alarm Setup:", 16, 0); //�������ñ�ǩ

    // ��ʾ����ʹ��״̬����ѡ��ָʾ��
    if (g_interface_manager.alarm_selected_item == 0) {
        Show_Str(5, 55, GREEN, BLACK, (uint8_t*)">", 12, 0); //ѡ��ָʾ��
    } else {
        Show_Str(5, 55, BLACK, BLACK, (uint8_t*)" ", 12, 0); //���ָʾ��
    }
    sprintf(str_buffer, "Alarm Enable: %3s", g_interface_manager.alarm_enabled ? "ON" : "OFF");
    Show_Str(15, 55, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //����ʹ��״̬

    // ��ʾVSWR��ֵ����λ���༭ģʽ��
    // ��ʾVSWRѡ��ָʾ��
    if (g_interface_manager.alarm_selected_item >= 1 && g_interface_manager.alarm_selected_item <= 4) {
        Show_Str(5, 70, GREEN, BLACK, (uint8_t*)">", 12, 0); //VSWRѡ��ָʾ��
    } else {
        Show_Str(5, 70, BLACK, BLACK, (uint8_t*)" ", 12, 0); //���ָʾ��
    }

    Show_Str(15, 70, WHITE, BLACK, (uint8_t*)"VSWR Limit: ", 12, 0);

    // �ֽ�VSWRֵΪ����λ�����޸����㾫�����⣩
    float current_value = g_interface_manager.vswr_alarm_threshold;
    // ת��Ϊ�������⸡�㾫������
    int total_tenths = (int)(current_value * 10.0f + 0.5f);  // �������뵽0.1

    int hundreds = (total_tenths / 1000) % 10;
    int tens = (total_tenths / 100) % 10;
    int ones = (total_tenths / 10) % 10;
    int decimal = total_tenths % 10;

    // ��ʾ��λ����ѡ��ָʾ��
    uint16_t color_h = (g_interface_manager.alarm_selected_item == 1) ? GREEN : WHITE;
    sprintf(str_buffer, "%d", hundreds);
    Show_Str(106, 70, color_h, BLACK, (uint8_t*)str_buffer, 12, 0);

    // ��ʾʮλ����ѡ��ָʾ��
    uint16_t color_t = (g_interface_manager.alarm_selected_item == 2) ? GREEN : WHITE;
    sprintf(str_buffer, "%d", tens);
    Show_Str(112, 70, color_t, BLACK, (uint8_t*)str_buffer, 12, 0);

    // ��ʾ��λ����ѡ��ָʾ��
    uint16_t color_o = (g_interface_manager.alarm_selected_item == 3) ? GREEN : WHITE;
    sprintf(str_buffer, "%d", ones);
    Show_Str(118, 70, color_o, BLACK, (uint8_t*)str_buffer, 12, 0);

    // ��ʾС����
    Show_Str(124, 70, WHITE, BLACK, (uint8_t*)".", 12, 0);

    // ��ʾС��λ����ѡ��ָʾ��
    uint16_t color_d = (g_interface_manager.alarm_selected_item == 4) ? GREEN : WHITE;
    sprintf(str_buffer, "%d", decimal);
    Show_Str(128, 70, color_d, BLACK, (uint8_t*)str_buffer, 12, 0);

    Show_Str(10, 85, YELLOW, BLACK, (uint8_t*)"Buzzer when exceed", 12, 0); //���޷�������ʾ

    Show_Str(5, 100, GRAY, BLACK, (uint8_t*)"DOWN:Switch OK:Edit", 12, 0); //������ʾ
    Show_Str(10, 115, GRAY, BLACK, (uint8_t*)"UP:Back", 12, 0); //������ʾ
}

/**
 * @brief �������ý�����ʾ
 */
void Interface_DisplayBrightness(void)
{
    char str_buffer[32];

    Show_Str(40, 5, WHITE, BLACK, (uint8_t*)"Brightness", 16, 0); //��ʾ���ȱ���
    LCD_DrawLine(0, 25, LCD_W - 1, 25); //����ָ���

    Show_Str(10, 32, CYAN, BLACK, (uint8_t*)"Backlight:", 16, 0); //�������ȱ�ǩ

    // ��ʾ��ǰ����
    sprintf(str_buffer, "Current: %02d0%%", g_interface_manager.brightness_level);
    Show_Str(10, 55, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //��ǰ���Ȱٷֱ�

    // ����������
    LCD_DrawRectangle(10, 75, 139, 85); //�������߿�

    // ������������ڲ�����
    //LCD_Fill(11, 76, 139, 84, BLACK); //�������������(��ע��)

    // ���㲢���Ƶ�ǰ������
    uint16_t bar_width = g_interface_manager.brightness_level * 128 / 10;  // 128���ؿ�� (139-11)
    if (bar_width > 0) {
        LCD_Fill(11, 76, 11 + bar_width - 1, 84, GREEN);  // ����������
    }

    Show_Str(10, 95, YELLOW, BLACK, (uint8_t*)"DN:Add OK:Confirm", 12, 0); //������ʾ
    Show_Str(10, 115, YELLOW, BLACK, (uint8_t*)"UP:Back", 12, 0); //������ʾ
    
}

/**
 * @brief ���ڽ�����ʾ
 */
void Interface_DisplayAbout(void)
{
    Show_Str(60, 5, WHITE, BLACK, (uint8_t*)"About", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(0, 35, CYAN, BLACK, (uint8_t*)"RF Power Meter V1.0", 16, 0);//�汾
    Show_Str(10, 55, WHITE, BLACK, (uint8_t*)"Freq: 1Hz-100MHz", 12, 0);//Ƶ��
    Show_Str(10, 70, WHITE, BLACK, (uint8_t*)"Power: 0W-2kW", 12, 0);//���ʷ�Χ����
    Show_Str(10, 85, WHITE, BLACK, (uint8_t*)"VSWR: 1.0-999.0", 12, 0);//פ����
    Show_Str(10, 100, WHITE, BLACK, (uint8_t*)"Author: XUN YU TEK", 12, 0);//����

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back", 12, 0);
}

/**
 * @brief ϵͳ�������н���
 */
void System_BootSequence(void)
{
    //uint8_t progress = 0;
    uint8_t i = 0;
    char progress_str[8] = "";

    // ��ʾ��������
    LCD_Clear(BLACK); //����
    Show_Str(20, 10, GREEN, BLACK, (uint8_t*)"RF Power Meter", 16, 0); //��Ƶ���ʼƱ���
    Show_Str(30, 30, WHITE, BLACK, (uint8_t*)"System Boot", 16, 0); //ϵͳ������ʾ

    Show_Str(20, 65, CYAN, BLACK, (uint8_t*)"Initializing...", 12, 0); //��ʼ����ʾ

    // ��ʼ������ʾ (0-10%)
    for (i = 0; i <= 10; i++) {
        sprintf(progress_str, "%3d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);  // �ƶ���ԭ������λ��
        HAL_Delay(50);
    }

    // ����1��Ƶ�ʼƳ�ʼ��
    Show_Str(20, 80, YELLOW, BLACK, (uint8_t*)"FreqCounter Init", 12, 0); //Ƶ�ʼƳ�ʼ����ʾ

    // ������ʾ (10-30%)
    for (i = 10; i <= 30; i++) {
        sprintf(progress_str, "%3d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0); //���Ȱٷֱ���ʾ
        HAL_Delay(30);
    }

    if (FreqCounter_Init() != 0) {
        LCD_Clear(BLACK); //������ʾ����
        Show_Str(10, 50, RED, BLACK, (uint8_t*)"FreqCounter Init FAIL!", 16, 0); //Ƶ�ʼƳ�ʼ��ʧ��
        Show_Str(30, 70, WHITE, BLACK, (uint8_t*)"System Halted", 16, 0); //ϵͳֹͣ��ʾ
        while(1) {
            HAL_Delay(1000);  // ֹͣ���У���ι����ϵͳ��λ
        }
    }
    Show_Str(120, 80, GREEN, BLACK, (uint8_t*)"OK", 12, 0); //����1��ɱ��
    HAL_IWDG_Refresh(&hiwdg);  // ����1��ɺ�ι��

    // ����2��Ƶ�ʼ�����
    Show_Str(20, 95, YELLOW, BLACK, (uint8_t*)"FreqCounter Start", 12, 0);

    // ������ʾ (30-50%)
    for (i = 30; i <= 50; i++) {
        sprintf(progress_str, "%3d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);
        HAL_Delay(30);
    }

    if (FreqCounter_Start() != 0) {
        LCD_Clear(BLACK);
        Show_Str(10, 50, RED, BLACK, (uint8_t*)"FreqCounter Start FAIL!", 16, 0);
        Show_Str(30, 70, WHITE, BLACK, (uint8_t*)"System Halted", 16, 0);
        while(1) {
            HAL_Delay(1000);  // ֹͣ���У���ι����ϵͳ��λ
        }
    }
    Show_Str(135, 95, GREEN, BLACK, (uint8_t*)"OK", 12, 0); // �޸��������ƶ������ص�
    HAL_IWDG_Refresh(&hiwdg);  // ����2��ɺ�ι��

    // ����3�������������ʼ��
    Show_Str(20, 110, YELLOW, BLACK, (uint8_t*)"Interface Init", 12, 0);

    // ������ʾ (50-70%)
    for (i = 50; i <= 70; i++) {
        sprintf(progress_str, "%3d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);
        HAL_Delay(30);
    }

    if (InterfaceManager_Init() != 0) {
        LCD_Clear(BLACK);
        Show_Str(10, 50, RED, BLACK, (uint8_t*)"Interface Init FAIL!", 16, 0);
        Show_Str(30, 70, WHITE, BLACK, (uint8_t*)"System Halted", 16, 0);
        while(1) {
            HAL_Delay(1000);  // ֹͣ���У���ι����ϵͳ��λ
        }
    }
    Show_Str(135, 110, GREEN, BLACK, (uint8_t*)"OK", 12, 0); // �޸��������ƶ������ص�
    HAL_IWDG_Refresh(&hiwdg);  // ����3��ɺ�ι��
    HAL_Delay(300);

    // ����4��PWM���� (ֻ����°�������������ͽ�����ʾ)
    const uint16_t y_boot = 47;      // "System Boot"��y����
    const uint16_t font_h = 16;      // �ֺŸ߶�
    const uint16_t margin = 3;      // ����߾࣬ȷ����������������
    uint16_t y_clear_start = y_boot + font_h + margin;
    LCD_Fill(0, y_clear_start, lcddev.width - 1, lcddev.height - 1, BLACK);
    Show_Str(20, 10, GREEN, BLACK, (uint8_t*)"RF Power Meter", 16, 0);
    Show_Str(30, 30, WHITE, BLACK, (uint8_t*)"System Boot", 16, 0);

    // �ָ�֮ǰ�Ľ��� (70%)
    sprintf(progress_str, "%3d%%", 70);
    Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);

    Show_Str(20, 80, YELLOW, BLACK, (uint8_t*)"PWM Backlight", 12, 0);

    // ������ʾ (70-85%)
    for (i = 70; i <= 85; i++) {
        sprintf(progress_str, "%3d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);
        HAL_Delay(30);
    }

    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
    Show_Str(135, 80, GREEN, BLACK, (uint8_t*)"OK", 12, 0); // �޸��������ƶ������ص�

    // ����5��ϵͳ����
    Show_Str(20, 95, YELLOW, BLACK, (uint8_t*)"System Ready", 12, 0);

    // ������ʾ (85-100%)
    for (i = 85; i <= 100; i++) {
        sprintf(progress_str, "%3d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);
        HAL_Delay(25);
    }

    Show_Str(135, 95, GREEN, BLACK, (uint8_t*)"OK", 12, 0); // �޸��������ƶ������ص�
    HAL_IWDG_Refresh(&hiwdg);  // ����������ɺ����һ��ι��

    // ��ʾ�����Ϣ
    Show_Str(35, 110, WHITE, BLACK, (uint8_t*)"Boot Complete!", 16, 0);
    HAL_Delay(1000);

    // ����׼������������
    LCD_Clear(BLACK);
}
