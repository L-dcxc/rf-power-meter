/**
 * @file interface_manager.c
 * @brief ��Ƶ���ʼƽ������ϵͳʵ���ļ�
 * @author �������
 * @date 2025-01-10
 */

#include "interface_manager.h"
#include "gpio.h"
#include "tim.h"
#include <stdio.h>
#include <string.h>

/* ȫ�ֱ������� */
InterfaceManager_t g_interface_manager = {
    .current_interface = INTERFACE_MAIN,
    .menu_cursor = 0,
    .brightness_level = 8,
    .alarm_enabled = 1,
    .vswr_alarm_threshold = 3.0f,
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

/* �˵����� */
const MenuItem_t menu_table[MAX_MENU_ITEMS] = {
    {INTERFACE_CALIBRATION, INTERFACE_MENU, INTERFACE_STANDARD, Interface_DisplayCalibration, "У׼����"},
    {INTERFACE_STANDARD, INTERFACE_MENU, INTERFACE_ALARM, Interface_DisplayStandard, "�궨����"},
    {INTERFACE_ALARM, INTERFACE_MENU, INTERFACE_BRIGHTNESS, Interface_DisplayAlarm, "���ޱ���"},
    {INTERFACE_BRIGHTNESS, INTERFACE_MENU, INTERFACE_ABOUT, Interface_DisplayBrightness, "��ʾ����"},
    {INTERFACE_ABOUT, INTERFACE_MENU, INTERFACE_CALIBRATION, Interface_DisplayAbout, "�����豸"},
    {INTERFACE_MENU, INTERFACE_MAIN, INTERFACE_CALIBRATION, Interface_DisplayMenu, "���ò˵�"}
};

/**
 * @brief �����������ʼ��
 */
int8_t InterfaceManager_Init(void)
{
    // ��ʼ�����������״̬
    g_interface_manager.current_interface = INTERFACE_MAIN;
    g_interface_manager.menu_cursor = 0;
    g_interface_manager.brightness_level = 8;  // Ĭ��80%����
    g_interface_manager.alarm_enabled = 1;     // Ĭ��ʹ�ܱ���
    g_interface_manager.vswr_alarm_threshold = 3.0f;  // Ĭ��VSWR������ֵ
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
    static uint32_t last_key_time = 0;
    static KeyValue_t last_key = KEY_NONE;
    uint32_t current_time = HAL_GetTick();
    
    // ��������50ms�ں����ظ�����
    if (current_time - last_key_time < 50) {
        return KEY_NONE;
    }
    
    // ��ȡ����״̬ (�͵�ƽ��Ч)
    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_12) == GPIO_PIN_RESET) {
        if (last_key != KEY_UP) {
            last_key_time = current_time;
            last_key = KEY_UP;
            return KEY_UP;
        }
    } else if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_13) == GPIO_PIN_RESET) {
        if (last_key != KEY_OK) {
            last_key_time = current_time;
            last_key = KEY_OK;
            return KEY_OK;
        }
    } else if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_14) == GPIO_PIN_RESET) {
        if (last_key != KEY_DOWN) {
            last_key_time = current_time;
            last_key = KEY_DOWN;
            return KEY_DOWN;
        }
    } else {
        last_key = KEY_NONE;
    }
    
    return KEY_NONE;
}

/**
 * @brief ���ñ�������
 */
void InterfaceManager_SetBrightness(uint8_t level)
{
    if (level > 10) level = 10;

    g_interface_manager.brightness_level = level;

    // ����PWMռ�ձ� (10%-100%)
    uint16_t duty = 1000 + (level * 900 / 10);

    // ����TIM3 CH3��PWMռ�ձ� (PB0�������)
    __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, duty);
}

/**
 * @brief ����������
 */
void InterfaceManager_Beep(uint16_t duration_ms)
{
    // ���������� (PB4)
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_SET);
    HAL_Delay(duration_ms);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET);
}

/**
 * @brief ��������ʾ
 */
void Interface_DisplayMain(void)
{
    char str_buffer[32];
    uint16_t vswr_color;

    // ��ʾ����
    Show_Str(30, 5, WHITE, BLACK, (uint8_t*)"��Ƶ���ʼ�", 16, 0);

    // ���Ʒָ���
    LCD_DrawLine(0, 25, 160, 25);
    LCD_DrawLine(80, 25, 80, 128);

    // �����ʾ������Ϣ
    Show_Str(5, 30, CYAN, BLACK, (uint8_t*)"������:", 12, 0);
    if (g_power_result.is_valid) {
        sprintf(str_buffer, "%.2f %s", g_power_result.forward_power,
                g_power_result.forward_unit == POWER_UNIT_KW ? "kW" : "W");
        Show_Str(5, 45, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);
    } else {
        Show_Str(5, 45, GRAY, BLACK, (uint8_t*)"-- W", 12, 0);
    }

    Show_Str(5, 60, CYAN, BLACK, (uint8_t*)"���书��:", 12, 0);
    if (g_power_result.is_valid) {
        sprintf(str_buffer, "%.2f %s", g_power_result.reflected_power,
                g_power_result.reflected_unit == POWER_UNIT_KW ? "kW" : "W");
        Show_Str(5, 75, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);
    } else {
        Show_Str(5, 75, GRAY, BLACK, (uint8_t*)"-- W", 12, 0);
    }

    // �Ҳ���ʾ��Ƶ����
    Show_Str(85, 30, CYAN, BLACK, (uint8_t*)"פ����:", 12, 0);
    if (g_rf_params.is_valid) {
        // ����VSWRֵѡ����ɫ
        switch (g_rf_params.vswr_color) {
            case VSWR_COLOR_GREEN:  vswr_color = GREEN; break;
            case VSWR_COLOR_YELLOW: vswr_color = YELLOW; break;
            case VSWR_COLOR_RED:    vswr_color = RED; break;
            default:                vswr_color = WHITE; break;
        }
        sprintf(str_buffer, "%.2f", g_rf_params.vswr);
        Show_Str(85, 45, vswr_color, BLACK, (uint8_t*)str_buffer, 12, 0);
    } else {
        Show_Str(85, 45, GRAY, BLACK, (uint8_t*)"--", 12, 0);
    }

    Show_Str(85, 60, CYAN, BLACK, (uint8_t*)"����ϵ��:", 12, 0);
    if (g_rf_params.is_valid) {
        sprintf(str_buffer, "%.3f", g_rf_params.reflection_coeff);
        Show_Str(85, 75, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);
    } else {
        Show_Str(85, 75, GRAY, BLACK, (uint8_t*)"--", 12, 0);
    }

    // �ײ���ʾ����Ч�ʺ�Ƶ��
    LCD_DrawLine(0, 95, 160, 95);

    Show_Str(5, 100, CYAN, BLACK, (uint8_t*)"����Ч��:", 12, 0);
    if (g_rf_params.is_valid) {
        sprintf(str_buffer, "%.1f%%", g_rf_params.transmission_eff);
        Show_Str(60, 100, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);
    } else {
        Show_Str(60, 100, GRAY, BLACK, (uint8_t*)"--%", 12, 0);
    }

    Show_Str(5, 115, CYAN, BLACK, (uint8_t*)"Ƶ��:", 12, 0);
    FreqResult_t freq_result;
    if (FreqCounter_GetResult(&freq_result) == 0) {
        sprintf(str_buffer, "%.2f %s", freq_result.frequency_display,
                FreqCounter_GetUnitString(freq_result.unit));
        Show_Str(35, 115, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);
    } else {
        Show_Str(35, 115, GRAY, BLACK, (uint8_t*)"-- Hz", 12, 0);
    }

    // ��ʾ������ʾ
    Show_Str(100, 115, GRAY, BLACK, (uint8_t*)"OK:�˵�", 12, 0);
}

/**
 * @brief �˵�������ʾ
 */
void Interface_DisplayMenu(void)
{
    char str_buffer[32];
    const char* menu_items[] = {
        "У׼����",
        "�궨����",
        "���ޱ���",
        "��ʾ����",
        "�����豸"
    };

    // ��ʾ����
    Show_Str(50, 5, WHITE, BLACK, (uint8_t*)"���ò˵�", 16, 0);
    LCD_DrawLine(0, 25, 160, 25);

    // ��ʾ�˵���
    for (int i = 0; i < MAX_MENU_ITEMS - 1; i++) {
        uint16_t color = (i == g_interface_manager.menu_cursor) ? YELLOW : WHITE;
        uint16_t bg_color = (i == g_interface_manager.menu_cursor) ? BLUE : BLACK;

        sprintf(str_buffer, "> %s", menu_items[i]);
        Show_Str(10, 35 + i * 18, color, bg_color, (uint8_t*)str_buffer, 14, 0);
    }

    // ��ʾ������ʾ
    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"��:���� ��:�л� OK:ȷ��", 12, 0);
}

/**
 * @brief У׼������ʾ
 */
void Interface_DisplayCalibration(void)
{
    Show_Str(50, 5, WHITE, BLACK, (uint8_t*)"У׼����", 16, 0);
    LCD_DrawLine(0, 25, 160, 25);

    Show_Str(10, 35, CYAN, BLACK, (uint8_t*)"У׼����:", 14, 0);
    Show_Str(10, 55, WHITE, BLACK, (uint8_t*)"1. ��·У׼", 12, 0);
    Show_Str(10, 70, WHITE, BLACK, (uint8_t*)"2. ��·У׼", 12, 0);
    Show_Str(10, 85, WHITE, BLACK, (uint8_t*)"3. ����У׼", 12, 0);
    Show_Str(10, 100, WHITE, BLACK, (uint8_t*)"4. ����У׼", 12, 0);

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"��:����", 12, 0);
}

/**
 * @brief �궨������ʾ
 */
void Interface_DisplayStandard(void)
{
    Show_Str(50, 5, WHITE, BLACK, (uint8_t*)"�궨����", 16, 0);
    LCD_DrawLine(0, 25, 160, 25);

    Show_Str(10, 35, CYAN, BLACK, (uint8_t*)"���ʱ궨:", 14, 0);
    Show_Str(10, 55, WHITE, BLACK, (uint8_t*)"������б��: 1.00", 12, 0);
    Show_Str(10, 70, WHITE, BLACK, (uint8_t*)"���书��б��: 1.00", 12, 0);
    Show_Str(10, 85, WHITE, BLACK, (uint8_t*)"���ƫ��: 0.00V", 12, 0);

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"��:����", 12, 0);
}

/**
 * @brief �������ý�����ʾ
 */
void Interface_DisplayAlarm(void)
{
    char str_buffer[32];

    Show_Str(50, 5, WHITE, BLACK, (uint8_t*)"���ޱ���", 16, 0);
    LCD_DrawLine(0, 25, 160, 25);

    Show_Str(10, 35, CYAN, BLACK, (uint8_t*)"��������:", 14, 0);

    // ��ʾ����ʹ��״̬
    sprintf(str_buffer, "����ʹ��: %s", g_interface_manager.alarm_enabled ? "����" : "�ر�");
    Show_Str(10, 55, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    // ��ʾVSWR��ֵ
    sprintf(str_buffer, "VSWR��ֵ: %.1f", g_interface_manager.vswr_alarm_threshold);
    Show_Str(10, 70, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    Show_Str(10, 85, YELLOW, BLACK, (uint8_t*)"����ʱ����������", 12, 0);

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"��:����", 12, 0);
}

/**
 * @brief �������ý�����ʾ
 */
void Interface_DisplayBrightness(void)
{
    char str_buffer[32];

    Show_Str(50, 5, WHITE, BLACK, (uint8_t*)"��ʾ����", 16, 0);
    LCD_DrawLine(0, 25, 160, 25);

    Show_Str(10, 35, CYAN, BLACK, (uint8_t*)"��������:", 14, 0);

    // ��ʾ��ǰ����
    sprintf(str_buffer, "��ǰ����: %d0%%", g_interface_manager.brightness_level);
    Show_Str(10, 55, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0);

    // ����������
    LCD_DrawRectangle(10, 75, 140, 85);
    uint16_t bar_width = g_interface_manager.brightness_level * 130 / 10;
    LCD_Fill(11, 76, 10 + bar_width, 84, GREEN);

    Show_Str(10, 95, YELLOW, BLACK, (uint8_t*)"��:��������", 12, 0);
    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"��:����", 12, 0);
}

/**
 * @brief ���ڽ�����ʾ
 */
void Interface_DisplayAbout(void)
{
    Show_Str(50, 5, WHITE, BLACK, (uint8_t*)"�����豸", 16, 0);
    LCD_DrawLine(0, 25, 160, 25);

    Show_Str(10, 35, CYAN, BLACK, (uint8_t*)"��Ƶ���ʼ� V1.0", 14, 0);
    Show_Str(10, 55, WHITE, BLACK, (uint8_t*)"Ƶ�ʷ�Χ: 1Hz-100MHz", 12, 0);
    Show_Str(10, 70, WHITE, BLACK, (uint8_t*)"���ʷ�Χ: 0.1W-1kW", 12, 0);
    Show_Str(10, 85, WHITE, BLACK, (uint8_t*)"VSWR����: 1.0-10.0", 12, 0);
    Show_Str(10, 100, WHITE, BLACK, (uint8_t*)"������: ��Ĺ�˾", 12, 0);

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"��:����", 12, 0);
}
