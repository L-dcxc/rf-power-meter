#include "soft_i2c.h"

/**
 * @brief I2Cʱ����ʱ
 */
void SoftI2C_Delay(void)
{
    // Լ5us��ʱ��������100kHz I2C�ٶ�
    for(volatile int i = 0; i < 50; i++);
}

/**
 * @brief SDA���ø�
 */
void SoftI2C_SDA_High(void)
{
    HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_PIN_SET);
}

/**
 * @brief SDA���õ�
 */
void SoftI2C_SDA_Low(void)
{
    HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_PIN_RESET);
}

/**
 * @brief SCL���ø�
 */
void SoftI2C_SCL_High(void)
{
    HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_SET);
}

/**
 * @brief SCL���õ�
 */
void SoftI2C_SCL_Low(void)
{
    HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_RESET);
}

/**
 * @brief ��ȡSDA��״̬
 * @return 1-�ߵ�ƽ��0-�͵�ƽ
 */
uint8_t SoftI2C_SDA_Read(void)
{
    return HAL_GPIO_ReadPin(I2C_SDA_PORT, I2C_SDA_PIN);
}

/**
 * @brief ��ʼ��I2C GPIO
 */
void SoftI2C_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // ʹ��GPIOBʱ��
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    // ����PB10(SDA)��PB11(SCL)Ϊ��©���
    GPIO_InitStruct.Pin = I2C_SDA_PIN | I2C_SCL_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;     // ��©���
    GPIO_InitStruct.Pull = GPIO_PULLUP;             // �ڲ�����
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;   // ����
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    // ��ʼ״̬��SDA��SCL���ø�
    SoftI2C_SDA_High();
    SoftI2C_SCL_High();
    SoftI2C_Delay();
}

/**
 * @brief ����I2C��ʼ�ź�
 */
void SoftI2C_Start(void)
{
    SoftI2C_SDA_High();
    SoftI2C_SCL_High();
    SoftI2C_Delay();
    
    SoftI2C_SDA_Low();      // SDA������
    SoftI2C_Delay();
    
    SoftI2C_SCL_Low();      // Ȼ��SCL����
    SoftI2C_Delay();
}

/**
 * @brief ����I2Cֹͣ�ź�
 */
void SoftI2C_Stop(void)
{
    SoftI2C_SDA_Low();
    SoftI2C_SCL_Low();
    SoftI2C_Delay();
    
    SoftI2C_SCL_High();     // SCL������
    SoftI2C_Delay();
    
    SoftI2C_SDA_High();     // Ȼ��SDA����
    SoftI2C_Delay();
}

/**
 * @brief дһ���ֽڵ�I2C����
 * @param byte Ҫд����ֽ�
 * @return I2C_ACK(0)-�յ�Ӧ��I2C_NACK(1)-δ�յ�Ӧ��
 */
uint8_t SoftI2C_WriteByte(uint8_t byte)
{
    uint8_t i;
    uint8_t ack;
    
    // ����8λ���ݣ�MSB�ȷ�
    for(i = 0; i < 8; i++)
    {
        SoftI2C_SCL_Low();
        SoftI2C_Delay();
        
        if(byte & 0x80)
            SoftI2C_SDA_High();
        else
            SoftI2C_SDA_Low();
            
        byte <<= 1;
        SoftI2C_Delay();
        
        SoftI2C_SCL_High();
        SoftI2C_Delay();
    }
    
    // ��ȡӦ��λ
    SoftI2C_SCL_Low();
    SoftI2C_SDA_High();     // �ͷ�SDA���ô��豸����
    SoftI2C_Delay();
    
    SoftI2C_SCL_High();
    SoftI2C_Delay();
    
    ack = SoftI2C_SDA_Read();   // ��ȡӦ��λ��0=ACK��1=NACK
    
    SoftI2C_SCL_Low();
    SoftI2C_Delay();
    
    return ack;
}

/**
 * @brief ��I2C���߶�ȡһ���ֽ�
 * @param ack 1-����ACK��0-����NACK
 * @return ��ȡ�����ֽ�
 */
uint8_t SoftI2C_ReadByte(uint8_t ack)
{
    uint8_t i;
    uint8_t byte = 0;
    
    SoftI2C_SDA_High();     // �ͷ�SDA���ô��豸����
    
    // ��ȡ8λ���ݣ�MSB�ȶ�
    for(i = 0; i < 8; i++)
    {
        SoftI2C_SCL_Low();
        SoftI2C_Delay();
        
        SoftI2C_SCL_High();
        SoftI2C_Delay();
        
        byte <<= 1;
        if(SoftI2C_SDA_Read())
            byte |= 0x01;
    }
    
    // ����Ӧ��λ
    SoftI2C_SCL_Low();
    SoftI2C_Delay();
    
    if(ack)
        SoftI2C_SDA_Low();      // ����ACK
    else
        SoftI2C_SDA_High();     // ����NACK
        
    SoftI2C_Delay();
    
    SoftI2C_SCL_High();
    SoftI2C_Delay();
    
    SoftI2C_SCL_Low();
    SoftI2C_Delay();
    
    return byte;
}
