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
    .last_update_time = 0,
    .need_refresh = 1
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
    .k_forward = 10.0f,         // Ĭ��10W/V
    .k_reflected = 10.0f,       // Ĭ��10W/V
    .band_gain_fwd = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
    .band_gain_ref = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f},
    .freq_trim = 1.0f,
    .is_calibrated = 0
};

/* У׼״̬ȫ�ֱ��� */
CalibrationState_t g_calibration_state = {
    .current_step = CAL_STEP_CONFIRM,
    .current_band = 0,
    .sample_count = 0,
    .sample_sum_fwd = 0.0f,
    .sample_sum_ref = 0.0f,
    .is_stable = 0,
    .stable_count = 0,
    .ref_power = 10.0f,         // Ĭ�ϲο�����10W
    .ref_power_index = 2        // ��Ӧ10W��λ
};

/* Ƶ�ζ��壨11��HF����Ƶ�Σ�*/
static const char* band_names[11] = {
    "160m", "80m", "60m", "40m", "30m", "20m", "17m", "15m", "12m", "10m", "6m"
};

static const float band_frequencies[11] = {
    1.8f, 3.5f, 5.3f, 7.0f, 10.1f, 14.0f, 18.1f, 21.0f, 24.9f, 28.0f, 50.0f  // MHz
};

/* �ο����ʵ�λ */
static const float ref_power_levels[5] = {1.0f, 5.0f, 10.0f, 50.0f, 100.0f};  // W

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
        saved_vswr_threshold >= 1.5f && saved_vswr_threshold <= 5.0f) {
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
            case INTERFACE_CAL_REFLECT:
                Interface_DisplayCalReflect();
                break;
            case INTERFACE_CAL_FREQ:
                Interface_DisplayCalFreq();
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
        g_interface_manager.current_interface <= INTERFACE_CAL_FREQ) {
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
                InterfaceIndex_t target = (InterfaceIndex_t)(INTERFACE_CALIBRATION + g_interface_manager.menu_cursor);
                InterfaceManager_SwitchTo(target);
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
                // �л�ѡ����������� ? VSWR��ֵ��
                g_interface_manager.alarm_selected_item =
                    (g_interface_manager.alarm_selected_item + 1) % 2;
                g_interface_manager.need_refresh = 1;
            } else if (key == KEY_OK) {
                // ���ڵ�ǰѡ�е���Ŀ
                if (g_interface_manager.alarm_selected_item == 0) {
                    // �л���������
                    g_interface_manager.alarm_enabled = !g_interface_manager.alarm_enabled;
                } else {
                    // ����VSWR��ֵ��ѭ����1.5 �� 2.0 �� 2.5 �� 3.0 �� 3.5 �� 4.0 �� 5.0 �� 1.5��
                    if (g_interface_manager.vswr_alarm_threshold >= 5.0f) {
                        g_interface_manager.vswr_alarm_threshold = 1.5f;
                    } else if (g_interface_manager.vswr_alarm_threshold >= 4.0f) {
                        g_interface_manager.vswr_alarm_threshold = 5.0f;
                    } else if (g_interface_manager.vswr_alarm_threshold >= 3.5f) {
                        g_interface_manager.vswr_alarm_threshold = 4.0f;
                    } else if (g_interface_manager.vswr_alarm_threshold >= 3.0f) {
                        g_interface_manager.vswr_alarm_threshold = 3.5f;
                    } else if (g_interface_manager.vswr_alarm_threshold >= 2.5f) {
                        g_interface_manager.vswr_alarm_threshold = 3.0f;
                    } else if (g_interface_manager.vswr_alarm_threshold >= 2.0f) {
                        g_interface_manager.vswr_alarm_threshold = 2.5f;
                    } else if (g_interface_manager.vswr_alarm_threshold >= 1.5f) {
                        g_interface_manager.vswr_alarm_threshold = 2.0f;
                    } else {
                        g_interface_manager.vswr_alarm_threshold = 1.5f;
                    }
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
                // ��ʼУ׼��
                Calibration_StartStep(CAL_STEP_CONFIRM);
                InterfaceManager_SwitchTo(INTERFACE_CAL_CONFIRM);
            }
            break;

        case INTERFACE_CAL_CONFIRM:
            // У׼ȷ��ҳ��������
            if (key == KEY_UP) {
                InterfaceManager_SwitchTo(INTERFACE_CALIBRATION);
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
    // �Զ�ѡ���ʵ�λ
    if (forward_power >= 1000.0f) {
        g_power_result.forward_power = forward_power / 1000.0f;
        g_power_result.forward_unit = POWER_UNIT_KW;
    } else {
        g_power_result.forward_power = forward_power;
        g_power_result.forward_unit = POWER_UNIT_W;
    }
    
    if (reflected_power >= 1000.0f) {
        g_power_result.reflected_power = reflected_power / 1000.0f;
        g_power_result.reflected_unit = POWER_UNIT_KW;
    } else {
        g_power_result.reflected_power = reflected_power;
        g_power_result.reflected_unit = POWER_UNIT_W;
    }
    
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
    if (vswr <= 1.5f) {
        g_rf_params.vswr_color = VSWR_COLOR_GREEN;
    } else if (vswr <= 2.0f) {
        g_rf_params.vswr_color = VSWR_COLOR_YELLOW;
    } else {
        g_rf_params.vswr_color = VSWR_COLOR_RED;
    }
    
    g_rf_params.is_valid = 1;
    
    // ����Ƿ���Ҫ����
    if (g_interface_manager.alarm_enabled && vswr > g_interface_manager.vswr_alarm_threshold) {
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

    g_interface_manager.brightness_level = level;

    // ����PWMռ�ձ� (10%-100%)
    uint16_t duty = 99 + (level * 90);

    // ����TIM3 CH3��PWMռ�ձ� (PB0�������)
    LCD_SetBacklight(duty);
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
 */
void InterfaceManager_BuzzerProcess(void)
{
    if (g_buzzer_state.is_active) {
        if (g_buzzer_state.duration_count > 0) {
            g_buzzer_state.duration_count--;
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
    g_calibration_state.current_band = 0;
    g_calibration_state.sample_count = 0;
    g_calibration_state.sample_sum_fwd = 0.0f;
    g_calibration_state.sample_sum_ref = 0.0f;
    g_calibration_state.is_stable = 0;
    g_calibration_state.stable_count = 0;
    g_calibration_state.ref_power = 10.0f;
    g_calibration_state.ref_power_index = 2;
}

/**
 * @brief ��EEPROM����У׼����
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

    // ��ȡ����ϵ��
    if (BL24C16_Read(0x0020, (uint8_t*)&g_calibration_data.k_forward, sizeof(float)) != EEPROM_OK) {
        g_calibration_data.k_forward = 10.0f;
    }
    if (BL24C16_Read(0x0024, (uint8_t*)&g_calibration_data.k_reflected, sizeof(float)) != EEPROM_OK) {
        g_calibration_data.k_reflected = 10.0f;
    }

    // ��ȡƵ����������
    for (int i = 0; i < 11; i++) {
        if (BL24C16_Read(0x0100 + i * 4, (uint8_t*)&g_calibration_data.band_gain_fwd[i], sizeof(float)) != EEPROM_OK) {
            g_calibration_data.band_gain_fwd[i] = 1.0f;
        }
        if (BL24C16_Read(0x0140 + i * 4, (uint8_t*)&g_calibration_data.band_gain_ref[i], sizeof(float)) != EEPROM_OK) {
            g_calibration_data.band_gain_ref[i] = 1.0f;
        }
    }

    // ��ȡƵ��΢��
    if (BL24C16_Read(0x0200, (uint8_t*)&g_calibration_data.freq_trim, sizeof(float)) != EEPROM_OK) {
        g_calibration_data.freq_trim = 1.0f;
    }

    // ��ȡУ׼��ɱ�־
    if (BL24C16_Read(0x0204, &g_calibration_data.is_calibrated, 1) != EEPROM_OK) {
        g_calibration_data.is_calibrated = 0;
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

    // �������ϵ��
    BL24C16_Write(0x0020, (uint8_t*)&g_calibration_data.k_forward, sizeof(float));
    BL24C16_Write(0x0024, (uint8_t*)&g_calibration_data.k_reflected, sizeof(float));

    // ����Ƶ����������
    for (int i = 0; i < 11; i++) {
        BL24C16_Write(0x0100 + i * 4, (uint8_t*)&g_calibration_data.band_gain_fwd[i], sizeof(float));
        BL24C16_Write(0x0140 + i * 4, (uint8_t*)&g_calibration_data.band_gain_ref[i], sizeof(float));
    }

    // ����Ƶ��΢��
    BL24C16_Write(0x0200, (uint8_t*)&g_calibration_data.freq_trim, sizeof(float));

    // ����У׼��ɱ�־
    g_calibration_data.is_calibrated = 1;
    BL24C16_Write(0x0204, &g_calibration_data.is_calibrated, 1);
}

/**
 * @brief ��ȡƵ������
 */
uint8_t Calibration_GetBandIndex(float frequency)
{
    // ����Ƶ�ʷ��ض�Ӧ��Ƶ������(0-10)
    if (frequency < 2.5f) return 0;        // 160m (1.8MHz)
    else if (frequency < 4.5f) return 1;   // 80m (3.5MHz)
    else if (frequency < 6.0f) return 2;   // 60m (5.3MHz)
    else if (frequency < 8.5f) return 3;   // 40m (7.0MHz)
    else if (frequency < 12.0f) return 4;  // 30m (10.1MHz)
    else if (frequency < 16.0f) return 5;  // 20m (14.0MHz)
    else if (frequency < 19.5f) return 6;  // 17m (18.1MHz)
    else if (frequency < 23.0f) return 7;  // 15m (21.0MHz)
    else if (frequency < 26.5f) return 8;  // 12m (24.9MHz)
    else if (frequency < 35.0f) return 9;  // 10m (28.0MHz)
    else return 10;                        // 6m (50.0MHz)
}

/**
 * @brief Ӧ��У׼����
 */
float Calibration_ApplyCorrection(float raw_power, uint8_t is_forward, float frequency)
{
    if (!g_calibration_data.is_calibrated) {
        return raw_power;  // δУ׼ʱֱ�ӷ���ԭֵ
    }

    uint8_t band_index = Calibration_GetBandIndex(frequency);
    float corrected_power;

    if (is_forward) {
        corrected_power = g_calibration_data.k_forward *
                         g_calibration_data.band_gain_fwd[band_index] *
                         (raw_power - g_calibration_data.forward_offset);
    } else {
        corrected_power = g_calibration_data.k_reflected *
                         g_calibration_data.band_gain_ref[band_index] *
                         (raw_power - g_calibration_data.reflected_offset);
    }

    return (corrected_power > 0.0f) ? corrected_power : 0.0f;
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
    g_calibration_state.is_stable = 0;
    g_calibration_state.stable_count = 0;
    g_interface_manager.need_refresh = 1;
}

/**
 * @brief ��ȡADC��ѹֵ
 */
static void Calibration_ReadADCVoltage(float* fwd_voltage, float* ref_voltage)
{
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
    *fwd_voltage = (float)adc_forward * 3.3f / 4095.0f;
    *ref_voltage = (float)adc_reflected * 3.3f / 4095.0f;
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

        // ������ɣ�32�Σ�
        if (g_calibration_state.sample_count >= 32) {
            float avg_fwd = g_calibration_state.sample_sum_fwd / 32.0f;
            float avg_ref = g_calibration_state.sample_sum_ref / 32.0f;

            // ���ݵ�ǰ���账��������
            switch (g_calibration_state.current_step) {
                case CAL_STEP_ZERO:
                    g_calibration_data.forward_offset = avg_fwd;
                    g_calibration_data.reflected_offset = avg_ref;
                    InterfaceManager_Beep(200);  // �ɹ���ʾ��
                    break;

                case CAL_STEP_POWER:
                    g_calibration_data.k_forward = g_calibration_state.ref_power /
                                                  (avg_fwd - g_calibration_data.forward_offset);
                    InterfaceManager_Beep(200);
                    break;

                case CAL_STEP_BAND:
                    {
                        uint8_t band = g_calibration_state.current_band;
                        float ref_fwd = g_calibration_data.k_forward *
                                       (avg_fwd - g_calibration_data.forward_offset);
                        float ref_ref = g_calibration_data.k_reflected *
                                       (avg_ref - g_calibration_data.reflected_offset);

                        g_calibration_data.band_gain_fwd[band] = g_calibration_state.ref_power / ref_fwd;
                        g_calibration_data.band_gain_ref[band] = g_calibration_state.ref_power / ref_ref;
                        InterfaceManager_Beep(200);
                    }
                    break;

                case CAL_STEP_REFLECT:
                    {
                        float measured_power = g_calibration_data.k_forward *
                                             g_calibration_data.band_gain_fwd[5] *  // ʹ��20mƵ��
                                             (avg_fwd - g_calibration_data.forward_offset);
                        float measured_ref = avg_ref - g_calibration_data.reflected_offset;

                        // ����ʹ��25�����أ�����VSWR=2.0��
                        float target_ref_power = measured_power / 9.0f;  // VSWR=2.0ʱ�ķ��书��
                        g_calibration_data.k_reflected = target_ref_power / measured_ref;
                        InterfaceManager_Beep(200);
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
    Show_Str(35, 5, WHITE, BLACK, (uint8_t*)"Zero Cal", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(10, 30, CYAN, BLACK, (uint8_t*)"Step 1/5:", 12, 0);
    Show_Str(10, 45, WHITE, BLACK, (uint8_t*)"Turn OFF RF", 16, 0);
    Show_Str(10, 65, WHITE, BLACK, (uint8_t*)"No signal input", 12, 0);

    if (g_calibration_state.sample_count > 0) {
        Show_Str(10, 80, YELLOW, BLACK, (uint8_t*)"Sampling...", 12, 0);
        char progress[16];
        sprintf(progress, "%d/32 ", g_calibration_state.sample_count);
        Show_Str(100, 80, YELLOW, BLACK, (uint8_t*)progress, 12, 0);
    } else {
        Show_Str(10, 80, GREEN, BLACK, (uint8_t*)"Ready to sample", 12, 0);
    }

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"OK:Sample UP:Back", 12, 0);
}

/**
 * @brief ��ʾ����б��У׼
 */
void Interface_DisplayCalPower(void)
{
    char str_buffer[32];

    Show_Str(30, 5, WHITE, BLACK, (uint8_t*)"Power Cal", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(10, 30, CYAN, BLACK, (uint8_t*)"Step 2/5:", 12, 0);
    Show_Str(10, 45, WHITE, BLACK, (uint8_t*)"Connect 50ohm", 12, 0);

    // ��ʾ�ο�����ѡ��
    sprintf(str_buffer, "Ref Power: %.0fW", ref_power_levels[g_calibration_state.ref_power_index]);
    Show_Str(10, 60, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    if (g_calibration_state.is_stable) {
        Show_Str(10, 75, GREEN, BLACK, (uint8_t*)"STABLE - Ready", 12, 0);
    } else {
        Show_Str(10, 75, YELLOW, BLACK, (uint8_t*)"Waiting stable..", 12, 0);
    }

    if (g_calibration_state.sample_count > 0) {
        Show_Str(10, 90, YELLOW, BLACK, (uint8_t*)"Sampling...", 12, 0);
    }

    Show_Str(5, 105, GRAY, BLACK, (uint8_t*)"DOWN:Power OK:Cal", 12, 0);
    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back", 12, 0);
}

/**
 * @brief ��ʾƵ����������
 */
void Interface_DisplayCalBand(void)
{
    char str_buffer[32];

    Show_Str(35, 5, WHITE, BLACK, (uint8_t*)"Band Cal", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(10, 30, CYAN, BLACK, (uint8_t*)"Step 3/5:", 12, 0);

    // ��ʾ��ǰƵ��
    sprintf(str_buffer, "Band: %s (%.1fMHz)",
            band_names[g_calibration_state.current_band],
            band_frequencies[g_calibration_state.current_band]);
    Show_Str(10, 45, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    // ��ʾ����
    sprintf(str_buffer, "Progress: %d/11 ", g_calibration_state.current_band + 1);
    Show_Str(10, 60, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    if (g_calibration_state.is_stable) {
        Show_Str(10, 75, GREEN, BLACK, (uint8_t*)"STABLE - Ready", 12, 0);
    } else {
        Show_Str(10, 75, YELLOW, BLACK, (uint8_t*)"Set freq & power", 12, 0);
    }

    if (g_calibration_state.sample_count > 0) {
        Show_Str(10, 90, YELLOW, BLACK, (uint8_t*)"Sampling...", 12, 0);
    }

    Show_Str(5, 105, GRAY, BLACK, (uint8_t*)"DOWN:Next OK:Cal", 12, 0);
    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back", 12, 0);
}

/**
 * @brief ��ʾ����ͨ������
 */
void Interface_DisplayCalReflect(void)
{
    Show_Str(25, 5, WHITE, BLACK, (uint8_t*)"Reflect Cal", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(10, 30, CYAN, BLACK, (uint8_t*)"Step 4/5:", 12, 0);
    Show_Str(10, 45, WHITE, BLACK, (uint8_t*)"Connect 25ohm", 12, 0);
    Show_Str(10, 60, WHITE, BLACK, (uint8_t*)"(SWR=2.0 std)", 12, 0);

    if (g_calibration_state.is_stable) {
        Show_Str(10, 75, GREEN, BLACK, (uint8_t*)"STABLE - Ready", 12, 0);
    } else {
        Show_Str(10, 75, YELLOW, BLACK, (uint8_t*)"Waiting stable..", 12, 0);
    }

    if (g_calibration_state.sample_count > 0) {
        Show_Str(10, 90, YELLOW, BLACK, (uint8_t*)"Sampling...", 12, 0);
    }

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"OK:Cal UP:Back", 12, 0);
}

/**
 * @brief ��ʾƵ�ʼ�΢��
 */
void Interface_DisplayCalFreq(void)
{
    char str_buffer[32];

    Show_Str(30, 5, WHITE, BLACK, (uint8_t*)"Freq Cal", 16, 0);
    LCD_DrawLine(0, 25, LCD_W - 1, 25);

    Show_Str(10, 30, CYAN, BLACK, (uint8_t*)"Step 5/5:", 12, 0);
    Show_Str(10, 45, WHITE, BLACK, (uint8_t*)"Input 10.000MHz", 12, 0);

    // ��ʾ��ǰƵ�ʶ���
    FreqResult_t freq_result;
    if (FreqCounter_GetResult(&freq_result) == 0) {
        sprintf(str_buffer, "Read: %.3fMHz", freq_result.frequency_display);
        Show_Str(10, 60, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);
    } else {
        Show_Str(10, 60, GRAY, BLACK, (uint8_t*)"Read: -- MHz", 12, 0);
    }

    // ��ʾ΢��ϵ��
    sprintf(str_buffer, "Trim: %.6f", g_calibration_data.freq_trim);
    Show_Str(10, 75, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    Show_Str(5, 105, GRAY, BLACK, (uint8_t*)"DOWN:Adj OK:Save", 12, 0);
    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back", 12, 0);
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
        sprintf(str_buffer, "%.2f %s", g_power_result.forward_power,
                g_power_result.forward_unit == POWER_UNIT_KW ? "kW" : "W");
        Show_Str(5, 45, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //��������ֵ
    } else {
        Show_Str(5, 45, GRAY, BLACK, (uint8_t*)"-- W", 12, 0); //��������Ч��ʾ
    }

    Show_Str(5, 60, CYAN, BLACK, (uint8_t*)"Reflect:", 12, 0); //���书�ʱ�ǩ
    if (g_power_result.is_valid) {
        sprintf(str_buffer, "%.2f %s", g_power_result.reflected_power,
                g_power_result.reflected_unit == POWER_UNIT_KW ? "kW" : "W");
        Show_Str(5, 75, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //���书����ֵ
    } else {
        Show_Str(5, 75, GRAY, BLACK, (uint8_t*)"-- W", 12, 0); //���书����Ч��ʾ
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
        sprintf(str_buffer, "%.2f", g_rf_params.vswr);
        Show_Str(85, 45, vswr_color, BLACK, (uint8_t*)str_buffer, 12, 0); //פ������ֵ(����ɫ)
    } else {
        Show_Str(85, 45, GRAY, BLACK, (uint8_t*)"--", 12, 0); //פ������Ч��ʾ
    }

    Show_Str(85, 60, CYAN, BLACK, (uint8_t*)"Refl.Coef:", 12, 0); //����ϵ����ǩ
    if (g_rf_params.is_valid) {
        sprintf(str_buffer, "%.3f", g_rf_params.reflection_coeff);
        Show_Str(85, 75, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //����ϵ����ֵ
    } else {
        Show_Str(85, 75, GRAY, BLACK, (uint8_t*)"--", 12, 0); //����ϵ����Ч��ʾ
    }

    // �ײ���ʾ����Ч�ʺ�Ƶ��
    LCD_DrawLine(0, 95, 160, 95); //�ײ��ָ���

    Show_Str(5, 100, CYAN, BLACK, (uint8_t*)"Efficiency:", 12, 0); //����Ч�ʱ�ǩ
    if (g_rf_params.is_valid) {
        sprintf(str_buffer, "%.1f%%", g_rf_params.transmission_eff);
        Show_Str(70, 100, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //����Ч����ֵ
    } else {
        Show_Str(70, 100, GRAY, BLACK, (uint8_t*)"--%", 12, 0); //����Ч����Ч��ʾ
    }

    Show_Str(5, 115, CYAN, BLACK, (uint8_t*)"Freq:", 12, 0); //Ƶ�ʱ�ǩ
    FreqResult_t freq_result;
    if (FreqCounter_GetResult(&freq_result) == 0) {
        sprintf(str_buffer, "%.2f %s", freq_result.frequency_display,
                FreqCounter_GetUnitString(freq_result.unit));
        Show_Str(40, 115, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //Ƶ����ֵ
    } else {
        Show_Str(40, 115, GRAY, BLACK, (uint8_t*)"-- Hz", 12, 0); //Ƶ����Ч��ʾ
    }

    // ��ʾ������ʾ
    Show_Str(110, 115, GRAY, BLACK, (uint8_t*)"OK:Menu", 12, 0); //������ʾ
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
    LCD_DrawLine(0, 25, 160, 25); //����ָ���

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
 * @brief �궨������ʾ
 */
void Interface_DisplayStandard(void)
{
    Show_Str(50, 5, WHITE, BLACK, (uint8_t*)"Settings", 16, 0); //�궨���ñ���
    LCD_DrawLine(0, 25, 160, 25); //����ָ���

    Show_Str(10, 35, CYAN, BLACK, (uint8_t*)"Power Cal:", 16, 0); //����У׼��ǩ
    Show_Str(10, 55, WHITE, BLACK, (uint8_t*)"Forward Slope: 1.00", 12, 0); //������б��
    Show_Str(10, 70, WHITE, BLACK, (uint8_t*)"Reflect Slope: 1.00", 12, 0); //���书��б��
    Show_Str(10, 85, WHITE, BLACK, (uint8_t*)"Zero Offset: 0.00V", 12, 0); //���ƫ��

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back", 12, 0); //������ʾ
}

/**
 * @brief �������ý�����ʾ
 */
void Interface_DisplayAlarm(void)
{
    char str_buffer[32];

    Show_Str(40, 5, WHITE, BLACK, (uint8_t*)"Alarm Limit", 16, 0); //���ޱ�������
    LCD_DrawLine(0, 25, 160, 25); //����ָ���

    Show_Str(10, 35, CYAN, BLACK, (uint8_t*)"Alarm Setup:", 16, 0); //�������ñ�ǩ

    // ��ʾ����ʹ��״̬����ѡ��ָʾ��
    if (g_interface_manager.alarm_selected_item == 0) {
        Show_Str(5, 55, GREEN, BLACK, (uint8_t*)">", 12, 0); //ѡ��ָʾ��
    } else {
        Show_Str(5, 55, BLACK, BLACK, (uint8_t*)" ", 12, 0); //���ָʾ��
    }
    sprintf(str_buffer, "Alarm Enable: %s", g_interface_manager.alarm_enabled ? "ON " : "OFF");
    Show_Str(15, 55, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //����ʹ��״̬

    // ��ʾVSWR��ֵ����ѡ��ָʾ��
    if (g_interface_manager.alarm_selected_item == 1) {
        Show_Str(5, 70, GREEN, BLACK, (uint8_t*)">", 12, 0); //ѡ��ָʾ��
    } else {
        Show_Str(5, 70, BLACK, BLACK, (uint8_t*)" ", 12, 0); //���ָʾ��
    }
    sprintf(str_buffer, "VSWR Limit: %.1f", g_interface_manager.vswr_alarm_threshold);
    Show_Str(15, 70, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //פ���ȱ�����ֵ

    Show_Str(10, 85, YELLOW, BLACK, (uint8_t*)"Buzzer when exceed", 12, 0); //���޷�������ʾ

    Show_Str(5, 100, GRAY, BLACK, (uint8_t*)"DOWN:Switch OK:Edit", 12, 0); //������ʾ
    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back", 12, 0); //������ʾ
}

/**
 * @brief �������ý�����ʾ
 */
void Interface_DisplayBrightness(void)
{
    char str_buffer[32];

    Show_Str(40, 5, WHITE, BLACK, (uint8_t*)"Brightness", 16, 0); //��ʾ���ȱ���
    LCD_DrawLine(0, 25, 160, 25); //����ָ���

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
    Show_Str(5, 115, YELLOW, BLACK, (uint8_t*)"UP:Back", 12, 0); //������ʾ
    
}

/**
 * @brief ���ڽ�����ʾ
 */
void Interface_DisplayAbout(void)
{
    Show_Str(60, 5, WHITE, BLACK, (uint8_t*)"About", 16, 0);
    LCD_DrawLine(0, 25, 160, 25);

    Show_Str(0, 35, CYAN, BLACK, (uint8_t*)"RF Power Meter V1.0", 16, 0);//�汾
    Show_Str(10, 55, WHITE, BLACK, (uint8_t*)"Freq: 1Hz-100MHz", 12, 0);//Ƶ��
    Show_Str(10, 70, WHITE, BLACK, (uint8_t*)"Power: 0.1W-1kW", 12, 0);//����
    Show_Str(10, 85, WHITE, BLACK, (uint8_t*)"VSWR: 1.0-10.0", 12, 0);//פ����
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
        sprintf(progress_str, "%d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);  // �ƶ���ԭ������λ��
        HAL_Delay(50);
    }

    // ����1��Ƶ�ʼƳ�ʼ��
    Show_Str(20, 80, YELLOW, BLACK, (uint8_t*)"FreqCounter Init", 12, 0); //Ƶ�ʼƳ�ʼ����ʾ

    // ������ʾ (10-30%)
    for (i = 10; i <= 30; i++) {
        sprintf(progress_str, "%d%%", i);
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
    Show_Str(130, 80, GREEN, BLACK, (uint8_t*)"OK", 12, 0); //����1��ɱ��
    HAL_IWDG_Refresh(&hiwdg);  // ����1��ɺ�ι��

    // ����2��Ƶ�ʼ�����
    Show_Str(20, 95, YELLOW, BLACK, (uint8_t*)"FreqCounter Start", 12, 0);

    // ������ʾ (30-50%)
    for (i = 30; i <= 50; i++) {
        sprintf(progress_str, "%d%%", i);
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
    Show_Str(130, 95, GREEN, BLACK, (uint8_t*)"OK", 12, 0);
    HAL_IWDG_Refresh(&hiwdg);  // ����2��ɺ�ι��

    // ����3�������������ʼ��
    Show_Str(20, 110, YELLOW, BLACK, (uint8_t*)"Interface Init", 12, 0);

    // ������ʾ (50-70%)
    for (i = 50; i <= 70; i++) {
        sprintf(progress_str, "%d%%", i);
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
    Show_Str(130, 110, GREEN, BLACK, (uint8_t*)"OK", 12, 0);
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
    sprintf(progress_str, "%d%%", 70);
    Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);

    Show_Str(20, 80, YELLOW, BLACK, (uint8_t*)"PWM Backlight", 12, 0);

    // ������ʾ (70-85%)
    for (i = 70; i <= 85; i++) {
        sprintf(progress_str, "%d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);
        HAL_Delay(30);
    }

    HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
    Show_Str(130, 80, GREEN, BLACK, (uint8_t*)"OK", 12, 0);

    // ����5��ϵͳ����
    Show_Str(20, 95, YELLOW, BLACK, (uint8_t*)"System Ready", 12, 0);

    // ������ʾ (85-100%)
    for (i = 85; i <= 100; i++) {
        sprintf(progress_str, "%d%%", i);
        Show_Str(55, 47, WHITE, BLACK, (uint8_t*)progress_str, 16, 0);
        HAL_Delay(25);
    }

    Show_Str(130, 95, GREEN, BLACK, (uint8_t*)"OK", 12, 0);
    HAL_IWDG_Refresh(&hiwdg);  // ����������ɺ����һ��ι��

    // ��ʾ�����Ϣ
    Show_Str(35, 110, WHITE, BLACK, (uint8_t*)"Boot Complete!", 16, 0);
    HAL_Delay(1000);

    // ����׼������������
    LCD_Clear(BLACK);
}
