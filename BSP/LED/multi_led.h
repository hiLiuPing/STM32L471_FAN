#ifndef __MULTI_LED__H
#define __MULTI_LED__H

#include "main.h"


#define USE_FREERTOS 1   

/* ================= Time Macro ================= */
#ifdef USE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#define LED_GET_TICK_MS()  (xTaskGetTickCount() * portTICK_PERIOD_MS)
#else
#define LED_GET_TICK_MS()  HAL_GetTick()
#endif

#define LED_LOGIC_TICK_MS    10   // 逻辑刷新步进，单位ms

typedef enum
{
    LED_MODE_GPIO = 0,
    LED_MODE_PWM
} LED_DriveMode_t;

struct LED_Object;
typedef struct LED_Object LED_Object_t;

typedef void (*LED_Handler_t)(LED_Object_t *obj);

struct LED_Object
{
    GPIO_TypeDef *gpio_port;
    uint16_t gpio_pin;

    TIM_HandleTypeDef *pwm_htim;
    uint32_t pwm_channel;
    uint32_t pwm_arr;

    LED_DriveMode_t drive_mode;

    LED_Handler_t handler;
    LED_Handler_t timeout_handler;

    uint32_t period;
    uint32_t counter;

    uint16_t duty;
    int dir;

    uint32_t timeout;

    uint8_t active_high;      // 1 = 高电平点亮, 0 = 低电平点亮

    struct LED_Object *next;
};

/* ================= API ================= */
void LED_Driver_Init(LED_Object_t *obj,
                     GPIO_TypeDef *gpio_port,
                     uint16_t gpio_pin,
                     TIM_HandleTypeDef *pwm_htim,
                     uint32_t pwm_channel,
                     uint8_t active_high);

void LED_Driver_SendCmd(LED_Object_t *obj,
                        LED_DriveMode_t mode,
                        LED_Handler_t action,
                        uint32_t period,
                        uint32_t timeout,
                        LED_Handler_t next_action);

void LED_Driver_GPIO_Write(LED_Object_t *obj, uint8_t level);
void LED_Driver_GPIO_Toggle(LED_Object_t *obj);
void LED_Driver_PWM_SetDuty(LED_Object_t *obj, uint16_t duty);

// void LED_Driver_System_Init(void);
void LED_Driver_Refresh(uint32_t dt);

void LED_On_Handler(LED_Object_t *obj);
void LED_Off_Handler(LED_Object_t *obj);
void LED_Breath_Handler(LED_Object_t *obj);
void LED_Heartbeat_Handler(LED_Object_t *obj);
void LED_Blink_Handler(LED_Object_t *obj);

/* ================= Helper ================= */
void LED_Driver_Update(void);

#endif