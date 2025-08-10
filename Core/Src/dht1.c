/**
 * ************************************************************************
 * 
 * @file My_DHT11.c
 * @author zxr
 * @brief 
 * 
 * ************************************************************************
 * @copyright Copyright (c) 2024 zxr 
 * ************************************************************************
 */
#include "dht1.h"
#include "gpio.h"
//#include "tim.h"
/**
 * ************************************************************************
 * @brief 将DHT11配置为推挽输出模式
 * ************************************************************************
 */
void DHT11_PP_OUT(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.Pin = DHT11_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;	
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
	HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

/**
 * ************************************************************************
 * @brief 将DHT11配置为上拉输入模式
 * ************************************************************************
 */
void DHT11_UP_IN(void)
{

	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.Pin = DHT11_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;	//上拉
	HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

//复位DHT11
void DHT11_Rst(void)	   
{                 
	DHT11_PP_OUT(); 	//SET OUTPUT
	DHT11_PULL_0; 	//拉低
	HAL_Delay(20);    	//拉低延时至少18ms
	DHT11_PULL_1; 	//DQ=1，拉高 
	my_delay_us(50);     	//拉高延时至少20~40us
}

//检测回应
//返回1：检测错误
//返回0：检测成功
uint8_t DHT11_Check(void) 	   
{   
	uint8_t retry=0;
	DHT11_UP_IN();//SET INPUT	 
    while (DHT11_ReadPin&&retry<100)//DHT11拉低40~80us
	{
		retry++;
		my_delay_us(1);
	};	 
	if(retry>=100)return 1;
	else retry=0;
    while (!DHT11_ReadPin&&retry<100)//DHT11再次拉高40~80us
	{
		retry++;
		my_delay_us(1);
	};
	if(retry>=100)return 1;	    
	return 0;
}

//读取一个位Bit
//返回1或0
uint8_t DHT11_Read_Bit(void) 			 
{
 	uint8_t retry=0;
	while(DHT11_ReadPin&&retry<100)//等待变低电平
	{
		retry++;
		my_delay_us(1);
	}
	retry=0;
	while(!DHT11_ReadPin&&retry<100)//等待变高电平
	{
		retry++;
		my_delay_us(1);
	}
	my_delay_us(40);//等待40us
	if(DHT11_ReadPin)return 1;
	else return 0;		   
}

//读取一个字节
//返回读到的数据
uint8_t DHT11_Read_Byte(void)    
{        
	uint8_t i,dat;
	dat=0;
	for (i=0;i<8;i++) 
	{
		dat<<=1; 
		dat|=DHT11_Read_Bit();
	}						    
	return dat;
}

//DHT11读取一次数据
//temp:温度(范围:0~50°)
//humi:湿度(范围:20%~90%)
//tem：温度小数位
//hum：湿度小数位
uint8_t DHT11_Read_Data(uint8_t *temp,uint8_t *humi,uint8_t *tem,uint8_t *hum)    
{        
 	uint8_t buf[5];
	uint8_t i;
	DHT11_Rst();
	if(DHT11_Check()==0)
	{
		for(i=0;i<5;i++)//读取40位字节
		{
			buf[i]=DHT11_Read_Byte();
		}
		if((buf[0]+buf[1]+buf[2]+buf[3])==buf[4])
		{
			*humi=buf[0];
			*hum=buf[1];
			*temp=buf[2];
			*tem=buf[3];
		}
	}
	else return 1;
	return 0;	    
}


uint8_t DHT11_Init(void)
{	
	DHT11_PULL_1;
	DHT11_Rst();
	return DHT11_Check();
}

//		DHT11_PP_OUT();
//        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); // 将PA5置低电平
//        HAL_Delay(19); // 延时18ms
//        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET); // 将PA5置高电平
//        my_delay_us(20); // 延时20us

//        // 设置PA5为输入模式以接收数据
//        DHT11_UP_IN();

//        // 从DHT11读取数据
//        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET) // 如果PA5被拉低
//        {
//            while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET); // 等待引脚变高
//            while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_SET); // 等待引脚变低

//            // 读取40位数据
//            for (int i = 0; i < 40; i++)
//            {
//                if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_SET) // 等待50us
//                my_delay_us(50); // 延时70us
//                if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_SET) // 如果引脚为高电平
//                    buffer[i / 8] |= (1 << (7 - (i % 8))); // bit设置为1
//                else
//                    buffer[i / 8] &= ~(1 << (7 - (i % 8))); // bit设置为0
//                while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_SET); // 等待引脚变低
//            }

//            // 校验和验证
//            RH_byte1 = buffer[0];
//            RH_byte2 = buffer[1];
//            T_byte1 = buffer[2];
//            T_byte2 = buffer[3];
//            checksum = buffer[4];
//            if (checksum == (RH_byte1 + RH_byte2 + T_byte1 + T_byte2)) // 如果校验和正确
//            {
//							humi=RH_byte1<<8|RH_byte2;
//							temp=T_byte1<<8|T_byte2;
//							OLED_ShowNum(58,0,humi,2,4);
//							OLED_ShowNum(58,16,temp,2,4);
//						}
///**
// * ************************************************************************
// * @brief 读取字节
// * @return temp
// * ************************************************************************
// */
//uint8_t DHT11_ReadByte(void)
//{
//	uint8_t i, temp = 0;

//	for (i = 0; i < 8; i++)
//	{
//		while (DHT11_ReadPin == 0);		// 等待低电平结束
//		
//		my_delay_us(40);			//	延时 40 微秒
//		
//		if (DHT11_ReadPin == 1)
//		{
//			while (DHT11_ReadPin == 1);	// 等待高电平结束
//			
//			temp |= (uint8_t)(0X01 << (7 - i));			// 先发送高位
//		}
//		else
//		{
//			temp &= (uint8_t)~(0X01 << (7 - i));
//		}
//	}
//	return temp;
//}

///**
// * ************************************************************************
// * @brief 读取一次数据
// * @param[in] DHT11_Data  定义的结构体变量
// * @return 0或1（数据校验是否成功）
// * @note 它首先向DHT11发送启动信号，然后等待DHT11的应答。如果DHT11正确应答，
// * 		 则继续读取湿度整数、湿度小数、温度整数、温度小数和校验和数据，
// * 		 并计算校验和以进行数据校验
// * ************************************************************************
// */
//uint8_t DHT11_ReadData(DHT11_Data_TypeDef *DHT11_Data)
//{
//	DHT11_PP_OUT();			// 主机输出，主机拉低
//	DHT11_PULL_0;	
//	my_delay_ms(18);				// 延时 18 ms
//	
//	DHT11_PULL_1;					// 主机拉高，延时 30 us
//	my_delay_us(30);	

//	DHT11_UP_IN();				// 主机输入，获取 DHT11 数据
//	
//	if (DHT11_ReadPin == 0)				// 收到从机应答
//	{
//		while (DHT11_ReadPin == 0);		// 等待从机应答的低电平结束
//		
//		while (DHT11_ReadPin == 1);		// 等待从机应答的高电平结束
//		
//		/*开始接收数据*/   
//		DHT11_Data->humi_int  = DHT11_ReadByte();
//		DHT11_Data->humi_dec = DHT11_ReadByte();
//		DHT11_Data->temp_int  = DHT11_ReadByte();
//		DHT11_Data->temp_dec = DHT11_ReadByte();
//		DHT11_Data->check_sum = DHT11_ReadByte();
//		
//		DHT11_PP_OUT();		// 读取结束，主机拉高
//		DHT11_PULL_1;	
//		
//		// 数据校验
//		if (DHT11_Data->check_sum == DHT11_Data->humi_int + DHT11_Data->humi_dec + DHT11_Data->temp_int + DHT11_Data->temp_dec)	
//		{
//			return 1;
//		}		
//		else
//		{
//			return 0;
//		}
//	}
//	else		// 未收到从机应答
//	{
//		return 0;
//	}
//}

void my_delay_us(uint32_t nus)
{
   //设置定时1us中断一次
	HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000000);
    //调用系统自带的延时函数
	HAL_Delay(nus - 1);
    //将定时中断恢复为1ms中断
	HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);
}

void my_delay_ms(uint32_t ms)
{
    my_delay_us(ms * 1000);
}
