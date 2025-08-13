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
// �ⲿ��������
extern PowerResult_t g_power_result;
extern RFParams_t g_rf_params;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
// ���ݲɼ��ʹ���������
void ProcessPowerMeasurement(void);     // �����ʲ���
void ProcessRFParameters(void);         // ������Ƶ��������
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
  //HAL_TIM_Base_Start_IT(&htim2);          //ʹ���ж��ⲿʱ�������ڲ��ж�
  HAL_TIM_Base_Start_IT(&htim3);          //ʹ���ж�
  //HAL_TIM_Base_Start_IT(&htim4);          //ʹ���ж�
  // ��ʼ��LCD
  LCD_Init();
  LCD_SetBacklight(8000);  // ���ñ�������Ϊ80%
  LCD_Clear(BLACK);

  // ִ��ϵͳ��������
  System_BootSequence();

  // ������ɺ��һ��ι��
  HAL_IWDG_Refresh(&hiwdg);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    // ��ѭ����ʼʱι��
    HAL_IWDG_Refresh(&hiwdg);

    // �����ʲ���
    ProcessPowerMeasurement();

    // ������Ƶ��������
    ProcessRFParameters();

    // ������������ (������ʾ���ºͰ�������)
    InterfaceManager_Process();

    // ���Ƶ�ʼ��½��
    if (FreqCounter_IsNewResult()) {
      // Ƶ�ʽ�����ڽ�����ʾ���Զ���ȡ
      FreqCounter_ClearNewResultFlag();
    }

    // ���洦����ɺ�ι��
    HAL_IWDG_Refresh(&hiwdg);

    // ������ʱ
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
 * @brief �����ʲ���
 */
void ProcessPowerMeasurement(void)
{
    static uint32_t last_adc_time = 0;
    uint32_t current_time = HAL_GetTick();

    // ÿ100ms��ȡһ��ADC (���ʼ��)
    if (current_time - last_adc_time >= 100) {
        last_adc_time = current_time;

        // ����ADCת��
        HAL_ADC_Start(&hadc1);

        // ��ȡPA2 (������)
        HAL_ADC_PollForConversion(&hadc1, 10);
        uint32_t adc_forward = HAL_ADC_GetValue(&hadc1);

        // ��ȡPA3 (���书��)
        HAL_ADC_PollForConversion(&hadc1, 10);
        uint32_t adc_reflected = HAL_ADC_GetValue(&hadc1);

        HAL_ADC_Stop(&hadc1);

        // ת��Ϊ��ѹֵ (3.3V�ο���ѹ��12λADC)
        float forward_voltage = (float)adc_forward * 3.3f / 4095.0f;
        float reflected_voltage = (float)adc_reflected * 3.3f / 4095.0f;

        // ת��Ϊ����ֵ (�������Ӳ���궨��������ʾ����ʽ)
        float forward_power = forward_voltage * 10.0f;    // ʾ����10W/V
        float reflected_power = reflected_voltage * 10.0f;

        // ���¹������ݵ����������
        InterfaceManager_UpdatePower(forward_power, reflected_power);
    }
}

/**
 * @brief ������Ƶ��������
 */
void ProcessRFParameters(void)
{
    static uint32_t last_calc_time = 0;
    uint32_t current_time = HAL_GetTick();

    // ÿ200ms����һ����Ƶ����
    if (current_time - last_calc_time >= 200) {
        last_calc_time = current_time;

        // ������Ƶ����
        if (g_power_result.is_valid && g_power_result.forward_power > 0.01f) {
            // ���㷴��ϵ�� �� = ��(Pr/Pi)
            float reflection_coeff = sqrtf(g_power_result.reflected_power / g_power_result.forward_power);

            // ����פ���� VSWR = (1 + |��|) / (1 - |��|)
            float vswr = (1.0f + reflection_coeff) / (1.0f - reflection_coeff);
            if (vswr < 1.0f) vswr = 1.0f;  // VSWR��СֵΪ1
            if (vswr > 10.0f) vswr = 10.0f; // �������ֵ

            // ���㴫��Ч�� �� = 1 - Pr/Pi
            float transmission_eff = (1.0f - g_power_result.reflected_power / g_power_result.forward_power) * 100.0f;
            if (transmission_eff < 0.0f) transmission_eff = 0.0f;
            if (transmission_eff > 100.0f) transmission_eff = 100.0f;

            // ������Ƶ���������������
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
