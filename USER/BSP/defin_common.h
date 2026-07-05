#ifndef __DEFIN_COMMON_H__
#define __DEFIN_COMMON_H__

#include "stm32l4xx_hal.h"
typedef enum app_run_state_t {
    MUSIC_ON = 0,     // 音乐开启状态
    DISPLAY_ON,       // 显示开启状态（值自动为1）
    // 后续可扩展其他状态，比如 MUSIC_OFF、DISPLAY_OFF 等
} app_run_state_t;

extern app_run_state_t g_app_current_state; // 声明全局状态变量


typedef struct
{
    float alpha;
    float state;
    uint8_t initialized;
} LPF_t;

void LPF_Init(LPF_t *f, float alpha);
float LPF_Update(LPF_t *f, float input);

float IIR_Filter(float prev, float new_val, float alpha);
#endif /* __KEY_H__ */
