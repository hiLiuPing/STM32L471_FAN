#include "led_app.h"

#include <stddef.h>

#include "main.h"
#include "multi_led.h"
#include "tim.h"

LED_Object_t led_blue;  // PE3 -> TIM3_CH1
LED_Object_t led_green; // PE4 -> TIM3_CH2
LED_Object_t led_red;   // PE5 -> TIM3_CH3
LED_Object_t led2_blue;
LED_Object_t led2_green;
LED_Object_t led2_red;

RGB_Object_t rgb;

void LED_App_Init(void)
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

void LED_CMD_OnEvent(LED_EVT_t event)
{

    switch (event)
    {
    case LED_EVT_ON:
        RGB_SendCmd(&rgb, RGB_EFFECT_STATIC, 0, 0, RGB_COLOR_WHITE, 0);
        break;
  case LED_EVT_STATIC_RED:
        RGB_SendCmd(&rgb, RGB_EFFECT_STATIC, 0, 0, RGB_COLOR_RED, 0);
        break;
    case LED_EVT_STATIC_GREEN:
        RGB_SendCmd(&rgb, RGB_EFFECT_STATIC, 0, 0, RGB_COLOR_GREEN, 0);
        break;
    case LED_EVT_STATIC_BLUE:
        RGB_SendCmd(&rgb, RGB_EFFECT_STATIC, 0, 0, RGB_COLOR_BLUE, 0);
        break;
    case LED_EVT_BLINK_RED:
        RGB_SendCmd(&rgb, RGB_EFFECT_BLINK, 1000, 0, RGB_COLOR_RED, 0);
        break;
    case LED_EVT_BLINK_GREEN:
        RGB_SendCmd(&rgb, RGB_EFFECT_BLINK, 1000, 0, RGB_COLOR_GREEN, 0);
        break;
    case LED_EVT_BLINK_BLUE:
        RGB_SendCmd(&rgb, RGB_EFFECT_BLINK, 1000, 0, RGB_COLOR_BLUE, 0);
        break;
    case LED_EVT_HEARTBEAT_RED:
        RGB_SendCmd(&rgb, RGB_EFFECT_HEARTBEAT, 2000, 2000, RGB_COLOR_RED, 0);
        break;
    case LED_EVT_HEARTBEAT_GREEN:

      RGB_SendCmd(&rgb, RGB_EFFECT_HEARTBEAT, 2000, 2000, RGB_COLOR_GREEN, 0);
        break;
    case LED_EVT_HEARTBEAT_BLUE:
        RGB_SendCmd(&rgb, RGB_EFFECT_HEARTBEAT, 2000, 2000, RGB_COLOR_BLUE, 0);
        break;
     case LED_EVT_BREATH_RED:
        RGB_SendCmd(&rgb, RGB_EFFECT_BREATH, 5000, 0, RGB_COLOR_RED, 0);
        break;
    case LED_EVT_BREATH_GREEN:
        RGB_SendCmd(&rgb, RGB_EFFECT_BREATH, 5000, 0, RGB_COLOR_GREEN, 0);
        break;
    case LED_EVT_BREATH_BLUE:
        RGB_SendCmd(&rgb, RGB_EFFECT_BREATH, 5000, 0, RGB_COLOR_BLUE, 0);
        break;
    case LED_EVT_RAINBOW:
       RGB_SendCmd(&rgb, RGB_EFFECT_RAINBOW, 5000, 0, 0, 0);
        break;
    case LED_EVT_STOP:
        RGB_Stop(&rgb);
        break;

    default:
        break;
    }
}
