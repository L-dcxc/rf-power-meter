#ifndef __SOFT_I2C_H__
#define __SOFT_I2C_H__

#include "main.h"
#include "gpio.h"

/* I2C引脚定义 */
#define I2C_SDA_PORT    GPIOB
#define I2C_SDA_PIN     GPIO_PIN_10
#define I2C_SCL_PORT    GPIOB
#define I2C_SCL_PIN     GPIO_PIN_11

/* I2C应答信号 */
#define I2C_ACK         0
#define I2C_NACK        1

/* 函数声明 */
void SoftI2C_Init(void);                    // 初始化I2C GPIO
void SoftI2C_Start(void);                   // 发送起始信号
void SoftI2C_Stop(void);                    // 发送停止信号
uint8_t SoftI2C_WriteByte(uint8_t byte);    // 写一个字节，返回ACK/NACK
uint8_t SoftI2C_ReadByte(uint8_t ack);      // 读一个字节，ack=1发送ACK，ack=0发送NACK
void SoftI2C_Delay(void);                   // I2C时序延时

/* 内部函数 */
void SoftI2C_SDA_High(void);                // SDA置高
void SoftI2C_SDA_Low(void);                 // SDA置低
void SoftI2C_SCL_High(void);                // SCL置高
void SoftI2C_SCL_Low(void);                 // SCL置低
uint8_t SoftI2C_SDA_Read(void);             // 读取SDA状态

#endif /* __SOFT_I2C_H__ */
