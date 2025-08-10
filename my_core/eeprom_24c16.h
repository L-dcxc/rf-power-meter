#ifndef __EEPROM_24C16_H__
#define __EEPROM_24C16_H__

#include "main.h"
#include "soft_i2c.h"

/* BL24C16F参数定义 */
#define EEPROM_SIZE         2048        // 总容量：2KB
#define EEPROM_PAGE_SIZE    16          // 页大小：16字节
#define EEPROM_BLOCK_SIZE   256         // 块大小：256字节
#define EEPROM_BASE_ADDR    0xA0        // 基础设备地址

/* 错误代码定义 */
#define EEPROM_OK           0           // 操作成功
#define EEPROM_ERROR        -1          // 一般错误
#define EEPROM_TIMEOUT      -2          // 超时错误
#define EEPROM_ADDR_ERROR   -3          // 地址错误
#define EEPROM_SIZE_ERROR   -4          // 大小错误

/* 超时参数 */
#define EEPROM_WRITE_TIMEOUT_MS     10  // 写操作超时时间(ms)
#define EEPROM_ACK_POLL_DELAY_US    500 // ACK轮询间隔(us)

/* 函数声明 */
int BL24C16_Init(void);                                         // 初始化EEPROM
int BL24C16_Read(uint16_t addr, uint8_t* buf, uint16_t len);    // 读取数据
int BL24C16_Write(uint16_t addr, const uint8_t* data, uint16_t len);  // 写入数据
int BL24C16_Update(uint16_t addr, const uint8_t* data, uint16_t len); // 更新数据(先读后比较)
int BL24C16_WritePage(uint16_t addr, const uint8_t* data, uint8_t len); // 页写入
int BL24C16_WaitReady(void);                                    // 等待写操作完成

/* 内部函数 */
uint8_t BL24C16_GetDeviceAddr(uint16_t addr);                  // 获取设备地址
int BL24C16_CheckAddr(uint16_t addr, uint16_t len);            // 检查地址范围

#endif /* __EEPROM_24C16_H__ */
