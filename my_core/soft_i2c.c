#include "soft_i2c.h"

/**
 * @brief I2C时序延时
 */
void SoftI2C_Delay(void)
{
    // 约5us延时，适用于100kHz I2C速度
    for(volatile int i = 0; i < 50; i++);
}

/**
 * @brief SDA线置高
 */
void SoftI2C_SDA_High(void)
{
    HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_PIN_SET);
}

/**
 * @brief SDA线置低
 */
void SoftI2C_SDA_Low(void)
{
    HAL_GPIO_WritePin(I2C_SDA_PORT, I2C_SDA_PIN, GPIO_PIN_RESET);
}

/**
 * @brief SCL线置高
 */
void SoftI2C_SCL_High(void)
{
    HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_SET);
}

/**
 * @brief SCL线置低
 */
void SoftI2C_SCL_Low(void)
{
    HAL_GPIO_WritePin(I2C_SCL_PORT, I2C_SCL_PIN, GPIO_PIN_RESET);
}

/**
 * @brief 读取SDA线状态
 * @return 1-高电平，0-低电平
 */
uint8_t SoftI2C_SDA_Read(void)
{
    return HAL_GPIO_ReadPin(I2C_SDA_PORT, I2C_SDA_PIN);
}

/**
 * @brief 初始化I2C GPIO
 */
void SoftI2C_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // 使能GPIOB时钟
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    // 配置PB10(SDA)和PB11(SCL)为开漏输出
    GPIO_InitStruct.Pin = I2C_SDA_PIN | I2C_SCL_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;     // 开漏输出
    GPIO_InitStruct.Pull = GPIO_PULLUP;             // 内部上拉
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;   // 高速
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    
    // 初始状态：SDA和SCL都置高
    SoftI2C_SDA_High();
    SoftI2C_SCL_High();
    SoftI2C_Delay();
}

/**
 * @brief 发送I2C起始信号
 */
void SoftI2C_Start(void)
{
    SoftI2C_SDA_High();
    SoftI2C_SCL_High();
    SoftI2C_Delay();
    
    SoftI2C_SDA_Low();      // SDA先拉低
    SoftI2C_Delay();
    
    SoftI2C_SCL_Low();      // 然后SCL拉低
    SoftI2C_Delay();
}

/**
 * @brief 发送I2C停止信号
 */
void SoftI2C_Stop(void)
{
    SoftI2C_SDA_Low();
    SoftI2C_SCL_Low();
    SoftI2C_Delay();
    
    SoftI2C_SCL_High();     // SCL先拉高
    SoftI2C_Delay();
    
    SoftI2C_SDA_High();     // 然后SDA拉高
    SoftI2C_Delay();
}

/**
 * @brief 写一个字节到I2C总线
 * @param byte 要写入的字节
 * @return I2C_ACK(0)-收到应答，I2C_NACK(1)-未收到应答
 */
uint8_t SoftI2C_WriteByte(uint8_t byte)
{
    uint8_t i;
    uint8_t ack;
    
    // 发送8位数据，MSB先发
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
    
    // 读取应答位
    SoftI2C_SCL_Low();
    SoftI2C_SDA_High();     // 释放SDA，让从设备控制
    SoftI2C_Delay();
    
    SoftI2C_SCL_High();
    SoftI2C_Delay();
    
    ack = SoftI2C_SDA_Read();   // 读取应答位：0=ACK，1=NACK
    
    SoftI2C_SCL_Low();
    SoftI2C_Delay();
    
    return ack;
}

/**
 * @brief 从I2C总线读取一个字节
 * @param ack 1-发送ACK，0-发送NACK
 * @return 读取到的字节
 */
uint8_t SoftI2C_ReadByte(uint8_t ack)
{
    uint8_t i;
    uint8_t byte = 0;
    
    SoftI2C_SDA_High();     // 释放SDA，让从设备控制
    
    // 读取8位数据，MSB先读
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
    
    // 发送应答位
    SoftI2C_SCL_Low();
    SoftI2C_Delay();
    
    if(ack)
        SoftI2C_SDA_Low();      // 发送ACK
    else
        SoftI2C_SDA_High();     // 发送NACK
        
    SoftI2C_Delay();
    
    SoftI2C_SCL_High();
    SoftI2C_Delay();
    
    SoftI2C_SCL_Low();
    SoftI2C_Delay();
    
    return byte;
}
