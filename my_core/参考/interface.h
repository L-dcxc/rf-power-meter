#ifndef __INTERFACE_H
#define __INTERFACE_H

extern u8 func_index;
extern u8 count_100ms_en;
extern char aht_str[10];


// 定义一个结构体包含你想操作内容
typedef struct {
    u8 current;                      // 当前索引号
    u8 enter;                        // 确定
    u8 ret;                          // 返回
    void (*current_operation)(void); // 显示函数
  } Menu;


void disp_nowtime();       //显示当前时间
void menu_choose();        //显示菜单选着
void Action();             //显示开机界面
void system_information(); //系统信息
void Menu_desplay(void);   // 界面显示
void main_interface();     // 显示主界面函数
void disp_set_time();      //设置时间界面
void shutdowm();           //关机界面
void recharge_init();      //充电界面
void lowpower_off();//限制开机界面
void set_Language();//语言切换
void scan_password();//密码界面
void balck_system();//恢复出厂
void zero_set();//零点设置
void Alarm_look();//报警查看
void Alarm_set();//报警设置
void Calibration_set();//标定设置
void choose_Gas();//气体选择
void backlight_set();//背光调节
void sleep_set();//休眠设置
void dis_low_high(u8 x, u8 y, u8 num);//显示报警类型
void alarm_log_clear();
void Mute_mode();//声音设置
#endif
