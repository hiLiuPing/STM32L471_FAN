#include "user_HardwareInitTask.h"

#include "app_sensors.h"
#include "key.h"
#include "log.h"
#include "main.h"
#include "multi_led.h"
#include "rgb_led.h"
#include "tim.h"

#include "usart.h"
#include "user_TasksInit.h"

LED_Object_t led2_blue;
LED_Object_t led2_green;
LED_Object_t led2_red;
LED_Object_t led_blue;
LED_Object_t led_green;
LED_Object_t led_red;

RGB_Object_t rgb;

static void HardwareInitTask_InitLeds(void)
{
    LED_Driver_Init(&led_blue, LED1_B_GPIO_Port, LED1_B_Pin, &htim4, TIM_CHANNEL_2, 1U);
    LED_Driver_Init(&led_red, LED1_R_GPIO_Port, LED1_R_Pin, &htim4, TIM_CHANNEL_1, 1U);
    LED_Driver_Init(&led_green, LED1_G_GPIO_Port, LED1_G_Pin, &htim4, TIM_CHANNEL_3, 1U);

    LED_Driver_Init(&led2_blue, LED2_B_GPIO_Port, LED2_B_Pin, &htim3, TIM_CHANNEL_1, 1U);
    LED_Driver_Init(&led2_green, LED2_G_GPIO_Port, LED2_G_Pin, &htim3, TIM_CHANNEL_2, 1U);
    LED_Driver_Init(&led2_red, LED2_R_GPIO_Port, LED2_R_Pin, &htim3, TIM_CHANNEL_3, 1U);

    LED_Driver_SendCmd(&led2_blue, LED_MODE_PWM, LED_Heartbeat_Handler, 2000U, 0U, NULL);
    LED_Driver_SendCmd(&led2_green, LED_MODE_PWM, LED_Heartbeat_Handler, 2000U, 0U, NULL);
    LED_Driver_SendCmd(&led2_red, LED_MODE_PWM, LED_Heartbeat_Handler, 2000U, 0U, NULL);

    RGB_Init(&rgb, &led_red, &led_green, &led_blue);
    RGB_SendCmd(&rgb, RGB_EFFECT_RAINBOW, 5000U, 0U, 0U, 0U);
}

void HardwareInitTask(void *argument)
{
    (void)argument;

    log_init(&huart1);

    log_printf("start app");
log_printf("step1: init leds...");
HardwareInitTask_InitLeds();
log_printf("step2: key init...");
Key_Init();
log_printf("step3: ui task create...");
// configASSERT(UI_Task_Create() == pdPASS);
// log_printf("step4: hw ready");
User_Tasks_SetHardwareReady();
log_printf("step5: delete self");
vTaskDelete(NULL);
}
