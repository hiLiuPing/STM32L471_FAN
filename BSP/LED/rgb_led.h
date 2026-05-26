/* rgb_led.h */

#ifndef __RGB_LED_H
#define __RGB_LED_H

#include "multi_led.h"

/* ================= Color Macro ================= */

#define RGB_COLOR(r,g,b) \
    (((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF))

#define RGB_COLOR_RED       RGB_COLOR(255,0,0)
#define RGB_COLOR_GREEN     RGB_COLOR(0,255,0)
#define RGB_COLOR_BLUE      RGB_COLOR(0,0,255)
#define RGB_COLOR_WHITE     RGB_COLOR(255,255,255)
#define RGB_COLOR_YELLOW    RGB_COLOR(255,255,0)
#define RGB_COLOR_CYAN      RGB_COLOR(0,255,255)
#define RGB_COLOR_PURPLE    RGB_COLOR(255,0,255)
#define RGB_COLOR_ORANGE    RGB_COLOR(255,128,0)
#define RGB_COLOR_PINK      RGB_COLOR(255,20,147)
#define RGB_COLOR_OFF       RGB_COLOR(0,0,0)

/* ================= Effect ================= */

typedef enum
{
    RGB_EFFECT_NONE = 0,

    RGB_EFFECT_STATIC,
    RGB_EFFECT_BREATH,
    RGB_EFFECT_RAINBOW,
    RGB_EFFECT_BLINK,
    RGB_EFFECT_HEARTBEAT,

} RGB_Effect_t;

/* ================= Object ================= */

typedef struct
{
    LED_Object_t *r;
    LED_Object_t *g;
    LED_Object_t *b;

    uint16_t r_duty;
    uint16_t g_duty;
    uint16_t b_duty;

    uint32_t color;
    uint32_t color2;

    RGB_Effect_t effect;

    uint32_t period;
    uint32_t counter;

    uint32_t timeout;

    uint8_t enable;

} RGB_Object_t;

/* ================= API ================= */

void RGB_Init(RGB_Object_t *rgb,
              LED_Object_t *r,
              LED_Object_t *g,
              LED_Object_t *b);

void RGB_Update(RGB_Object_t *rgb,
                uint32_t dt);

void RGB_SendCmd(RGB_Object_t *rgb,
                 RGB_Effect_t effect,
                 uint32_t period,
                 uint32_t timeout,
                 uint32_t color,
                 uint32_t color2);

void RGB_Stop(RGB_Object_t *rgb);

void RGB_SetColor(RGB_Object_t *rgb,
                  uint16_t r,
                  uint16_t g,
                  uint16_t b);

void RGB_SetHex(RGB_Object_t *rgb,
                uint32_t color);

void RGB_HSV_To_RGB(uint16_t h,
                    uint8_t s,
                    uint8_t v,
                    uint16_t *r,
                    uint16_t *g,
                    uint16_t *b);

#endif