#include "led_app.h"

#include <stddef.h>

#include "FreeRTOS.h"
#include "queue.h"

#include "main.h"
#include "multi_led.h"
#include "tim.h"

static LED_Object_t s_led_blue;  /* PE3 -> TIM4_CH2 */
static LED_Object_t s_led_green; /* PE4 -> TIM4_CH3 */
static LED_Object_t s_led_red;   /* PE5 -> TIM4_CH1 */
static LED_Object_t s_led2_blue;
static LED_Object_t s_led2_green;
static LED_Object_t s_led2_red;

static RGB_Object_t s_rgb_pwr;
static RGB_Object_t s_rgb_sw;

typedef struct
{
    LED_Target_t target;
    LED_EVT_t event;
} LED_App_Command_t;

static QueueHandle_t s_led_command_queue;

static RGB_Object_t *LED_App_GetTarget(LED_Target_t target)
{
    switch (target)
    {
    case LED_TARGET_PWR:
        return &s_rgb_pwr;
    case LED_TARGET_SW:
        return &s_rgb_sw;
    default:
        return NULL;
    }
}

void LED_App_Init(void)
{
    LED_Driver_Init(&s_led_blue, LED1_B_GPIO_Port, LED1_B_Pin, &htim4, TIM_CHANNEL_2, 1U);
    LED_Driver_Init(&s_led_red, LED1_R_GPIO_Port, LED1_R_Pin, &htim4, TIM_CHANNEL_1, 1U);
    LED_Driver_Init(&s_led_green, LED1_G_GPIO_Port, LED1_G_Pin, &htim4, TIM_CHANNEL_3, 1U);

    LED_Driver_Init(&s_led2_blue, LED2_B_GPIO_Port, LED2_B_Pin, &htim3, TIM_CHANNEL_1, 1U);
    LED_Driver_Init(&s_led2_green, LED2_G_GPIO_Port, LED2_G_Pin, &htim3, TIM_CHANNEL_2, 1U);
    LED_Driver_Init(&s_led2_red, LED2_R_GPIO_Port, LED2_R_Pin, &htim3, TIM_CHANNEL_3, 1U);

    RGB_Init(&s_rgb_pwr, &s_led_red, &s_led_green, &s_led_blue);
    RGB_Init(&s_rgb_sw, &s_led2_red, &s_led2_green, &s_led2_blue);
    RGB_Stop(&s_rgb_sw);
    s_led_command_queue = xQueueCreate(8U, sizeof(LED_App_Command_t));
    if (s_led_command_queue == NULL)
    {
        Error_Handler();
    }
    
}

void LED_App_Update(uint32_t dt)
{
    LED_App_Command_t command;

    while (xQueueReceive(s_led_command_queue, &command, 0U) == pdPASS)
    {
        RGB_Object_t *rgb = LED_App_GetTarget(command.target);

        if (rgb == NULL)
        {
            continue;
        }

        switch (command.event)
        {
        case LED_EVT_ON:
            RGB_SendCmd(rgb, RGB_EFFECT_STATIC, 0U, 0U, RGB_COLOR_WHITE, 0U);
            break;
        case LED_EVT_FLASH_BLUE:
            RGB_SendCmd(rgb, RGB_EFFECT_STATIC, 0U, 100U, RGB_COLOR_BLUE, 0U);
            break;
        case LED_EVT_STATIC_RED:
            RGB_SendCmd(rgb, RGB_EFFECT_STATIC, 0U, 0U, RGB_COLOR_RED, 0U);
            break;
        case LED_EVT_STATIC_GREEN:
            RGB_SendCmd(rgb, RGB_EFFECT_STATIC, 0U, 0U, RGB_COLOR_GREEN, 0U);
            break;
        case LED_EVT_STATIC_BLUE:
            RGB_SendCmd(rgb, RGB_EFFECT_STATIC, 0U, 0U, RGB_COLOR_BLUE, 0U);
            break;
        case LED_EVT_BLINK_RED:
            RGB_SendCmd(rgb, RGB_EFFECT_BLINK, 1000U, 0U, RGB_COLOR_RED, 0U);
            break;
        case LED_EVT_BLINK_GREEN:
            RGB_SendCmd(rgb, RGB_EFFECT_BLINK, 1000U, 0U, RGB_COLOR_GREEN, 0U);
            break;
        case LED_EVT_BLINK_BLUE:
            RGB_SendCmd(rgb, RGB_EFFECT_BLINK, 1000U, 0U, RGB_COLOR_BLUE, 0U);
            break;
        case LED_EVT_HEARTBEAT_RED:
            RGB_SendCmd(rgb, RGB_EFFECT_HEARTBEAT, 2000U, 2000U, RGB_COLOR_RED, 0U);
            break;
        case LED_EVT_HEARTBEAT_GREEN:
            RGB_SendCmd(rgb, RGB_EFFECT_HEARTBEAT, 2000U, 2000U, RGB_COLOR_GREEN, 0U);
            break;
        case LED_EVT_HEARTBEAT_BLUE:
            RGB_SendCmd(rgb, RGB_EFFECT_HEARTBEAT, 2000U, 2000U, RGB_COLOR_BLUE, 0U);
            break;
        case LED_EVT_BREATH_RED:
            RGB_SendCmd(rgb, RGB_EFFECT_BREATH, 5000U, 0U, RGB_COLOR_RED, 0U);
            break;
        case LED_EVT_BREATH_GREEN:
            RGB_SendCmd(rgb, RGB_EFFECT_BREATH, 5000U, 0U, RGB_COLOR_GREEN, 0U);
            break;
        case LED_EVT_BREATH_BLUE:
            RGB_SendCmd(rgb, RGB_EFFECT_BREATH, 5000U, 0U, RGB_COLOR_BLUE, 0U);
            break;
        case LED_EVT_RAINBOW:
            RGB_SendCmd(rgb, RGB_EFFECT_RAINBOW, 5000U, 0U, 0U, 0U);
            break;
        case LED_EVT_STOP:
            RGB_Stop(rgb);
            break;
        default:
            break;
        }
    }

    RGB_Update(&s_rgb_pwr, dt);
    RGB_Update(&s_rgb_sw, dt);
}

void LED_CMD_OnEvent(LED_Target_t target, LED_EVT_t event)
{
    LED_App_Command_t command;

    if ((target >= LED_TARGET_COUNT) || (s_led_command_queue == NULL))
    {
        return;
    }

    command.target = target;
    command.event = event;
    if (xQueueSend(s_led_command_queue, &command, 0U) != pdPASS)
    {
        LED_App_Command_t discarded;

        (void)xQueueReceive(s_led_command_queue, &discarded, 0U);
        (void)xQueueSend(s_led_command_queue, &command, 0U);
    }
}
