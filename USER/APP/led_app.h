#ifndef __LED_APP_H__
#define __LED_APP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "rgb_led.h"

typedef enum {
    LED_EVT_NONE = 0,
    LED_EVT_ON,
    LED_EVT_FLASH_BLUE,
    LED_EVT_STATIC_RED,
    LED_EVT_STATIC_GREEN,
    LED_EVT_STATIC_BLUE,
    LED_EVT_BLINK_RED,
    LED_EVT_BLINK_GREEN,
    LED_EVT_BLINK_BLUE,
    LED_EVT_HEARTBEAT_RED,
    LED_EVT_HEARTBEAT_GREEN,
    LED_EVT_HEARTBEAT_BLUE,
    LED_EVT_BREATH_RED,
    LED_EVT_BREATH_GREEN,
    LED_EVT_BREATH_BLUE,
    LED_EVT_RAINBOW,
    LED_EVT_STOP,
} LED_EVT_t;

typedef enum {
    LED_TARGET_PWR = 0,
    LED_TARGET_SW,
    LED_TARGET_COUNT
} LED_Target_t;

void LED_App_Init(void);
void LED_App_Update(uint32_t dt);
void LED_CMD_OnEvent(LED_Target_t target, LED_EVT_t event);

#ifdef __cplusplus
}
#endif

#endif /* __LED_APP_H__ */
