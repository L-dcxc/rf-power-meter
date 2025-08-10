#ifndef __SOFT_I2C_H__
#define __SOFT_I2C_H__

#include "main.h"
#include "gpio.h"

/* I2C���Ŷ��� */
#define I2C_SDA_PORT    GPIOB
#define I2C_SDA_PIN     GPIO_PIN_10
#define I2C_SCL_PORT    GPIOB
#define I2C_SCL_PIN     GPIO_PIN_11

/* I2CӦ���ź� */
#define I2C_ACK         0
#define I2C_NACK        1

/* �������� */
void SoftI2C_Init(void);                    // ��ʼ��I2C GPIO
void SoftI2C_Start(void);                   // ������ʼ�ź�
void SoftI2C_Stop(void);                    // ����ֹͣ�ź�
uint8_t SoftI2C_WriteByte(uint8_t byte);    // дһ���ֽڣ�����ACK/NACK
uint8_t SoftI2C_ReadByte(uint8_t ack);      // ��һ���ֽڣ�ack=1����ACK��ack=0����NACK
void SoftI2C_Delay(void);                   // I2Cʱ����ʱ

/* �ڲ����� */
void SoftI2C_SDA_High(void);                // SDA�ø�
void SoftI2C_SDA_Low(void);                 // SDA�õ�
void SoftI2C_SCL_High(void);                // SCL�ø�
void SoftI2C_SCL_Low(void);                 // SCL�õ�
uint8_t SoftI2C_SDA_Read(void);             // ��ȡSDA״̬

#endif /* __SOFT_I2C_H__ */
