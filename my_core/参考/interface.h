#ifndef __INTERFACE_H
#define __INTERFACE_H

extern u8 func_index;
extern u8 count_100ms_en;
extern char aht_str[10];


// ����һ���ṹ����������������
typedef struct {
    u8 current;                      // ��ǰ������
    u8 enter;                        // ȷ��
    u8 ret;                          // ����
    void (*current_operation)(void); // ��ʾ����
  } Menu;


void disp_nowtime();       //��ʾ��ǰʱ��
void menu_choose();        //��ʾ�˵�ѡ��
void Action();             //��ʾ��������
void system_information(); //ϵͳ��Ϣ
void Menu_desplay(void);   // ������ʾ
void main_interface();     // ��ʾ�����溯��
void disp_set_time();      //����ʱ�����
void shutdowm();           //�ػ�����
void recharge_init();      //������
void lowpower_off();//���ƿ�������
void set_Language();//�����л�
void scan_password();//�������
void balck_system();//�ָ�����
void zero_set();//�������
void Alarm_look();//�����鿴
void Alarm_set();//��������
void Calibration_set();//�궨����
void choose_Gas();//����ѡ��
void backlight_set();//�������
void sleep_set();//��������
void dis_low_high(u8 x, u8 y, u8 num);//��ʾ��������
void alarm_log_clear();
void Mute_mode();//��������
#endif
