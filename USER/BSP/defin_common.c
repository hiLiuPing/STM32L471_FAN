#include "defin_common.h"

// 定义全局状态变量（可选，用于记录当前应用状态）
app_run_state_t g_app_current_state = DISPLAY_ON;


void LPF_Init(LPF_t *f, float alpha)
{
    f->alpha = alpha;
    f->initialized = 0;
}

float LPF_Update(LPF_t *f, float input)
{
    if(!f->initialized)
    {
        f->state = input;
        f->initialized = 1;
        return input;
    }

    f->state =
        f->alpha * input +
        (1.0f - f->alpha) * f->state;

    return f->state;
}
float IIR_Filter(float prev, float new_val, float alpha)
{
    return prev * alpha + new_val * (1.0f - alpha);
}
