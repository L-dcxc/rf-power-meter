
#include "BSP_Common.h"
#include "LCD_SPI.h"

void Timer2_Init(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	
	TIM_InternalClockConfig(TIM2);
	
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInitStructure.TIM_Period = 10 - 1;	//1ms
	TIM_TimeBaseInitStructure.TIM_Prescaler = 7200 - 1;
	TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);
	
	TIM_ClearFlag(TIM2, TIM_FLAG_Update);
	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
	
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
	
	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
	NVIC_Init(&NVIC_InitStructure);
	
	TIM_Cmd(TIM2, ENABLE);
}

void SPI2_init(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	SPI_InitTypeDef  SPI_InitStructure;
	DMA_InitTypeDef DMA_InitStructure;
	RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOC|RCC_APB2Periph_GPIOB ,ENABLE);
	
	//配置SPI2管脚
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO|RCC_APB2Periph_GPIOB, ENABLE);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14| GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_SetBits(GPIOB,GPIO_Pin_13|GPIO_Pin_14|GPIO_Pin_15);		
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	if((SPI2->CR1 &0x0040)==0)  
	{
		//----------硬件SPI-------------
			RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2 ,ENABLE);
			SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
			SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
			SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
			SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
			SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
			SPI_InitStructure.SPI_NSS =  SPI_NSS_Soft;
			SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;//2分频18M
			SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
			SPI_InitStructure.SPI_CRCPolynomial = 7;
			SPI_Init(SPI2, &SPI_InitStructure);
			SPI_Cmd(SPI2, ENABLE);
	}


    NVIC_InitTypeDef NVIC_InitStructure;
    // 使能DMA1_Channel5中断
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel5_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);


 // 使能SPI2的DMA请求
    SPI_I2S_DMACmd(SPI2, SPI_I2S_DMAReq_Tx, ENABLE);

    // 使能DMA1时钟
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    // DMA1_Channel5配置，用于SPI2_TX
    DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t) &SPI2->DR;
    DMA_InitStructure.DMA_MemoryBaseAddr = 0;
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralDST;
    DMA_InitStructure.DMA_BufferSize = 0;
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    DMA_InitStructure.DMA_Mode = DMA_Mode_Normal;
    DMA_InitStructure.DMA_Priority = DMA_Priority_VeryHigh;
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel5, &DMA_InitStructure);

    // 使能DMA1_Channel5中断
    DMA_ITConfig(DMA1_Channel5, DMA_IT_TC, ENABLE);

    // 使能DMA1_Channel5
    DMA_Cmd(DMA1_Channel5, ENABLE);

}

void SPI2_Send_dat8(uint8_t dat) 
{	
		SPI2->CR1&=0x7FF;									//设置SPI8位传输模式
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET){}//等待发送区空  
	  SPI_I2S_SendData(SPI2, dat);                                  //通过外设SPIx发送一个byte  数据
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY) == SET);
}

void SPI2_Send_dat16(uint16_t dat)
{
	 SPI2->CR1 |= SPI_DataSize_16b;				//设置SPI16位传输模式
   while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET){}//等待发送区空  
	 SPI_I2S_SendData(SPI2, dat);                                  //通过外设SPIx发送一个byte  数据
   while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY) == SET);
}
// 发送指定长度数据（DMA方式）
void SPI2_Send_DMA_buf(uint8_t *dataBuffer, uint16_t len) {
   
		if (dataBuffer == NULL || len == 0) return; // 参数检查
	  SPI2->CR1 |= SPI_DataSize_16b;   		//设置SPI16位传输模式
		DMA_Cmd(DMA1_Channel5, DISABLE );
		DMA1_Channel5->CMAR = (uint32_t)dataBuffer;
		DMA1_Channel5->CNDTR= len;
		DMA_Cmd(DMA1_Channel5, ENABLE);
}
//  等待DMA传输完成
void SPI2_WaitForDmaDone(void) 
{
	   while(DMA1_Channel5->CNDTR);
		 SPI2->CR1&=0x7FF;									//还原SPI8位传输模式
}


//  DMA1通道5中断服务程序（SPI2发送完成）
void DMA1_Channel5_IRQHandler(void) {
	
    // 检查传输完成中断
    if (DMA_GetITStatus(DMA1_IT_TC5) != RESET) {
        // 清除中断标志
        DMA_ClearITPendingBit(DMA1_IT_TC5);
        
        // 等待SPI发送完成
        while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY) == SET);
 
        // 停止DMA通道
        DMA_Cmd(DMA1_Channel5, DISABLE);
			
		    DMA1_Channel5->CNDTR=0;
			
				LCD_CS_Set();		
    }
}
   