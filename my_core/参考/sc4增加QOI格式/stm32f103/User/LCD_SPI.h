#ifndef LCD_SPI_H
#define LCD_SPI_H

#include "BSP_Common.h"

#define USE_HORIZONTAL 3 
#if USE_HORIZONTAL==0||USE_HORIZONTAL==1
#define LCD_W 240
#define LCD_H 280

#else
#define LCD_W 280
#define LCD_H 240
#endif


#define LCD_SCL_PORT    GPIOA
#define LCD_SCL_PIN     GPIO_Pin_12

#define LCD_SDA_PORT    GPIOB
#define LCD_SDA_PIN     GPIO_Pin_13

#define LCD_RES_PORT    GPIOA
#define LCD_RES_PIN     GPIO_Pin_1


#define LCD_DC_PORT     GPIOB
#define LCD_DC_PIN      GPIO_Pin_0

#define LCD_CS_PORT     GPIOB
#define LCD_CS_PIN      GPIO_Pin_12

#define LCD_BLK_PORT    GPIOB
#define LCD_BLK_PIN     GPIO_Pin_1

#define LCD_RES_Clr()  GPIO_ResetBits(GPIOA,GPIO_Pin_1)//RES
#define LCD_RES_Set()  GPIO_SetBits(GPIOA,GPIO_Pin_1)

#define LCD_DC_Clr()   GPIO_ResetBits(GPIOB,GPIO_Pin_0)//DC
#define LCD_DC_Set()   GPIO_SetBits(GPIOB,GPIO_Pin_0)
 		     
#define LCD_CS_Clr()   GPIO_ResetBits(GPIOB,GPIO_Pin_12)//CS
#define LCD_CS_Set()   GPIO_SetBits(GPIOB,GPIO_Pin_12)

#define LCD_BLK_Clr()  GPIO_ResetBits(GPIOB,GPIO_Pin_1)//BLK
#define LCD_BLK_Set()  GPIO_SetBits(GPIOB,GPIO_Pin_1)


void inline LCD_GPIO_Init(void)
{
	  GPIO_InitTypeDef  GPIO_InitStructure;
		RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA|RCC_APB2Periph_GPIOB ,ENABLE);
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0  |GPIO_Pin_1  | GPIO_Pin_12;//  初始化片选 RS 背光 CS 
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
		GPIO_Init(GPIOB, &GPIO_InitStructure);  
		GPIO_InitStructure.GPIO_Pin = 	GPIO_Pin_1;
		GPIO_Init(GPIOA, &GPIO_InitStructure);  
}

void LCD_Init(void);
void LCD_Address_Set(u16 x1,u16 y1,u16 x2,u16 y2);
void Set_SPI_DataSize_16b();
void LCD_Fill(uint16_t xsta,uint16_t ysta,uint16_t xend,uint16_t yend,uint16_t color);
void LCD_DrawPoint(uint16_t x,uint16_t y,uint16_t color);
void LCD_DMA_Fill_color(uint16_t xs,uint16_t ys,uint16_t w,uint16_t h,uint16_t *color);
#endif
