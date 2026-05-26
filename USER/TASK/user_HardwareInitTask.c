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


    LED_Driver_Init(&led_green, LED1_G_GPIO_Port, LED1_G_Pin, &htim4, TIM_CHANNEL_2, 1);
    LED_Driver_Init(&led_blue, LED1_B_GPIO_Port, LED1_B_Pin, &htim4, TIM_CHANNEL_1, 1);
    LED_Driver_Init(&led_red, LED1_R_GPIO_Port, LED1_R_Pin, &htim4, TIM_CHANNEL_3, 1);

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
	// gate, for current flow route selection, high current by default
	// Gate_Port_Init();
	// flow_route_selection(HIGH_CUR);

	// usb init
	// MX_USB_DEVICE_Init();

	// ADC sample start
	// HAL_ADC_Start_DMA(&hadc1, (uint32_t *)adc_raw_buffer, ADC_TIMES*2 * ADC_CHANNELS); /*启动ADC的DMA传输，配合定时器触发ADC转换  12位的ADC对应0-4095 */
	// HAL_TIM_Base_Start(&htim2); /*开启定时器，用溢出时间来触发ADC*/

	// PWM Start for LCD backlight
	// HAL_TIM_PWM_Start(&htim1,TIM_CHANNEL_3);

	// beep
	// HAL_TIM_PWM_Start(&htim4,TIM_CHANNEL_4);

	// key
	Key_Init();

	// system settings from eeprom
	// if(!EEPROM_Init_Check()) {
	// 	EEPROM_SysSetting_Get();
	// }
	// Gate_Set_Mode(Sys_Get_CurrentRangeMode());

	// FUSB CC pin dis connect
	// HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET);

	// FUSB302 init
	// fusb302_dev_init();

	// lcd
	// done in lvgl disp init

	// tim5 for elapsed time (us)
	// HAL_TIM_Base_Start(&htim5);

	// ui
	// LVGL and disp init
	// lv_init();
	// lv_port_disp_init();
	// ui_init();

	// 恢复任务调度
	// xTaskResumeAll();

	// 自己删除自己
	vTaskDelete(NULL);
}