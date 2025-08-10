#ifndef __EEPROM_24C16_H__
#define __EEPROM_24C16_H__

#include "main.h"
#include "soft_i2c.h"

/* BL24C16F�������� */
#define EEPROM_SIZE         2048        // ��������2KB
#define EEPROM_PAGE_SIZE    16          // ҳ��С��16�ֽ�
#define EEPROM_BLOCK_SIZE   256         // ���С��256�ֽ�
#define EEPROM_BASE_ADDR    0xA0        // �����豸��ַ

/* ������붨�� */
#define EEPROM_OK           0           // �����ɹ�
#define EEPROM_ERROR        -1          // һ�����
#define EEPROM_TIMEOUT      -2          // ��ʱ����
#define EEPROM_ADDR_ERROR   -3          // ��ַ����
#define EEPROM_SIZE_ERROR   -4          // ��С����

/* ��ʱ���� */
#define EEPROM_WRITE_TIMEOUT_MS     10  // д������ʱʱ��(ms)
#define EEPROM_ACK_POLL_DELAY_US    500 // ACK��ѯ���(us)

/* �������� */
int BL24C16_Init(void);                                         // ��ʼ��EEPROM
int BL24C16_Read(uint16_t addr, uint8_t* buf, uint16_t len);    // ��ȡ����
int BL24C16_Write(uint16_t addr, const uint8_t* data, uint16_t len);  // д������
int BL24C16_Update(uint16_t addr, const uint8_t* data, uint16_t len); // ��������(�ȶ���Ƚ�)
int BL24C16_WritePage(uint16_t addr, const uint8_t* data, uint8_t len); // ҳд��
int BL24C16_WaitReady(void);                                    // �ȴ�д�������

/* �ڲ����� */
uint8_t BL24C16_GetDeviceAddr(uint16_t addr);                  // ��ȡ�豸��ַ
int BL24C16_CheckAddr(uint16_t addr, uint16_t len);            // ����ַ��Χ

#endif /* __EEPROM_24C16_H__ */
