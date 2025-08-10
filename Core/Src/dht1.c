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
 * @brief ��DHT11����Ϊ�������ģʽ
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
 * @brief ��DHT11����Ϊ��������ģʽ
 * ************************************************************************
 */
void DHT11_UP_IN(void)
{

	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.Pin = DHT11_PIN;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;	//����
	HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

//��λDHT11
void DHT11_Rst(void)	   
{                 
	DHT11_PP_OUT(); 	//SET OUTPUT
	DHT11_PULL_0; 	//����
	HAL_Delay(20);    	//������ʱ����18ms
	DHT11_PULL_1; 	//DQ=1������ 
	my_delay_us(50);     	//������ʱ����20~40us
}

//����Ӧ
//����1��������
//����0�����ɹ�
uint8_t DHT11_Check(void) 	   
{   
	uint8_t retry=0;
	DHT11_UP_IN();//SET INPUT	 
    while (DHT11_ReadPin&&retry<100)//DHT11����40~80us
	{
		retry++;
		my_delay_us(1);
	};	 
	if(retry>=100)return 1;
	else retry=0;
    while (!DHT11_ReadPin&&retry<100)//DHT11�ٴ�����40~80us
	{
		retry++;
		my_delay_us(1);
	};
	if(retry>=100)return 1;	    
	return 0;
}

//��ȡһ��λBit
//����1��0
uint8_t DHT11_Read_Bit(void) 			 
{
 	uint8_t retry=0;
	while(DHT11_ReadPin&&retry<100)//�ȴ���͵�ƽ
	{
		retry++;
		my_delay_us(1);
	}
	retry=0;
	while(!DHT11_ReadPin&&retry<100)//�ȴ���ߵ�ƽ
	{
		retry++;
		my_delay_us(1);
	}
	my_delay_us(40);//�ȴ�40us
	if(DHT11_ReadPin)return 1;
	else return 0;		   
}

//��ȡһ���ֽ�
//���ض���������
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

//DHT11��ȡһ������
//temp:�¶�(��Χ:0~50��)
//humi:ʪ��(��Χ:20%~90%)
//tem���¶�С��λ
//hum��ʪ��С��λ
uint8_t DHT11_Read_Data(uint8_t *temp,uint8_t *humi,uint8_t *tem,uint8_t *hum)    
{        
 	uint8_t buf[5];
	uint8_t i;
	DHT11_Rst();
	if(DHT11_Check()==0)
	{
		for(i=0;i<5;i++)//��ȡ40λ�ֽ�
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
//        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_RESET); // ��PA5�õ͵�ƽ
//        HAL_Delay(19); // ��ʱ18ms
//        HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, GPIO_PIN_SET); // ��PA5�øߵ�ƽ
//        my_delay_us(20); // ��ʱ20us

//        // ����PA5Ϊ����ģʽ�Խ�������
//        DHT11_UP_IN();

//        // ��DHT11��ȡ����
//        if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET) // ���PA5������
//        {
//            while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET); // �ȴ����ű��
//            while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_SET); // �ȴ����ű��

//            // ��ȡ40λ����
//            for (int i = 0; i < 40; i++)
//            {
//                if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_SET) // �ȴ�50us
//                my_delay_us(50); // ��ʱ70us
//                if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_SET) // �������Ϊ�ߵ�ƽ
//                    buffer[i / 8] |= (1 << (7 - (i % 8))); // bit����Ϊ1
//                else
//                    buffer[i / 8] &= ~(1 << (7 - (i % 8))); // bit����Ϊ0
//                while (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_SET); // �ȴ����ű��
//            }

//            // У�����֤
//            RH_byte1 = buffer[0];
//            RH_byte2 = buffer[1];
//            T_byte1 = buffer[2];
//            T_byte2 = buffer[3];
//            checksum = buffer[4];
//            if (checksum == (RH_byte1 + RH_byte2 + T_byte1 + T_byte2)) // ���У�����ȷ
//            {
//							humi=RH_byte1<<8|RH_byte2;
//							temp=T_byte1<<8|T_byte2;
//							OLED_ShowNum(58,0,humi,2,4);
//							OLED_ShowNum(58,16,temp,2,4);
//						}
///**
// * ************************************************************************
// * @brief ��ȡ�ֽ�
// * @return temp
// * ************************************************************************
// */
//uint8_t DHT11_ReadByte(void)
//{
//	uint8_t i, temp = 0;

//	for (i = 0; i < 8; i++)
//	{
//		while (DHT11_ReadPin == 0);		// �ȴ��͵�ƽ����
//		
//		my_delay_us(40);			//	��ʱ 40 ΢��
//		
//		if (DHT11_ReadPin == 1)
//		{
//			while (DHT11_ReadPin == 1);	// �ȴ��ߵ�ƽ����
//			
//			temp |= (uint8_t)(0X01 << (7 - i));			// �ȷ��͸�λ
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
// * @brief ��ȡһ������
// * @param[in] DHT11_Data  ����Ľṹ�����
// * @return 0��1������У���Ƿ�ɹ���
// * @note ��������DHT11���������źţ�Ȼ��ȴ�DHT11��Ӧ�����DHT11��ȷӦ��
// * 		 �������ȡʪ��������ʪ��С�����¶��������¶�С����У������ݣ�
// * 		 ������У����Խ�������У��
// * ************************************************************************
// */
//uint8_t DHT11_ReadData(DHT11_Data_TypeDef *DHT11_Data)
//{
//	DHT11_PP_OUT();			// �����������������
//	DHT11_PULL_0;	
//	my_delay_ms(18);				// ��ʱ 18 ms
//	
//	DHT11_PULL_1;					// �������ߣ���ʱ 30 us
//	my_delay_us(30);	

//	DHT11_UP_IN();				// �������룬��ȡ DHT11 ����
//	
//	if (DHT11_ReadPin == 0)				// �յ��ӻ�Ӧ��
//	{
//		while (DHT11_ReadPin == 0);		// �ȴ��ӻ�Ӧ��ĵ͵�ƽ����
//		
//		while (DHT11_ReadPin == 1);		// �ȴ��ӻ�Ӧ��ĸߵ�ƽ����
//		
//		/*��ʼ��������*/   
//		DHT11_Data->humi_int  = DHT11_ReadByte();
//		DHT11_Data->humi_dec = DHT11_ReadByte();
//		DHT11_Data->temp_int  = DHT11_ReadByte();
//		DHT11_Data->temp_dec = DHT11_ReadByte();
//		DHT11_Data->check_sum = DHT11_ReadByte();
//		
//		DHT11_PP_OUT();		// ��ȡ��������������
//		DHT11_PULL_1;	
//		
//		// ����У��
//		if (DHT11_Data->check_sum == DHT11_Data->humi_int + DHT11_Data->humi_dec + DHT11_Data->temp_int + DHT11_Data->temp_dec)	
//		{
//			return 1;
//		}		
//		else
//		{
//			return 0;
//		}
//	}
//	else		// δ�յ��ӻ�Ӧ��
//	{
//		return 0;
//	}
//}

void my_delay_us(uint32_t nus)
{
   //���ö�ʱ1us�ж�һ��
	HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000000);
    //����ϵͳ�Դ�����ʱ����
	HAL_Delay(nus - 1);
    //����ʱ�жϻָ�Ϊ1ms�ж�
	HAL_SYSTICK_Config(HAL_RCC_GetHCLKFreq()/1000);
}

void my_delay_ms(uint32_t ms)
{
    my_delay_us(ms * 1000);
}
