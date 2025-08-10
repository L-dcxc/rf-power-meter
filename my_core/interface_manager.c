/**
 * @file interface_manager.c
 * @brief ��Ƶ���ʼƽ������ϵͳʵ���ļ�
 * @author ����Ѽ
 * @date 2025-08-10
 */

#include "interface_manager.h"
#include "gpio.h"
#include "tim.h"
#include "eeprom_24c16.h"
#include <stdio.h>
#include <string.h>
#include "stm32f1xx_it.h"
// �ⲿ��������
extern IWDG_HandleTypeDef hiwdg;

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
        InterfaceManager_Beep(30);  // ����������
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
    Show_Str(20, 5, WHITE, BLACK, (uint8_t*)"RF Power Meter", 16, 0); //��Ƶ���ʼƱ���

    // ���Ʒָ���
    LCD_DrawLine(0, 25, 160, 25); //ˮƽ�ָ���
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
        Show_Str(10, 35 + i * 18, color, bg_color, (uint8_t*)str_buffer, 16, 0); //�˵���(��ǰѡ��Ϊ��ɫ������ɫ)
    }

    // ��ʾ������ʾ
    //Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back DN:Next OK:Enter", 12, 0); //������ʾ(��ע��)
}

/**
 * @brief У׼������ʾ
 */
void Interface_DisplayCalibration(void)
{
    Show_Str(40, 5, WHITE, BLACK, (uint8_t*)"Calibration", 16, 0); //У׼���ܱ���
    LCD_DrawLine(0, 25, 160, 25); //����ָ���

    Show_Str(10, 35, CYAN, BLACK, (uint8_t*)"Cal Steps:", 16, 0); //У׼�����ǩ
    Show_Str(10, 55, WHITE, BLACK, (uint8_t*)"1. Open Cal", 12, 0); //��·У׼
    Show_Str(10, 70, WHITE, BLACK, (uint8_t*)"2. Short Cal", 12, 0); //��·У׼
    Show_Str(10, 85, WHITE, BLACK, (uint8_t*)"3. Load Cal", 12, 0); //����У׼
    Show_Str(10, 100, WHITE, BLACK, (uint8_t*)"4. Power Cal", 12, 0); //����У׼

    Show_Str(5, 115, GRAY, BLACK, (uint8_t*)"UP:Back", 12, 0); //������ʾ
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

    // ��ʾ����ʹ��״̬
    sprintf(str_buffer, "Alarm Enable: %s", g_interface_manager.alarm_enabled ? "ON" : "OFF");
    Show_Str(10, 55, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //����ʹ��״̬

    // ��ʾVSWR��ֵ
    sprintf(str_buffer, "VSWR Limit: %.1f", g_interface_manager.vswr_alarm_threshold);
    Show_Str(10, 70, WHITE, BLACK, (uint8_t*)str_buffer, 12, 0); //פ���ȱ�����ֵ

    Show_Str(10, 85, YELLOW, BLACK, (uint8_t*)"Buzzer when exceed", 12, 0); //���޷�������ʾ

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

    // ����4��PWM���� (��Ҫ�����ػ棬��Ϊ��Ļ�ռ䲻��)
    LCD_Clear(BLACK);
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
