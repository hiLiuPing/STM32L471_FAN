/* Private includes -----------------------------------------------------------*/

// includes
// sys
#include "tim.h"
// #include "adc.h"
// #include "i2c.h"
#include "usart.h"
// user
#include "user_TasksInit.h"

// bsp
#include "key.h"
#include "log.h"
#include "multi_led.h"
#include "rgb_led.h"
// #include "lcd.h"
// #include "lcd_init.h"
// #include "gate.h"
// #include "fusb302_dev.h"
// #include "BL24C02.h" // settings

// ui
// #include "lvgl.h"
// #include "lv_port_disp.h"
// #include "ui.h"
LED_Object_t led2_blue;  // PE3 -> TIM3_CH1
LED_Object_t led2_green; // PE4 -> TIM3_CH2
LED_Object_t led2_red;   // PE5 -> TIM3_CH3
LED_Object_t led_blue;  // PE3 -> TIM3_CH1
LED_Object_t led_green; // PE4 -> TIM3_CH2
LED_Object_t led_red;   // PE5 -> TIM3_CH3


RGB_Object_t rgb;

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/


/**
  * @brief  hardwares init task
  * @param  argument: Not used
  * @retval None
  */
void HardwareInitTask(void *argument)
{
	// 挂起所有任务，保证初始化不被打断
	// vTaskSuspendAll();
    log_init(&huart1);

// LED init
    LED_Driver_Init(&led_blue, LED1_B_GPIO_Port, LED1_B_Pin, &htim4, TIM_CHANNEL_2, 1);
    LED_Driver_Init(&led_red, LED1_R_GPIO_Port, LED1_R_Pin, &htim4, TIM_CHANNEL_1, 1);
    LED_Driver_Init(&led_green, LED1_G_GPIO_Port, LED1_G_Pin, &htim4, TIM_CHANNEL_3, 1);

    LED_Driver_Init(&led2_green, LED2_G_GPIO_Port, LED2_G_Pin, &htim3, TIM_CHANNEL_2, 1);
    LED_Driver_Init(&led2_blue, LED2_B_GPIO_Port, LED2_B_Pin, &htim3, TIM_CHANNEL_1, 1);
    LED_Driver_Init(&led2_red, LED2_R_GPIO_Port, LED2_R_Pin, &htim3, TIM_CHANNEL_3, 1);
    LED_Driver_SendCmd(&led2_blue, LED_MODE_PWM, LED_Heartbeat_Handler, 2000, 0, NULL);
    LED_Driver_SendCmd(&led2_green, LED_MODE_PWM, LED_Heartbeat_Handler, 2000, 0, NULL);
    LED_Driver_SendCmd(&led2_red, LED_MODE_PWM, LED_Heartbeat_Handler, 2000, 0, NULL);


RGB_Init(&rgb,
         &led_red,
         &led_green,
         &led_blue);

RGB_SendCmd(&rgb,
            RGB_EFFECT_RAINBOW,
            5000,
            0,
            0,
            0);

// // RGB_SendCmd(&rgb,
// //             RGB_EFFECT_HEARTBEAT,
// //             1000,
// //             0,
// //             RGB_COLOR_GREEN,
// //             0);
// // RGB_SendCmd(&rgb,
// //             RGB_EFFECT_BREATH,
// //             2000,
// //             0,
// //             RGB_COLOR_BLUE,
// //             0);
// RGB_SendCmd(&rgb,
//             RGB_EFFECT_BLINK,
//             500,
//             0,
//             RGB_COLOR_RED,
//             0);									


	Key_Init();



	// 恢复任务调度
	// xTaskResumeAll();

	// 自己删除自己
	vTaskDelete(NULL);
}