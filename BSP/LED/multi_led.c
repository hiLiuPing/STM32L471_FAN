#include "multi_led.h"
#include <string.h>

static LED_Object_t* g_led_head = NULL;
static uint32_t g_led_last_tick = 0;

/* ================= GPIO (PWM模拟优先) ================= */
void LED_Driver_GPIO_Write(LED_Object_t *obj, uint8_t level)
{
    if (!obj) return;

    // 根据 active_high 自动翻转逻辑
    uint8_t out_level = obj->active_high ? level : !level;

    if (obj->pwm_htim)
    {
        LED_Driver_PWM_SetDuty(obj, level ? 1000 : 0); // PWM 仍用原 duty，不翻转
        return;
    }

    HAL_GPIO_WritePin(obj->gpio_port,
                      obj->gpio_pin,
                      out_level ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void LED_Driver_GPIO_Toggle(LED_Object_t *obj)
{
    if (!obj) return;

    if (obj->pwm_htim)
    {
        uint16_t next = obj->duty ? 0 : 1000;
        LED_Driver_PWM_SetDuty(obj, next);
        return;
    }

    HAL_GPIO_TogglePin(obj->gpio_port, obj->gpio_pin);
}

/* ================= PWM ================= */
void LED_Driver_PWM_SetDuty(LED_Object_t *obj, uint16_t duty)
{
    if (!obj || !obj->pwm_htim) return;

    if (duty > 1000) duty = 1000;

    // 如果 active_high = 0（低电平点亮），翻转占空比
    obj->duty = obj->active_high ? duty : (1000 - duty);

    uint32_t ccr = (obj->pwm_arr * obj->duty) / 1000;

    __HAL_TIM_SET_COMPARE(obj->pwm_htim,
                          obj->pwm_channel,
                          ccr);
}

/* ================= Init ================= */
void LED_Driver_Init(LED_Object_t *obj,
                     GPIO_TypeDef *gpio_port,
                     uint16_t gpio_pin,
                     TIM_HandleTypeDef *pwm_htim,
                     uint32_t pwm_channel,
                     uint8_t active_high)
{
    if (!obj) return;

    memset(obj, 0, sizeof(LED_Object_t));

    obj->gpio_port   = gpio_port;
    obj->gpio_pin    = gpio_pin;
    obj->pwm_htim    = pwm_htim;
    obj->pwm_channel = pwm_channel;
    obj->dir = 1;
    obj->active_high = active_high;

    if (pwm_htim)
    {
        obj->drive_mode = LED_MODE_PWM;
        HAL_TIM_PWM_Start(pwm_htim, pwm_channel);
        obj->pwm_arr = __HAL_TIM_GET_AUTORELOAD(pwm_htim);
    }
    else
    {
        obj->drive_mode = LED_MODE_GPIO;
    }

    obj->next = g_led_head;
    g_led_head = obj;
}

/* ================= SendCmd ================= */
void LED_Driver_SendCmd(LED_Object_t *obj,
                        LED_DriveMode_t mode,
                        LED_Handler_t action,
                        uint32_t period,
                        uint32_t timeout,
                        LED_Handler_t next_action)
{
    if (!obj || !action) return;

    if (obj->drive_mode == mode && 
        obj->handler == action && 
        obj->period == period)
    {
        obj->timeout = timeout;
        return; 
    }

    obj->drive_mode = mode;
    obj->handler = action;
    obj->period  = period;
    obj->timeout = timeout;
    obj->timeout_handler = next_action;

    obj->counter = 0;
    obj->duty = 0;
    obj->dir = 1;

    action(obj);
}

/* ================= Refresh ================= */
void LED_Driver_Refresh(uint32_t dt)
{
    LED_Object_t* curr = g_led_head;

    while (curr)
    {
        if (curr->handler)
        {
            curr->counter += dt;
            curr->handler(curr);
        }

        if (curr->timeout)
        {
            if (curr->timeout > dt)
                curr->timeout -= dt;
            else
            {
                curr->timeout = 0;
                if (curr->timeout_handler)
                {
                    curr->handler = curr->timeout_handler;
                    curr->handler(curr);
                }
                else
                {
                    curr->handler = NULL; 
                    LED_Driver_PWM_SetDuty(curr, 0); 
                }
                curr->counter = 0;
                curr->duty = 0;
                curr->dir = 1;
            }
        }

        curr = curr->next;
    }
}

/* ================= System Init ================= */
void LED_Driver_System_Init(void)
{
    g_led_head = NULL;
    g_led_last_tick = LED_GET_TICK_MS();
}

/* ================= Update ================= */
void LED_Driver_Update(void)
{
    uint32_t now = LED_GET_TICK_MS();
    uint32_t dt = now - g_led_last_tick;

    if (dt >= LED_LOGIC_TICK_MS) 
    {
        g_led_last_tick += LED_LOGIC_TICK_MS; 
        LED_Driver_Refresh(LED_LOGIC_TICK_MS); 
    }
}

/* ================= Handlers ================= */
void LED_On_Handler(LED_Object_t *obj)
{
    if(!obj) return;
    LED_Driver_GPIO_Write(obj, 1);
    if(obj->pwm_htim) LED_Driver_PWM_SetDuty(obj, 1000);
    obj->handler = NULL; 
}

void LED_Off_Handler(LED_Object_t *obj)
{
    if(!obj) return;
    LED_Driver_GPIO_Write(obj, 0);
    if(obj->pwm_htim) LED_Driver_PWM_SetDuty(obj, 0);
    obj->handler = NULL; 
}

void LED_Breath_Handler(LED_Object_t *obj)
{
    if(!obj || obj->period == 0) return;
    if (obj->counter >= obj->period) obj->counter = 0;

    uint32_t half = obj->period / 2;
    uint32_t linear_duty;

    if (obj->counter < half)
        linear_duty = (obj->counter * 1000) / half;
    else
        linear_duty = ((obj->period - obj->counter) * 1000) / half;

    uint32_t smooth_duty = (linear_duty * linear_duty) / 1000;

    LED_Driver_PWM_SetDuty(obj, smooth_duty);
}

void LED_Heartbeat_Handler(LED_Object_t *obj)
{
    if(!obj) return;
    uint32_t total_period = obj->period; 
    uint32_t bright_time = 50; 

    if (obj->counter >= total_period)
        obj->counter = 0;

    if (obj->counter < bright_time)
        LED_Driver_GPIO_Write(obj, 1); 
    else
        LED_Driver_GPIO_Write(obj, 0);
}

void LED_Blink_Handler(LED_Object_t *obj)
{
    if(!obj || obj->period == 0) return;

    if (obj->counter >= obj->period)
        obj->counter = 0;

    if (obj->counter < (obj->period / 2))
        LED_Driver_GPIO_Write(obj, 1); 
    else
        LED_Driver_GPIO_Write(obj, 0);
}