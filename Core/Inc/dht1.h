/**
 * ************************************************************************
 * 
 * @file My_DHT11.h
 * @author zxr
 * @brief 
 * 
 * ************************************************************************
 * @copyright Copyright (c) 2024 zxr 
 * ************************************************************************
 */
#ifndef __MY_DHT11_H
#define __MY_DHT11_H

#define DHT11_PORT			GPIOA
#define DHT11_PIN			GPIO_PIN_5

#define DHT11_PULL_1		HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET)
#define DHT11_PULL_0		HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET)

#define DHT11_ReadPin		HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)

typedef   signed          char int8_t;
typedef unsigned           int uint32_t;
typedef unsigned          char uint8_t;
/**
 * ************************************************************************
 * @brief 存储传感器数据的结构体
 * 
 * 
 * ************************************************************************
 */
typedef struct
{
	uint8_t humi_int;			// 湿度的整数部分
	uint8_t humi_dec;	 		// 湿度的小数部分
	uint8_t temp_int;	 		// 温度的整数部分
	uint8_t temp_dec;	 		// 温度的小数部分
	uint8_t check_sum;	 		// 校验和

} DHT11_Data_TypeDef;


//uint8_t DHT11_ReadData(DHT11_Data_TypeDef* DHT11_Data);
void my_delay_us(uint32_t us);
void my_delay_ms(uint32_t ms);
void DHT11_Rst(void);
uint8_t DHT11_Check(void);
uint8_t DHT11_Read_Bit(void);
uint8_t DHT11_Read_Byte(void);
uint8_t DHT11_Read_Data(uint8_t *temp,uint8_t *humi,uint8_t *tem,uint8_t *hum);
uint8_t DHT11_Init(void);
void DHT11_UP_IN(void);
void DHT11_PP_OUT(void);
#endif

