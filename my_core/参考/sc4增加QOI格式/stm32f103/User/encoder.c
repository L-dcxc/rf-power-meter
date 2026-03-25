
#include "encoder.h"

#define ENC_A_PORT GPIOA
#define ENC_A_PIN  GPIO_Pin_0
#define ENC_B_PORT GPIOA
#define ENC_B_PIN  GPIO_Pin_1

static uint8_t gpio_read(GPIO_TypeDef *port, uint16_t pin)
{
    return GPIO_ReadInputDataBit(port, pin) ? 1 : 0;
}

void ENC_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin  = ENC_A_PIN | ENC_B_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU; 
    GPIO_Init(ENC_A_PORT, &gpio);
}
const int state_table[4][4] = {
		{0,  -1,  1,  0}, 
		{1,   0,  0, -1},  
		{-1,  0,  0,  1},  
		{0,   1, -1,  0}  
};

int ENC_GetDir(void)
{
		static uint8_t encoder_state = 1; 
	  int direction=0;
		uint8_t a = gpio_read(ENC_A_PORT, ENC_A_PIN);
		uint8_t b = gpio_read(ENC_B_PORT, ENC_B_PIN);
		uint8_t  state= (b << 1) | a;
	   if(state!=encoder_state)
		{
				if(a!=b)
				{
					  direction = state_table[encoder_state][state];
				}
		}
		encoder_state = state;
		return direction;
}