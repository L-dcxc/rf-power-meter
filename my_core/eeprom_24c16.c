#include "eeprom_24c16.h"
#include <string.h>

/**
 * @brief 获取设备地址
 * @param addr 存储器地址
 * @return 设备地址
 */
uint8_t BL24C16_GetDeviceAddr(uint16_t addr)
{
    // 24C16的设备地址：0xA0 | ((addr >> 8) << 1)
    // 高位地址A10:A8放在控制字节的A2:A0位
    return EEPROM_BASE_ADDR | ((addr >> 8) << 1);
}

/**
 * @brief 检查地址范围
 * @param addr 起始地址
 * @param len 数据长度
 * @return EEPROM_OK-地址有效，其他-地址无效
 */
int BL24C16_CheckAddr(uint16_t addr, uint16_t len)
{
    if(addr >= EEPROM_SIZE)
        return EEPROM_ADDR_ERROR;
        
    if((addr + len) > EEPROM_SIZE)
        return EEPROM_SIZE_ERROR;
        
    return EEPROM_OK;
}

/**
 * @brief 等待EEPROM写操作完成
 * @return EEPROM_OK-成功，EEPROM_TIMEOUT-超时
 */
int BL24C16_WaitReady(void)
{
    uint32_t timeout = 0;
    uint8_t ack;
    
    // ACK轮询：在写周期内，EEPROM不会应答
    while(timeout < (EEPROM_WRITE_TIMEOUT_MS * 1000 / EEPROM_ACK_POLL_DELAY_US))
    {
        SoftI2C_Start();
        ack = SoftI2C_WriteByte(EEPROM_BASE_ADDR);  // 尝试写设备地址
        SoftI2C_Stop();
        
        if(ack == I2C_ACK)
            return EEPROM_OK;   // 收到ACK，写操作完成
            
        // 延时后重试
        HAL_Delay(1);
        timeout += (1000 / EEPROM_ACK_POLL_DELAY_US);
    }
    
    return EEPROM_TIMEOUT;
}

/**
 * @brief 初始化EEPROM
 * @return EEPROM_OK-成功，其他-失败
 */
int BL24C16_Init(void)
{
    // 初始化软件I2C
    SoftI2C_Init();
    
    // 尝试与EEPROM通信
    SoftI2C_Start();
    uint8_t ack = SoftI2C_WriteByte(EEPROM_BASE_ADDR);
    SoftI2C_Stop();
    
    if(ack == I2C_ACK)
        return EEPROM_OK;
    else
        return EEPROM_ERROR;
}

/**
 * @brief 页写入数据
 * @param addr 起始地址
 * @param data 数据指针
 * @param len 数据长度(不超过16字节且不跨页)
 * @return EEPROM_OK-成功，其他-失败
 */
int BL24C16_WritePage(uint16_t addr, const uint8_t* data, uint8_t len)
{
    uint8_t dev_addr = BL24C16_GetDeviceAddr(addr);
    uint8_t word_addr = addr & 0xFF;
    uint8_t ack;
    
    // 检查页边界
    if((addr % EEPROM_PAGE_SIZE) + len > EEPROM_PAGE_SIZE)
        return EEPROM_SIZE_ERROR;
    
    // 开始写操作
    SoftI2C_Start();
    
    // 发送设备地址(写)
    ack = SoftI2C_WriteByte(dev_addr);
    if(ack != I2C_ACK)
    {
        SoftI2C_Stop();
        return EEPROM_ERROR;
    }
    
    // 发送字地址
    ack = SoftI2C_WriteByte(word_addr);
    if(ack != I2C_ACK)
    {
        SoftI2C_Stop();
        return EEPROM_ERROR;
    }
    
    // 发送数据
    for(uint8_t i = 0; i < len; i++)
    {
        ack = SoftI2C_WriteByte(data[i]);
        if(ack != I2C_ACK)
        {
            SoftI2C_Stop();
            return EEPROM_ERROR;
        }
    }
    
    SoftI2C_Stop();
    
    // 等待写操作完成
    return BL24C16_WaitReady();
}

/**
 * @brief 读取数据
 * @param addr 起始地址
 * @param buf 数据缓冲区
 * @param len 数据长度
 * @return EEPROM_OK-成功，其他-失败
 */
int BL24C16_Read(uint16_t addr, uint8_t* buf, uint16_t len)
{
    int ret;
    uint16_t current_addr = addr;
    uint16_t remaining = len;
    uint16_t offset = 0;
    
    // 检查地址范围
    ret = BL24C16_CheckAddr(addr, len);
    if(ret != EEPROM_OK)
        return ret;
    
    while(remaining > 0)
    {
        uint8_t dev_addr = BL24C16_GetDeviceAddr(current_addr);
        uint8_t word_addr = current_addr & 0xFF;
        uint8_t ack;
        
        // 计算当前块内可读取的字节数
        uint16_t block_remaining = EEPROM_BLOCK_SIZE - (current_addr % EEPROM_BLOCK_SIZE);
        uint16_t read_len = (remaining < block_remaining) ? remaining : block_remaining;
        
        // 随机读：发送地址
        SoftI2C_Start();
        
        ack = SoftI2C_WriteByte(dev_addr);
        if(ack != I2C_ACK)
        {
            SoftI2C_Stop();
            return EEPROM_ERROR;
        }
        
        ack = SoftI2C_WriteByte(word_addr);
        if(ack != I2C_ACK)
        {
            SoftI2C_Stop();
            return EEPROM_ERROR;
        }
        
        // 重新开始，切换到读模式
        SoftI2C_Start();
        
        ack = SoftI2C_WriteByte(dev_addr | 0x01);  // 读地址
        if(ack != I2C_ACK)
        {
            SoftI2C_Stop();
            return EEPROM_ERROR;
        }
        
        // 连续读取数据
        for(uint16_t i = 0; i < read_len; i++)
        {
            uint8_t send_ack = (i < read_len - 1) ? 1 : 0;  // 最后一个字节发送NACK
            buf[offset + i] = SoftI2C_ReadByte(send_ack);
        }
        
        SoftI2C_Stop();
        
        // 更新地址和计数
        current_addr += read_len;
        remaining -= read_len;
        offset += read_len;
    }
    
    return EEPROM_OK;
}

/**
 * @brief 写入数据
 * @param addr 起始地址
 * @param data 数据指针
 * @param len 数据长度
 * @return EEPROM_OK-成功，其他-失败
 */
int BL24C16_Write(uint16_t addr, const uint8_t* data, uint16_t len)
{
    int ret;
    uint16_t current_addr = addr;
    uint16_t remaining = len;
    uint16_t offset = 0;

    // 检查地址范围
    ret = BL24C16_CheckAddr(addr, len);
    if(ret != EEPROM_OK)
        return ret;

    while(remaining > 0)
    {
        // 计算当前页内可写入的字节数
        uint16_t page_offset = current_addr % EEPROM_PAGE_SIZE;
        uint16_t page_remaining = EEPROM_PAGE_SIZE - page_offset;
        uint16_t write_len = (remaining < page_remaining) ? remaining : page_remaining;

        // 页写入
        ret = BL24C16_WritePage(current_addr, &data[offset], write_len);
        if(ret != EEPROM_OK)
            return ret;

        // 更新地址和计数
        current_addr += write_len;
        remaining -= write_len;
        offset += write_len;
    }

    return EEPROM_OK;
}

/**
 * @brief 更新数据(先读后比较，只写入变化的数据)
 * @param addr 起始地址
 * @param data 数据指针
 * @param len 数据长度
 * @return EEPROM_OK-成功，其他-失败
 */
int BL24C16_Update(uint16_t addr, const uint8_t* data, uint16_t len)
{
    int ret;
    uint8_t read_buf[EEPROM_PAGE_SIZE];
    uint16_t current_addr = addr;
    uint16_t remaining = len;
    uint16_t offset = 0;

    // 检查地址范围
    ret = BL24C16_CheckAddr(addr, len);
    if(ret != EEPROM_OK)
        return ret;

    while(remaining > 0)
    {
        // 计算当前页内可处理的字节数
        uint16_t page_offset = current_addr % EEPROM_PAGE_SIZE;
        uint16_t page_remaining = EEPROM_PAGE_SIZE - page_offset;
        uint16_t process_len = (remaining < page_remaining) ? remaining : page_remaining;

        // 读取当前数据
        ret = BL24C16_Read(current_addr, read_buf, process_len);
        if(ret != EEPROM_OK)
            return ret;

        // 比较数据，只有不同才写入
        if(memcmp(read_buf, &data[offset], process_len) != 0)
        {
            ret = BL24C16_WritePage(current_addr, &data[offset], process_len);
            if(ret != EEPROM_OK)
                return ret;
        }

        // 更新地址和计数
        current_addr += process_len;
        remaining -= process_len;
        offset += process_len;
    }

    return EEPROM_OK;
}
