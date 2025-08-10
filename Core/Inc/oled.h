#ifndef __OLED_H
#define __OLED_H
//#include "stm32f10x.h"
#include "gpio.h"
/*��������*/
#define OLED_W_SCL(x)		HAL_GPIO_WritePin(GPIOA,GPIO_PIN_7,x);
#define OLED_W_SDA(x)		HAL_GPIO_WritePin(GPIOA,GPIO_PIN_6,x);

typedef   unsigned char         u8;
typedef   unsigned short int    u16;
typedef   unsigned int          u32;


void OLED_Init(void);//OLED��ʼ��
void OLED_Clear(void);//����
void OLED_Refresh_Gram(void);//�����Դ浽OLED
void OLED_WriteData(uint8_t Data);//OLEDд����
void OLED_WriteCommand(uint8_t Command);//OLEDд����
void OLED_DrawPoint(u8 x,u8 y,u8 t);//����
void OLED_ShowChar(u8 x,u8 y,u8 chr,u8 size,u8 mode);//oled��ʾ�ַ�
void OLED_ShowString(u8 x,u8 y,const u8 *p,u8 size);//oled��ʾ�ַ���
void OLED_ShowNum(u8 x,u8 y,u32 num,u8 len,u8 size);//oled��ʾ����
u32 mypow(u8 m,u8 n);//m^n����
void OLED_ShowChinese(u8 x,u8 y,u8 num,u8 size,u8 mode);//oled��ʾ����
#endif



