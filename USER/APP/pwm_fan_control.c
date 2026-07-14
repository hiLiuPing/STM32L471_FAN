/**
 * @file pwm_fan_control.c
 * @brief 智能桌面 PWM 风扇控制系统核心算法实现
 * @version 1.0
 * @date 2026-07-14
 */

#include "pwm_fan_control.h"
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/* 基础限幅宏 */
#define CLAMP(val, min, max) ((val) < (min) ? (min) : ((val) > (max) ? (max) : (val)))

/* 硬件无关的轻量级线性同余生成器 (LCG) 伪随机数算法，确保在各种MCU上产生相同的平滑随机质感 */
static uint32_t fan_lcg_seed = 123456789U;
static float fan_random_float(void) {
    fan_lcg_seed = fan_lcg_seed * 1664525U + 1013904223U;
    return (float)(fan_lcg_seed & 0xFFFFFF) / 16777216.0f; /* 生成 0.0 到 1.0 之间的均匀浮点数 */
}

void Fan_Init(FanController_t *fan) {
    if (!fan) return;

    /* 1. 设置出厂默认的智能温控线 */
    fan->config.temp_min = 24.0f;
    fan->config.temp_max = 32.0f;
    fan->config.min_base_duty = 25.0f;  /* 24度以下温柔微风 */
    fan->config.max_base_duty = 85.0f;  /* 32度以上强劲冷风 */

    /* 2. 气象引擎参数：主波周期约 ~200s (0.015rad/s)，次波周期约 ~60s (0.05rad/s) */
    fan->config.weather_amp_1 = 10.0f; 
    fan->config.weather_freq_1 = 0.015f; 
    fan->config.weather_amp_2 = 4.0f;
    fan->config.weather_freq_2 = 0.05f;

    /* 3. 湍流引擎参数：高频微幅波动 */
    fan->config.turb_amplitude = 5.0f;  /* 最大扰动 ±5% */
    fan->config.turb_alpha = 0.35f;     /* 低通滤波系数，值越小随机风越柔和越有粉红噪声质感 */

    /* 4. 阵风引擎参数：突发大风事件 */
    fan->config.gust_trigger_prob = 400; // 1/400 概率，按100ms周期算，平均约40秒触发一次
    fan->config.gust_amp_min = 15.0f;    // 阵风最少突加 15% 占空比
    fan->config.gust_amp_max = 30.0f;    // 阵风最大突加 30% 占空比
    fan->config.gust_rise_ticks = 15;    // 1.5 秒内起风
    fan->config.gust_sustain_ticks = 20; // 高峰维持 2.0 秒
    fan->config.gust_fall_ticks = 40;    // 4.0 秒缓慢回落

    /* 5. 硬件保护及斜率参数 */
    fan->config.slew_rate_limit = 1.2f;   // 每 100ms 最大允许改变 1.2% (即 12%/s 的变化率)
    fan->config.absolute_min_duty = 15.0f; // 硬件防卡死下限
    fan->config.absolute_max_duty = 100.0f;// 硬件上限

    /* 6. 重置运行状态机 */
    fan->mode = FAN_MODE_NORMAL;
    fan->user_base_speed = 50.0f;
    fan->active_base = 50.0f;
    fan->current_duty = 0.0f;
    
    fan->weather_timer = 0.0f;
    fan->turb_raw_state = 0.0f;
    fan->turb_filtered = 0.0f;
    fan->turb_hold_counter = 0;
    
    fan->gust_state = GUST_STATE_IDLE;
    fan->gust_target_amp = 0.0f;
    fan->gust_ticks = 0;
}

void Fan_Configure(FanController_t *fan, const FanConfig_t *config) {
    if (fan && config) {
        fan->config = *config;
    }
}

void Fan_SetTarget(FanController_t *fan, FanMode_t mode, float base_speed) {
    if (!fan) return;
    fan->mode = mode;
    fan->user_base_speed = CLAMP(base_speed, 0.0f, 100.0f);
}

float Fan_Process_Tick_100ms(FanController_t *fan, float ambient_temp) {
    if (!fan) return 0.0f;

    /* -------------------------------------------------------------------------
     * 步骤 1: 确定当前时间的基准风速 (Base Speed)
     * ------------------------------------------------------------------------- */
    if (fan->mode == FAN_MODE_SMART) {
        /* 智能温控风：根据当前实时环境温度线性插值计算基准 */
        if (ambient_temp <= fan->config.temp_min) {
            fan->active_base = fan->config.min_base_duty;
        } else if (ambient_temp >= fan->config.temp_max) {
            fan->active_base = fan->config.max_base_duty;
        } else {
            float ratio = (ambient_temp - fan->config.temp_min) / (fan->config.temp_max - fan->config.temp_min);
            fan->active_base = fan->config.min_base_duty + ratio * (fan->config.max_base_duty - fan->config.min_base_duty);
        }
    } else {
        /* 普通模式与自然风模式下，直接采用用户设定的固定基准速度 */
        fan->active_base = fan->user_base_speed;
    }

    /* 步骤 2: 如果是普通模式或智能模式（不需要叠加自然风规律），直接通过斜率限制器追踪并返回 */
    if (fan->mode != FAN_MODE_NATURAL) {
        float slew_step = fan->config.slew_rate_limit;
        fan->current_duty += CLAMP(fan->active_base - fan->current_duty, -slew_step, slew_step);
        fan->current_duty = CLAMP(fan->current_duty, fan->config.absolute_min_duty, fan->config.absolute_max_duty);
        return fan->current_duty;
    }

    /* =========================================================================
     * 自然风核心多维合成引擎 (NATURAL WIND SYNTHESIS ENGINE)
     * ========================================================================= */

    /* 引擎子层 A: 气象趋势引擎 (Weather Engine) - 依靠长周期波形模拟宏观大风趋势 */
    fan->weather_timer += 0.1f; 
    float weather_offset = (fan->config.weather_amp_1 * sinf(fan->config.weather_freq_1 * fan->weather_timer)) +
                           (fan->config.weather_amp_2 * cosf(fan->config.weather_freq_2 * fan->weather_timer));

    /* 引擎子层 B: 湍流引擎 (Turbulence Engine) - 采用一阶低通滞后滤波逼近 1/f 粉红噪声 */
    if (fan->turb_hold_counter == 0) {
        /* 生成一个居中于 0.0f，幅度在 [-turb_amplitude, +turb_amplitude] 之间的白噪声 */
        float raw_noise = (fan_random_float() * 2.0f - 1.0f) * fan->config.turb_amplitude;
        /* 通过低通滤波把高频刺耳突变过滤为大自然中树叶沙沙声般的微幅湍流 */
        fan->turb_filtered = (fan->config.turb_alpha * raw_noise) + ((1.0f - fan->config.turb_alpha) * fan->turb_filtered);
        fan->turb_hold_counter = 5; // 保持此强度 500ms，防止每个 100ms 都在无规律乱抖破坏体感
    } else {
        fan->turb_hold_counter--;
    }

    /* 引擎子层 C: 阵风引擎状态机 (Gust Engine) */
    float gust_offset = 0.0f;
    switch (fan->gust_state) {
        case GUST_STATE_IDLE:
            /* 摇号触发阵风 */
            if ((uint32_t)(fan_random_float() * fan->config.gust_trigger_prob) == 0) {
                fan->gust_state = GUST_STATE_RISING;
                fan->gust_target_amp = fan->config.gust_amp_min + 
                    (fan_random_float() * (fan->config.gust_amp_max - fan->config.gust_amp_min));
                fan->gust_ticks = 0;
            }
            break;

        case GUST_STATE_RISING:
            fan->gust_ticks++;
            gust_offset = fan->gust_target_amp * ((float)fan->gust_ticks / (float)fan->config.gust_rise_ticks);
            if (fan->gust_ticks >= fan->config.gust_rise_ticks) {
                fan->gust_state = GUST_STATE_SUSTAIN;
                fan->gust_ticks = 0;
            }
            break;

        case GUST_STATE_SUSTAIN:
            fan->gust_ticks++;
            gust_offset = fan->gust_target_amp;
            if (fan->gust_ticks >= fan->config.gust_sustain_ticks) {
                fan->gust_state = GUST_STATE_FALLING;
                fan->gust_ticks = 0;
            }
            break;

        case GUST_STATE_FALLING:
            fan->gust_ticks++;
            float decay_ratio = 1.0f - ((float)fan->gust_ticks / (float)fan->config.gust_fall_ticks);
            gust_offset = fan->gust_target_amp * decay_ratio;
            if (fan->gust_ticks >= fan->config.gust_fall_ticks) {
                fan->gust_state = GUST_STATE_IDLE;
                fan->gust_ticks = 0;
            }
            break;
    }

    /* -------------------------------------------------------------------------
     * 步骤 3: 最终多维度叠加 (Target Duty) 并施加边界限制
     * ------------------------------------------------------------------------- */
    float target_duty = fan->active_base + weather_offset + fan->turb_filtered + gust_offset;
    
    /* 进斜率限制器之前，先切除可能会导致硬件过载或停机的不安全占空比 */
    target_duty = CLAMP(target_duty, fan->config.absolute_min_duty, fan->config.absolute_max_duty);

    /* -------------------------------------------------------------------------
     * 步骤 4: 斜率限制器 (Slew Limiter) 物理滤波
     * ------------------------------------------------------------------------- */
    float max_slew = fan->config.slew_rate_limit;
    fan->current_duty += CLAMP(target_duty - fan->current_duty, -max_slew, max_slew);

    /* 冗余二次安全校验 */
    fan->current_duty = CLAMP(fan->current_duty, fan->config.absolute_min_duty, fan->config.absolute_max_duty);

    return fan->current_duty;
}