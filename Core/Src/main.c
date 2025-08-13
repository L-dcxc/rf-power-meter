/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "iwdg.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ST7567A.h"
#include "frequency_counter.h"
#include "interface_manager.h"
#include <math.h>
#include "string.h"
#include "stdio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// 外部变量声明
extern PowerResult_t g_power_result;
extern RFParams_t g_rf_params;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
// 数据采集和处理函数声明
void ProcessPowerMeasurement(void);     // 处理功率测量
void ProcessRFParameters(void);         // 处理射频参数计算
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_ADC1_Init();
  MX_IWDG_Init();
  MX_SPI1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */
  HAL_Delay(100);//
  //HAL_TIM_Base_Start_IT(&htim2);          //使能中断外部时钟无需内部中断
  HAL_TIM_Base_Start_IT(&htim3);          //使能中断
  //HAL_TIM_Base_Start_IT(&htim4);          //使能中断
  // 初始化LCD
  LCD_Init();
  LCD_SetBacklight(8000);  // 设置背光亮度为80%
  LCD_Clear(BLACK);

  // 执行系统启动序列
  System_BootSequence();

  // 启动完成后第一次喂狗
  HAL_IWDG_Refresh(&hiwdg);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // 主循环开始时喂狗
    HAL_IWDG_Refresh(&hiwdg);

    // 处理功率测量
    ProcessPowerMeasurement();

    // 处理射频参数计算
    ProcessRFParameters();

    // 处理界面管理器 (包括显示更新和按键处理)
    InterfaceManager_Process();

    // 检查频率计新结果
    if (FreqCounter_IsNewResult()) {
      // 频率结果会在界面显示中自动获取
      FreqCounter_ClearNewResultFlag();
    }

    // 界面处理完成后喂狗
    HAL_IWDG_Refresh(&hiwdg);

    // 短暂延时
    //HAL_Delay(10);

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
 * @brief 处理功率测量
 */
void ProcessPowerMeasurement(void)
{
    static uint32_t last_adc_time = 0;
    uint32_t current_time = HAL_GetTick();

    // 每100ms读取一次ADC (功率检测)
    if (current_time - last_adc_time >= 100) {
        last_adc_time = current_time;

        // 启动ADC转换
        HAL_ADC_Start(&hadc1);

        // 读取PA2 (正向功率)
        HAL_ADC_PollForConversion(&hadc1, 10);
        uint32_t adc_forward = HAL_ADC_GetValue(&hadc1);

        // 读取PA3 (反射功率)
        HAL_ADC_PollForConversion(&hadc1, 10);
        uint32_t adc_reflected = HAL_ADC_GetValue(&hadc1);

        HAL_ADC_Stop(&hadc1);

        // 转换为电压值 (3.3V参考电压，12位ADC)
        float forward_voltage = (float)adc_forward * 3.3f / 4095.0f;
        float reflected_voltage = (float)adc_reflected * 3.3f / 4095.0f;

        // 转换为功率值 (根据你的硬件标定，这里是示例公式)
        float forward_power = forward_voltage * 10.0f;    // 示例：10W/V
        float reflected_power = reflected_voltage * 10.0f;

        // 更新功率数据到界面管理器
        InterfaceManager_UpdatePower(forward_power, reflected_power);
    }
}

/**
 * @brief 处理射频参数计算
 */
void ProcessRFParameters(void)
{
    static uint32_t last_calc_time = 0;
    uint32_t current_time = HAL_GetTick();

    // 每200ms计算一次射频参数
    if (current_time - last_calc_time >= 200) {
        last_calc_time = current_time;

        // 计算射频参数
        if (g_power_result.is_valid && g_power_result.forward_power > 0.01f) {
            // 计算反射系数 Γ = √(Pr/Pi)
            float reflection_coeff = sqrtf(g_power_result.reflected_power / g_power_result.forward_power);

            // 计算驻波比 VSWR = (1 + |Γ|) / (1 - |Γ|)
            float vswr = (1.0f + reflection_coeff) / (1.0f - reflection_coeff);
            if (vswr < 1.0f) vswr = 1.0f;  // VSWR最小值为1
            if (vswr > 10.0f) vswr = 10.0f; // 限制最大值

            // 计算传输效率 η = 1 - Pr/Pi
            float transmission_eff = (1.0f - g_power_result.reflected_power / g_power_result.forward_power) * 100.0f;
            if (transmission_eff < 0.0f) transmission_eff = 0.0f;
            if (transmission_eff > 100.0f) transmission_eff = 100.0f;

            // 更新射频参数到界面管理器
            InterfaceManager_UpdateRFParams(vswr, reflection_coeff, transmission_eff);
        }
    }
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
