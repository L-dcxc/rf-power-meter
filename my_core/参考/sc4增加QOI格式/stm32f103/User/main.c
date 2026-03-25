
#include "BSP_Common.h"
#include "LCD_SPI.h"
#include "stm32f103_flash_hard_stdspi_port.h"
#include "w25qxx.h"
#include "encoder.h"

#include "sc_demo_test.h"
#include "sc_obj.h"

void HSE_Init(uint32_t RCC_PLLMul_x) // 范围2-16
{
	ErrorStatus HSEStatus; // 定义
	// 重置RCC，否则不会有效果
	RCC_DeInit();
	// 打开HSE
	RCC_HSEConfig(RCC_HSE_ON);
	HSEStatus = RCC_WaitForHSEStartUp();

	// 判断HSE状态
	if (HSEStatus == SUCCESS)
	{
		// 库里面直接抄，针对flash
		FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
		FLASH_SetLatency(FLASH_Latency_2);

		// 对AHB APB1 APB2 分频
		RCC_HCLKConfig(RCC_SYSCLK_Div1); // AHB 72MHz Max
		RCC_PCLK1Config(RCC_HCLK_Div2);	 // APB1 36MHz Max
		RCC_PCLK2Config(RCC_HCLK_Div1);	 // APB2 72MHz Max

		// 设置PLL,官方库中声明要先设置后打开PLL
		RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_x);
		// 使能PLL
		RCC_PLLCmd(ENABLE);

		// 检测PLL状态,等待稳定跳出语句
		while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
			;
		// 系统时钟选为PLL
		RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
		while (RCC_GetSYSCLKSource() != 0x08)
			;
	}
	else
	{
		// 配置失败
	}
}

// 初始化按键
void bsp_key_init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_Pin = KEY_UP_PIN | KEY_DOWM_PIN | KEY_LEFT_PIN | KEY_RIGHT_PIN | KEY_ENTER_PIN;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
}
// 按键扫描
void bsp_key_scan_start(void *arg)
{
	static uint16_t last_key = 0;
	uint16_t key = 0;
	if (GPIO_ReadOutputDataBit(GPIOA, KEY_UP_PIN) == 0)
	{
		key |= CMD_UP; // 上
	}
	if (GPIO_ReadOutputDataBit(GPIOB, KEY_DOWM_PIN) == 0)
	{
		key |= CMD_DOWN; // 下
	}
	if (GPIO_ReadOutputDataBit(GPIOA, KEY_LEFT_PIN) == 0)
	{
		key |= CMD_LEFT; // 右
	}
	if (GPIO_ReadOutputDataBit(GPIOB, KEY_RIGHT_PIN) == 0)
	{
		key |= CMD_RIGHT; // 右
	}
	if (GPIO_ReadOutputDataBit(GPIOB, KEY_ENTER_PIN) == 0)
	{
		key |= CMD_ENTER; // 确定
	}
	if (key != last_key)
	{
		if (key)
		{
			sc_send_cmd_event(key); // 按下
		}
		else
		{
			// sc_send_cmd_event(last_key|0x8000); //释放
		}
		last_key = key;
	}
	sc_add_delay(0, bsp_key_scan_start, 20,arg); // 启动延时器0,重载值20ms
}

//共用SPI2
void spiflash_read(uint8_t *buf, uint32_t offset, uint32_t size)
{
	  SPI2_WaitForDmaDone();						//等LCD完成
		w25qxx_read_data(offset,buf,size);
}
extern lv_font_t lv_font_12;
int main(void)
{
	HSE_Init(RCC_PLLMul_16); //芯片超频 16倍频
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2); // 设置中断优先级分组2
	ENC_Init();
	//---先初始化flash防止冲突----
	flash_port_init();
	w25qxx_init();
	
	Timer2_Init();
  LCD_Init(); 			// LCD初始化
	sc_gui_init(LCD_DMA_Fill_color, 0, C_BLUE, C_ROYAL_BLUE, &lv_font_12);
	sc_clear(0, 0, SC_SCREEN_WIDTH, SC_SCREEN_HEIGHT, gui->bkc);
	//-------简易demo------------
	// sc_demo_pfs(18);
// sc_demo_text();
// sc_demo_label();
// sc_demo_Bar();
// sc_demo_button();
// sc_demo_Switch();
// sc_demo_Textbox();
// sc_demo_Image();
// sc_demo_Arc();
	//-------任务demo------------
	//sc_create_task(0, sc_demo_trans_task, 20);
	// sc_create_task(0, sc_demo_drity_task, 20);
	// sc_create_task(0, sc_demo_menu_task, 20);
	// sc_create_task(0, sc_demo_Textbox_task, 20);
		//	sc_create_task(0, sc_watch_demo_task, 20);
	// //-------触控demo----
  sc_demo_obj_watch();

	
	while (1)
	{
		  sc_task_loop(sc_touch_event); // 事件处理
      sc_touch_loop();
	}
}

// 理论 18M/240*280*16=16.74
// 实测PFS= 15.9@ 10行
void TIM2_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM2, TIM_IT_Update) == SET)
	{
		system_tick++;
		int enc = ENC_GetDir();
		if (enc == -1)
		{
			sc_send_cmd_event(CMD_LEFT); 
		}
		else if (enc == 1)
		{
			sc_send_cmd_event(CMD_RIGHT);
		}
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
	}
}
