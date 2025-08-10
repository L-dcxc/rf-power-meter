#include "eeprom_24c16.h"
#include <string.h>

/**
 * @brief ��ȡ�豸��ַ
 * @param addr �洢����ַ
 * @return �豸��ַ
 */
uint8_t BL24C16_GetDeviceAddr(uint16_t addr)
{
    // 24C16���豸��ַ��0xA0 | ((addr >> 8) << 1)
    // ��λ��ַA10:A8���ڿ����ֽڵ�A2:A0λ
    return EEPROM_BASE_ADDR | ((addr >> 8) << 1);
}

/**
 * @brief ����ַ��Χ
 * @param addr ��ʼ��ַ
 * @param len ���ݳ���
 * @return EEPROM_OK-��ַ��Ч������-��ַ��Ч
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
 * @brief �ȴ�EEPROMд�������
 * @return EEPROM_OK-�ɹ���EEPROM_TIMEOUT-��ʱ
 */
int BL24C16_WaitReady(void)
{
    uint32_t timeout = 0;
    uint8_t ack;
    
    // ACK��ѯ����д�����ڣ�EEPROM����Ӧ��
    while(timeout < (EEPROM_WRITE_TIMEOUT_MS * 1000 / EEPROM_ACK_POLL_DELAY_US))
    {
        SoftI2C_Start();
        ack = SoftI2C_WriteByte(EEPROM_BASE_ADDR);  // ����д�豸��ַ
        SoftI2C_Stop();
        
        if(ack == I2C_ACK)
            return EEPROM_OK;   // �յ�ACK��д�������
            
        // ��ʱ������
        HAL_Delay(1);
        timeout += (1000 / EEPROM_ACK_POLL_DELAY_US);
    }
    
    return EEPROM_TIMEOUT;
}

/**
 * @brief ��ʼ��EEPROM
 * @return EEPROM_OK-�ɹ�������-ʧ��
 */
int BL24C16_Init(void)
{
    // ��ʼ�����I2C
    SoftI2C_Init();
    
    // ������EEPROMͨ��
    SoftI2C_Start();
    uint8_t ack = SoftI2C_WriteByte(EEPROM_BASE_ADDR);
    SoftI2C_Stop();
    
    if(ack == I2C_ACK)
        return EEPROM_OK;
    else
        return EEPROM_ERROR;
}

/**
 * @brief ҳд������
 * @param addr ��ʼ��ַ
 * @param data ����ָ��
 * @param len ���ݳ���(������16�ֽ��Ҳ���ҳ)
 * @return EEPROM_OK-�ɹ�������-ʧ��
 */
int BL24C16_WritePage(uint16_t addr, const uint8_t* data, uint8_t len)
{
    uint8_t dev_addr = BL24C16_GetDeviceAddr(addr);
    uint8_t word_addr = addr & 0xFF;
    uint8_t ack;
    
    // ���ҳ�߽�
    if((addr % EEPROM_PAGE_SIZE) + len > EEPROM_PAGE_SIZE)
        return EEPROM_SIZE_ERROR;
    
    // ��ʼд����
    SoftI2C_Start();
    
    // �����豸��ַ(д)
    ack = SoftI2C_WriteByte(dev_addr);
    if(ack != I2C_ACK)
    {
        SoftI2C_Stop();
        return EEPROM_ERROR;
    }
    
    // �����ֵ�ַ
    ack = SoftI2C_WriteByte(word_addr);
    if(ack != I2C_ACK)
    {
        SoftI2C_Stop();
        return EEPROM_ERROR;
    }
    
    // ��������
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
    
    // �ȴ�д�������
    return BL24C16_WaitReady();
}

/**
 * @brief ��ȡ����
 * @param addr ��ʼ��ַ
 * @param buf ���ݻ�����
 * @param len ���ݳ���
 * @return EEPROM_OK-�ɹ�������-ʧ��
 */
int BL24C16_Read(uint16_t addr, uint8_t* buf, uint16_t len)
{
    int ret;
    uint16_t current_addr = addr;
    uint16_t remaining = len;
    uint16_t offset = 0;
    
    // ����ַ��Χ
    ret = BL24C16_CheckAddr(addr, len);
    if(ret != EEPROM_OK)
        return ret;
    
    while(remaining > 0)
    {
        uint8_t dev_addr = BL24C16_GetDeviceAddr(current_addr);
        uint8_t word_addr = current_addr & 0xFF;
        uint8_t ack;
        
        // ���㵱ǰ���ڿɶ�ȡ���ֽ���
        uint16_t block_remaining = EEPROM_BLOCK_SIZE - (current_addr % EEPROM_BLOCK_SIZE);
        uint16_t read_len = (remaining < block_remaining) ? remaining : block_remaining;
        
        // ����������͵�ַ
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
        
        // ���¿�ʼ���л�����ģʽ
        SoftI2C_Start();
        
        ack = SoftI2C_WriteByte(dev_addr | 0x01);  // ����ַ
        if(ack != I2C_ACK)
        {
            SoftI2C_Stop();
            return EEPROM_ERROR;
        }
        
        // ������ȡ����
        for(uint16_t i = 0; i < read_len; i++)
        {
            uint8_t send_ack = (i < read_len - 1) ? 1 : 0;  // ���һ���ֽڷ���NACK
            buf[offset + i] = SoftI2C_ReadByte(send_ack);
        }
        
        SoftI2C_Stop();
        
        // ���µ�ַ�ͼ���
        current_addr += read_len;
        remaining -= read_len;
        offset += read_len;
    }
    
    return EEPROM_OK;
}

/**
 * @brief д������
 * @param addr ��ʼ��ַ
 * @param data ����ָ��
 * @param len ���ݳ���
 * @return EEPROM_OK-�ɹ�������-ʧ��
 */
int BL24C16_Write(uint16_t addr, const uint8_t* data, uint16_t len)
{
    int ret;
    uint16_t current_addr = addr;
    uint16_t remaining = len;
    uint16_t offset = 0;

    // ����ַ��Χ
    ret = BL24C16_CheckAddr(addr, len);
    if(ret != EEPROM_OK)
        return ret;

    while(remaining > 0)
    {
        // ���㵱ǰҳ�ڿ�д����ֽ���
        uint16_t page_offset = current_addr % EEPROM_PAGE_SIZE;
        uint16_t page_remaining = EEPROM_PAGE_SIZE - page_offset;
        uint16_t write_len = (remaining < page_remaining) ? remaining : page_remaining;

        // ҳд��
        ret = BL24C16_WritePage(current_addr, &data[offset], write_len);
        if(ret != EEPROM_OK)
            return ret;

        // ���µ�ַ�ͼ���
        current_addr += write_len;
        remaining -= write_len;
        offset += write_len;
    }

    return EEPROM_OK;
}

/**
 * @brief ��������(�ȶ���Ƚϣ�ֻд��仯������)
 * @param addr ��ʼ��ַ
 * @param data ����ָ��
 * @param len ���ݳ���
 * @return EEPROM_OK-�ɹ�������-ʧ��
 */
int BL24C16_Update(uint16_t addr, const uint8_t* data, uint16_t len)
{
    int ret;
    uint8_t read_buf[EEPROM_PAGE_SIZE];
    uint16_t current_addr = addr;
    uint16_t remaining = len;
    uint16_t offset = 0;

    // ����ַ��Χ
    ret = BL24C16_CheckAddr(addr, len);
    if(ret != EEPROM_OK)
        return ret;

    while(remaining > 0)
    {
        // ���㵱ǰҳ�ڿɴ�����ֽ���
        uint16_t page_offset = current_addr % EEPROM_PAGE_SIZE;
        uint16_t page_remaining = EEPROM_PAGE_SIZE - page_offset;
        uint16_t process_len = (remaining < page_remaining) ? remaining : page_remaining;

        // ��ȡ��ǰ����
        ret = BL24C16_Read(current_addr, read_buf, process_len);
        if(ret != EEPROM_OK)
            return ret;

        // �Ƚ����ݣ�ֻ�в�ͬ��д��
        if(memcmp(read_buf, &data[offset], process_len) != 0)
        {
            ret = BL24C16_WritePage(current_addr, &data[offset], process_len);
            if(ret != EEPROM_OK)
                return ret;
        }

        // ���µ�ַ�ͼ���
        current_addr += process_len;
        remaining -= process_len;
        offset += process_len;
    }

    return EEPROM_OK;
}
