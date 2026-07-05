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
#include "dma.h"
#include "i2c.h"
#include "lptim.h"
#include "quadspi.h"
#include "rtc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "FreeRTOS.h"
#include "task.h"
#include "user_TasksInit.h"
// #include "log.h"
// #include "multi_led.h"
// #include "lcd.h"
// #include "lcd_init.h"
// #include "app_sensors.h"

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
// 定义上下文
// void Print_AllSensors(void);

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
  MX_DAC1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_SPI1_Init();
  MX_TIM4_Init();
  MX_I2C2_Init();
  MX_I2C1_Init();
  MX_TIM3_Init();
  MX_TIM17_Init();
  MX_LPTIM1_Init();
  MX_QUADSPI_Init();
  MX_RTC_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */
  User_Tasks_Init();
  vTaskStartScheduler();
  Error_Handler();

#if 0

 
// log_printf("system start");
// HAL_TIM_Base_Start_IT(&htim6); // 启动定时器 6 的中断驱动led
//     LED_Driver_System_Init();
// LED_Object_t led_blue;  // PE3 -> TIM3_CH1
// LED_Object_t led_green; // PE4 -> TIM3_CH2
// LED_Object_t led_red;   // PE5 -> TIM3_CH3
//     LED_Driver_Init(&led_green, LED1_G_GPIO_Port, LED1_G_Pin, &htim4, TIM_CHANNEL_2, 1);
//     LED_Driver_Init(&led_blue, LED1_B_GPIO_Port, LED1_B_Pin, &htim4, TIM_CHANNEL_1, 1);
//     LED_Driver_Init(&led_red, LED1_R_GPIO_Port, LED1_R_Pin, &htim4, TIM_CHANNEL_3, 1);
//     LED_Driver_SendCmd(&led_blue, LED_MODE_PWM, LED_Heartbeat_Handler, 2000, 0, NULL);
//     LED_Driver_SendCmd(&led_green, LED_MODE_PWM, LED_Heartbeat_Handler, 2000, 0, NULL);
//     LED_Driver_SendCmd(&led_red, LED_MODE_PWM, LED_Heartbeat_Handler, 2000, 0, NULL);

// LED_Object_t led2_blue;  // PE3 -> TIM3_CH1
// LED_Object_t led2_green; // PE4 -> TIM3_CH2
// LED_Object_t led2_red;   // PE5 -> TIM3_CH3
//     LED_Driver_Init(&led2_green, LED2_G_GPIO_Port, LED2_G_Pin, &htim3, TIM_CHANNEL_2, 1);
//     LED_Driver_Init(&led2_blue, LED2_B_GPIO_Port, LED2_B_Pin, &htim3, TIM_CHANNEL_1, 1);
//     LED_Driver_Init(&led2_red, LED2_R_GPIO_Port, LED2_R_Pin, &htim3, TIM_CHANNEL_3, 1);
//     LED_Driver_SendCmd(&led2_blue, LED_MODE_PWM, LED_Heartbeat_Handler, 2000, 0, NULL);
//     LED_Driver_SendCmd(&led2_green, LED_MODE_PWM, LED_Heartbeat_Handler, 2000, 0, NULL);
//     LED_Driver_SendCmd(&led2_red, LED_MODE_PWM, LED_Heartbeat_Handler, 2000, 0, NULL);


// HAL_TIM_PWM_Start(&htim17, TIM_CHANNEL_1);//启动风扇
// HAL_Delay(10);
// /* 初始 35% */
// __HAL_TIM_SET_COMPARE(&htim17, TIM_CHANNEL_1, 1120);


LCD_Init();
// 1. 设置背景色为黑色，清屏（可选）
    LCD_Fill(0, 0, LCD_W, LCD_H, BLACK);

    // 2. 定义显示参数
    uint16_t x = 10;      // 起始 X 坐标
    uint16_t y = 10;      // 起始 Y 坐标
    uint16_t font_color = WHITE;  // 字体颜色
    uint16_t bg_color = BLACK;    // 背景颜色
    uint16_t font_size = 16;      // 字体大小（对应 ASCII 字模库）
    uint8_t mode = 0;             // 模式（通常 0 为覆盖模式，1 为叠加模式）

    // 3. 显示英文字符串
    // 注意：确保 LCD_ShowChar 内部正确支持你传入的 sizey
    LCD_ShowString(x, y, "Hello World!", font_color, bg_color, font_size, mode);
    
    // 4. 显示下一行
    LCD_ShowString(x, y + 20, "STM32 Driver", font_color, bg_color, font_size, mode);


// HAL_GPIO_WritePin(FAN_EN_GPIO_Port, FAN_EN_Pin, GPIO_PIN_SET); // 打开风扇


// // 2. 传感器系统初始化
//     if (APP_Sensors_Init() == 0) {
//         log_printf("All sensors initialized successfully!\n");
//     } else {
//         log_printf("Sensor initialization failed!\n");
//     }
// APP_Sensors_Init();

#endif
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
// APP_Sensors_Update();  // 更新传感器数据
// Print_AllSensors();
//     HAL_Delay(2000);


//     __HAL_TIM_SET_COMPARE(&htim17, TIM_CHANNEL_1, 1600); // 50%
//     HAL_Delay(2000);
//     __HAL_TIM_SET_COMPARE(&htim17, TIM_CHANNEL_1, 2000); // 50%
//     HAL_Delay(2000);
//         __HAL_TIM_SET_COMPARE(&htim17, TIM_CHANNEL_1, 2400); // 50%
//     HAL_Delay(2000);
//     __HAL_TIM_SET_COMPARE(&htim17, TIM_CHANNEL_1, 2800); // 90%
//     HAL_Delay(2000);
//     __HAL_TIM_SET_COMPARE(&htim17, TIM_CHANNEL_1, 2400); // 50%
//     HAL_Delay(2000);
//     __HAL_TIM_SET_COMPARE(&htim17, TIM_CHANNEL_1, 2000); // 50%
//     HAL_Delay(2000);
//         __HAL_TIM_SET_COMPARE(&htim17, TIM_CHANNEL_1, 1600); // 50%
//     HAL_Delay(2000);
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

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
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



// /* =========================
//  * Charger 状态解析
//  * ========================= */
// static const char* bq_state_str(bq24295_state_t s)
// {
//     switch (s)
//     {
//         case BQ_CHG_IDLE:         return "IDLE";
//         case BQ_CHG_PRECHARGE:    return "PRECHG";
//         case BQ_CHG_FAST_CHARGE:  return "FAST_CHG";
//         case BQ_CHG_DONE:         return "DONE";
//         case BQ_CHG_SUSPEND:      return "SUSPEND";
//         case BQ_CHG_FAULT:        return "FAULT";
//         default:                  return "UNKNOWN";
//     }
// }

// /* =========================
//  * Charger 打印
//  * ========================= */
// void Print_Charger(bq24295_module_t *m)
// {
//     log_printf("\r\n[CHARGER]\r\n");

//     log_printf("REG08 Status : 0x%02X\r\n", m->reg_status);
//     log_printf("REG09 Fault  : 0x%02X\r\n", m->reg_fault);

//     log_printf("State        : %s\r\n", bq_state_str(m->state));

//     log_printf("Charging     : %s\r\n", m->charging ? "YES" : "NO");
//     log_printf("PowerGood    : %s\r\n", m->power_good ? "YES" : "NO");
//     log_printf("InputPresent : %s\r\n", m->input_present ? "YES" : "NO");

//     log_printf("I_LIMIT      : %d mA\r\n", m->vin_limit_mA);
//     log_printf("I_CHG        : %d mA\r\n", m->ichg_mA);
//     log_printf("V_REG        : %d mV\r\n", m->vreg_mV);

//     log_printf("Fault(Therm) : %s\r\n", m->fault_thermal ? "YES" : "NO");
//     log_printf("Fault(Timer) : %s\r\n", m->fault_timer ? "YES" : "NO");
//     log_printf("Fault(WD)    : %s\r\n", m->fault_watchdog ? "YES" : "NO");
//     log_printf("Fault(Input) : %s\r\n", m->fault_input ? "YES" : "NO");
// }

// /* =========================
//  * INA226
//  * ========================= */
// void Print_INA226(ina226_module_t *m)
// {
//     log_printf("\r\n[INA226]\r\n");
//     log_printf("Voltage : %.2f mV\r\n", m->voltage);
//     log_printf("Current : %.2f mA\r\n", m->current);
//     log_printf("Power   : %.2f mW\r\n", m->power);
// }

// /* =========================
//  * Battery
//  * ========================= */
// void Print_Battery(battery_module_t *m)
// {
//     log_printf("\r\n[BATTERY]\r\n");
//     log_printf("SOC     : %.1f %%\r\n", m->soc);
//     log_printf("Voltage : %.2f V\r\n", m->voltage);
// }

// /* =========================
//  * ENV
//  * ========================= */
// void Print_Env(env_module_t *m)
// {
//     log_printf("\r\n[ENV]\r\n");
//     log_printf("Temp : %.2f C\r\n", m->temp);
//     log_printf("Hum  : %.2f %%\r\n", m->hum);
// }

// /* =========================
//  * Motion
//  * ========================= */
// void Print_Motion(motion_module_t *m)
// {
//     log_printf("\r\n[MOTION]\r\n");
//     log_printf("X:%d Y:%d Z:%d\r\n", m->x, m->y, m->z);
// }

/* =========================
 * 一键总打印
 * ========================= */
// void Print_AllSensors(void)
// {
//     Print_Env(&g_sensors_environment);
//     Print_Motion(&g_sensors_motion);
//     Print_Battery(&g_sensors_battery);
//     Print_INA226(&g_sensors_ina226);
//     Print_Charger(&g_sensors_charger);

//     log_printf("\r\n----------------------\r\n");
// }
/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM6 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */
    // if (htim->Instance == TIM6)
    // {

    //     static uint16_t cnt = 0;
    //     cnt++;
    //     if(cnt >= 10)  // 每 10ms 执行一次
    //     {
    //         cnt = 0;

    //        LED_Driver_Update();;   
    //     }

    // }
  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM6)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

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
