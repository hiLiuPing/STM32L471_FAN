/* ================= rgb_led.c ================= */

#include "rgb_led.h"
#include <math.h>

/* ================= Gamma ================= */

static uint16_t rgb_gamma_table[1001];

static void RGB_Gamma_Init(void)
{
    for (int i = 0; i <= 1000; i++)
    {
        float x = (float)i / 1000.0f;

        float y = powf(x, 2.2f);

        rgb_gamma_table[i] = (uint16_t)(y * 1000.0f);
    }
}

/* ================= Internal ================= */

static uint16_t rgb_breath_curve(uint16_t x)
{
    return (x * x) / 1000;
}

static void rgb_hex_to_rgb(uint32_t color,
                           uint16_t *r,
                           uint16_t *g,
                           uint16_t *b)
{
    uint8_t rr;
    uint8_t gg;
    uint8_t bb;

    rr = (color >> 16) & 0xFF;
    gg = (color >> 8) & 0xFF;
    bb = color & 0xFF;

    *r = rr * 1000 / 255;
    *g = gg * 1000 / 255;
    *b = bb * 1000 / 255;
}

/* ================= Init ================= */

void RGB_Init(RGB_Object_t *rgb,
              LED_Object_t *r,
              LED_Object_t *g,
              LED_Object_t *b)
{
    static uint8_t gamma_init = 0;

    if (!gamma_init)
    {
        gamma_init = 1;

        RGB_Gamma_Init();
    }

    if (!rgb) return;

    rgb->r = r;
    rgb->g = g;
    rgb->b = b;

    rgb->r_duty = 0;
    rgb->g_duty = 0;
    rgb->b_duty = 0;

    /* ================= Gain ================= */
    /* 根据你现在硬件:
       R = 2.2k
       G = 1k
       B = 1k
    */

    rgb->r_gain = 900;
    rgb->g_gain = 1000;
    rgb->b_gain = 1000;

    rgb->color = RGB_COLOR_OFF;
    rgb->color2 = RGB_COLOR_OFF;

    rgb->effect = RGB_EFFECT_NONE;

    rgb->period = 1000;
    rgb->counter = 0;

    rgb->timeout = 0;

    rgb->enable = 0;
}

/* ================= Basic ================= */

void RGB_SetColor(RGB_Object_t *rgb,
                  uint16_t r,
                  uint16_t g,
                  uint16_t b)
{
    if (!rgb) return;

    if (r > 1000) r = 1000;
    if (g > 1000) g = 1000;
    if (b > 1000) b = 1000;

    /* ================= Gain ================= */

    r = (r * rgb->r_gain) / 1000;
    g = (g * rgb->g_gain) / 1000;
    b = (b * rgb->b_gain) / 1000;

    if (r > 1000) r = 1000;
    if (g > 1000) g = 1000;
    if (b > 1000) b = 1000;

    /* ================= Gamma ================= */

    r = rgb_gamma_table[r];
    g = rgb_gamma_table[g];
    b = rgb_gamma_table[b];

    rgb->r_duty = r;
    rgb->g_duty = g;
    rgb->b_duty = b;

    LED_Driver_PWM_SetDuty(rgb->r, r);
    LED_Driver_PWM_SetDuty(rgb->g, g);
    LED_Driver_PWM_SetDuty(rgb->b, b);
}

void RGB_SetHex(RGB_Object_t *rgb,
                uint32_t color)
{
    uint16_t r;
    uint16_t g;
    uint16_t b;

    rgb_hex_to_rgb(color, &r, &g, &b);

    RGB_SetColor(rgb, r, g, b);
}

void RGB_Stop(RGB_Object_t *rgb)
{
    if (!rgb) return;

    rgb->enable = 0;

    rgb->effect = RGB_EFFECT_NONE;

    RGB_SetHex(rgb, RGB_COLOR_OFF);
}

/* ================= HSV ================= */

void RGB_HSV_To_RGB(uint16_t h,
                    uint8_t s,
                    uint8_t v,
                    uint16_t *r,
                    uint16_t *g,
                    uint16_t *b)
{
    uint8_t region;
    uint16_t remainder;

    uint8_t p, q, t;

    if (s == 0)
    {
        *r = *g = *b = v * 1000 / 255;
        return;
    }

    h = h % 360;

    region = h / 60;

    remainder = (h - (region * 60)) * 255 / 60;

    p = (v * (255 - s)) >> 8;
    q = (v * (255 - ((s * remainder) >> 8))) >> 8;
    t = (v * (255 - ((s * (255 - remainder)) >> 8))) >> 8;

    uint8_t rr, gg, bb;

    switch(region)
    {
        default:
        case 0:
            rr = v;
            gg = t;
            bb = p;
            break;

        case 1:
            rr = q;
            gg = v;
            bb = p;
            break;

        case 2:
            rr = p;
            gg = v;
            bb = t;
            break;

        case 3:
            rr = p;
            gg = q;
            bb = v;
            break;

        case 4:
            rr = t;
            gg = p;
            bb = v;
            break;

        case 5:
            rr = v;
            gg = p;
            bb = q;
            break;
    }

    *r = rr * 1000 / 255;
    *g = gg * 1000 / 255;
    *b = bb * 1000 / 255;
}

/* ================= SendCmd ================= */

void RGB_SendCmd(RGB_Object_t *rgb,
                 RGB_Effect_t effect,
                 uint32_t period,
                 uint32_t timeout,
                 uint32_t color,
                 uint32_t color2)
{
    if (!rgb) return;

    rgb->effect = effect;

    rgb->period = period;
    rgb->timeout = timeout;

    rgb->color = color;
    rgb->color2 = color2;

    rgb->counter = 0;

    rgb->enable = 1;

    switch(effect)
    {
        case RGB_EFFECT_STATIC:
        {
            RGB_SetHex(rgb, color);
        } break;

        default:
            break;
    }
}

/* ================= Update ================= */

void RGB_Update(RGB_Object_t *rgb,
                uint32_t dt)
{
    if (!rgb) return;

    if (!rgb->enable) return;

    rgb->counter += dt;

    if (rgb->timeout)
    {
        if (rgb->timeout > dt)
        {
            rgb->timeout -= dt;
        }
        else
        {
            RGB_Stop(rgb);
            return;
        }
    }

    if (rgb->period)
    {
        if (rgb->counter >= rgb->period)
        {
            rgb->counter = 0;
        }
    }

    switch(rgb->effect)
    {
        case RGB_EFFECT_RAINBOW:
        {
            uint16_t h;

            uint16_t r;
            uint16_t g;
            uint16_t b;

            h = (rgb->counter * 360) / rgb->period;

            RGB_HSV_To_RGB(h,
                           255,
                           255,
                           &r,
                           &g,
                           &b);

            RGB_SetColor(rgb, r, g, b);

        } break;

        case RGB_EFFECT_BREATH:
        {
            uint16_t base_r;
            uint16_t base_g;
            uint16_t base_b;

            uint32_t half;

            uint32_t duty;

            rgb_hex_to_rgb(rgb->color,
                           &base_r,
                           &base_g,
                           &base_b);

            half = rgb->period / 2;

            if (rgb->counter < half)
            {
                duty = (rgb->counter * 1000) / half;
            }
            else
            {
                duty = ((rgb->period - rgb->counter) * 1000) / half;
            }

            duty = rgb_breath_curve(duty);

            RGB_SetColor(rgb,
                         (base_r * duty) / 1000,
                         (base_g * duty) / 1000,
                         (base_b * duty) / 1000);

        } break;

        case RGB_EFFECT_BLINK:
        {
            if (rgb->counter < (rgb->period / 2))
            {
                RGB_SetHex(rgb, rgb->color);
            }
            else
            {
                RGB_SetHex(rgb, RGB_COLOR_OFF);
            }

        } break;

        case RGB_EFFECT_HEARTBEAT:
        {
            if (rgb->counter < 80)
            {
                RGB_SetHex(rgb, rgb->color);
            }
            else if (rgb->counter < 160)
            {
                RGB_SetHex(rgb, RGB_COLOR_OFF);
            }
            else if (rgb->counter < 240)
            {
                RGB_SetHex(rgb, rgb->color);
            }
            else
            {
                RGB_SetHex(rgb, RGB_COLOR_OFF);
            }

        } break;

        default:
            break;
    }
}