/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define LED2_R_Pin GPIO_PIN_3
#define LED2_R_GPIO_Port GPIOE
#define LED2_G_Pin GPIO_PIN_4
#define LED2_G_GPIO_Port GPIOE
#define LED2_B_Pin GPIO_PIN_5
#define LED2_B_GPIO_Port GPIOE
#define SW_DOWN_Pin GPIO_PIN_6
#define SW_DOWN_GPIO_Port GPIOE
#define SW_UP_Pin GPIO_PIN_13
#define SW_UP_GPIO_Port GPIOC
#define ESP32_EN_Pin GPIO_PIN_2
#define ESP32_EN_GPIO_Port GPIOC
#define FAN_EN_Pin GPIO_PIN_3
#define FAN_EN_GPIO_Port GPIOC
#define PWR_SW_Pin GPIO_PIN_2
#define PWR_SW_GPIO_Port GPIOA
#define SPI1_PWM_Pin GPIO_PIN_6
#define SPI1_PWM_GPIO_Port GPIOA
#define SPI1_DC_Pin GPIO_PIN_4
#define SPI1_DC_GPIO_Port GPIOC
#define SPI1_RST_Pin GPIO_PIN_1
#define SPI1_RST_GPIO_Port GPIOB
#define SPI1_CS_Pin GPIO_PIN_2
#define SPI1_CS_GPIO_Port GPIOB
#define LED1_B_Pin GPIO_PIN_12
#define LED1_B_GPIO_Port GPIOD
#define LED1_G_Pin GPIO_PIN_13
#define LED1_G_GPIO_Port GPIOD
#define LED1_R_Pin GPIO_PIN_14
#define LED1_R_GPIO_Port GPIOD
#define SW_MIDLE_Pin GPIO_PIN_6
#define SW_MIDLE_GPIO_Port GPIOC
#define FAN_PWM_Pin GPIO_PIN_9
#define FAN_PWM_GPIO_Port GPIOB

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
