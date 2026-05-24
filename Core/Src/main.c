/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "dac.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "log.h"
#include "nv3007.h"
#include "multi_led.h"

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

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

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
  MX_DAC1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_SPI1_Init();
  MX_TIM6_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */
 log_init(&huart1);
 
log_printf("system start");
HAL_TIM_Base_Start_IT(&htim6); // 启动定时器 6 的中断
    LED_Driver_System_Init();
// #define LED1_G_Pin GPIO_PIN_12
// #define LED1_G_GPIO_Port GPIOD
// #define LED1_R_Pin GPIO_PIN_13
// #define LED1_R_GPIO_Port GPIOD
// #define LED1_B_Pin GPIO_PIN_14
// #define LED1_B_GPIO_Port GPIOD
LED_Object_t led_blue;  // PE3 -> TIM3_CH1
LED_Object_t led_green; // PE4 -> TIM3_CH2
LED_Object_t led_red;   // PE5 -> TIM3_CH3
    LED_Driver_Init(&led_green, LED1_G_GPIO_Port, LED1_G_Pin, &htim4, TIM_CHANNEL_2, 1);
    LED_Driver_Init(&led_blue, LED1_B_GPIO_Port, LED1_B_Pin, &htim4, TIM_CHANNEL_1, 1);
    LED_Driver_Init(&led_red, LED1_R_GPIO_Port, LED1_R_Pin, &htim4, TIM_CHANNEL_3, 1);
    LED_Driver_SendCmd(&led_blue, LED_MODE_PWM, LED_Off_Handler, 0, 0, NULL);
    LED_Driver_SendCmd(&led_green, LED_MODE_PWM, LED_Off_Handler, 0, 0, NULL);

    LED_Driver_SendCmd(&led_red, LED_MODE_PWM, LED_Heartbeat_Handler, 2000, 0, NULL);


NV3007_Init();
// NV3007_SetRotation(NV3007_ROTATION);
// HAL_GPIO_WritePin(SPI1_PWM_GPIO_Port, SPI1_PWM_Pin, GPIO_PIN_SET); // 打开背光
    // NV3007_Fill(RED);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    // // 2. 全屏刷红色，并等待 1 秒
    // NV3007_Fill(RED);
    // HAL_Delay(2000);
    
    // // 3. 全屏刷绿色，并等待 1 秒
    // NV3007_Fill(GREEN);
    // HAL_Delay(2000);
    
    // // 4. 全屏刷蓝色，并等待 1 秒
    // NV3007_Fill(BLUE);
    // HAL_Delay(2000);

// NV3007_Fill(0xF800); // 红
  NV3007_Fill(RED);
HAL_Delay(2000);
// 填充红色矩形：x=10,y=20,宽50,高100
NV3007_Fill(GREEN);
HAL_Delay(2000);
// 填充绿色矩形：x=60,y=120,宽60,高80

  NV3007_Fill(BLACK);
HAL_Delay(2000);

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

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 20;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {

        static uint16_t cnt = 0;
        cnt++;
        if(cnt >= 10)  // 每 10ms 执行一次
        {
            cnt = 0;

           LED_Driver_Update();;   
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
#ifdef USE_FULL_ASSERT
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
