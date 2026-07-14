/**
 * @file pwm_fan_control.h
 * @brief 智能桌面 PWM 风扇控制系统 (支持自然风/智能风/普通风模式)
 * @version 1.0
 * @date 2026-07-14
 * 
 * @note 本实现内含完整的多层气流调制引擎：
 *       1. Base Speed: 用户指定或根据环境温度动态映射。
 *       2. Weather Engine: 通过双正弦波叠加模拟 20~60 秒的长周期宏观趋势。
 *       3. Turbulence Engine: 利用一阶低通滤波模拟 1~5 秒的 1/f 粉红噪声高频湍流。
 *       4. Gust Engine: 状态机驱动的随机短时爆发阵风。
 *       5. Slew Limiter: 斜率限制器，消除转速突变带来的电机电磁声及过载。
 */

#ifndef PWM_FAN_CONTROL_H
#define PWM_FAN_CONTROL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 风扇运行模式枚举
 */
typedef enum {
    PWM_FAN_MODE_NORMAL = 0,   ///< 普通模式：固定追踪用户设定的基础风速
    PWM_FAN_MODE_SMART,        ///< 智能风模式：根据当前环境温度动态调整基础风速
    PWM_FAN_MODE_NATURAL       ///< 自然风模式：在基础风速上叠加气象、湍流、阵风三大引擎
} PwmFanMode_t;

/**
 * @brief 阵风引擎内部状态机
 */
typedef enum {
    GUST_STATE_IDLE = 0,   ///< 空闲：等待随机概率触发
    GUST_STATE_RISING,     ///< 飙升：风速在短时间内线性急剧上升
    GUST_STATE_SUSTAIN,    ///< 维持：保持在阵风最高峰值
    GUST_STATE_FALLING     ///< 衰减：阵风逐渐消散，柔和回落
} GustState_t;

/**
 * @brief 风扇算法可配置参数结构体
 */
typedef struct {
    float temp_min;         ///< 智能风温控下限 (°C)，低于此温度运行在最小基础占空比
    float temp_max;         ///< 智能风温控上限 (°C)，高于此温度运行在最大基础占空比
    float min_base_duty;    ///< 智能风温控对应最小占空比 (%) (通常 20.0f)
    float max_base_duty;    ///< 智能风温控对应最大占空比 (%) (通常 90.0f)
    
    float weather_amp_1;    ///< 气象引擎主波幅 (%)
    float weather_freq_1;   ///< 气象引擎主波频率 (rad/s)
    float weather_amp_2;    ///< 气象引擎次波幅 (%)
    float weather_freq_2;   ///< 气象引擎次波频率 (rad/s)
    
    float turb_amplitude;   ///< 湍流引擎最大抖动幅度 (%) (通常 3.0f ~ 5.0f)
    float turb_alpha;       ///< 粉红噪声一阶滞后滤波器系数 (0.0 到 1.0 之间)
    
    uint16_t gust_trigger_prob; ///< 阵风每 tick 触发的逆概率 (例如500代表每tick有1/500几率触发)
    float gust_amp_min;     ///< 阵风触发的最小额外占空比增量 (%)
    float gust_amp_max;     ///< 阵风触发的最大额外占空比增量 (%)
    uint16_t gust_rise_ticks;   ///< 阵风前沿上升周期 (ticks)
    uint16_t gust_sustain_ticks; ///< 阵风顶峰维持周期 (ticks)
    uint16_t gust_fall_ticks;   ///< 阵风后沿回落消散周期 (ticks)
    
    float slew_rate_limit;  ///< 斜率限制器每 tick 最大允许的变化量 (%/tick)
    float absolute_min_duty; ///< 硬件底层安全最小占空比，防止风扇电机卡死停转 (%)
    float absolute_max_duty; ///< 硬件底层安全最大占空比限制 (%)
} PwmFanConfig_t;

/**
 * @brief 风扇控制器运行期核心上下文结构体
 */
typedef struct {
    PwmFanConfig_t config;     ///< 风扇算法配置参数
    
    PwmFanMode_t mode;         ///< 当前风扇运行模式
    float user_base_speed;  ///< 用户输入的原始基础风速 (0.0 到 100.0)
    float active_base;      ///< 当前计算生效的基础风速 (考虑了温控映射后)
    float current_duty;     ///< 当前实际输出的 PWM 占空比 (0.0 到 100.0)
    
    float weather_timer;    ///< 气象引擎的时间轴累加器
    float turb_raw_state;   ///< 湍流引擎上一次生成的随机数原始状态
    float turb_filtered;    ///< 湍流一阶滤波后的当前结果值
    uint16_t turb_hold_counter; ///< 湍流更新频率计数器 (降低其调变频率增强质感)
    
    GustState_t gust_state; ///< 阵风引擎当前所处的内部状态
    float gust_target_amp;  ///< 当前这次阵风决定的最大峰值强度
    uint16_t gust_ticks;    ///< 阵风当前阶段的步进计数器
} PwmFanController_t;

/**
 * @brief 初始化风扇控制器参数（赋予出厂默认的自然风舒适参数）
 * @param fan 指向 PwmFanController_t 结构体的指针
 */
void PwmFan_Init(PwmFanController_t *fan);

/**
 * @brief 自定义或覆写风扇运行参数
 * @param fan 指向 PwmFanController_t 结构体的指针
 * @param config 用户自定义的配置结构体
 */
void PwmFan_Configure(PwmFanController_t *fan, const PwmFanConfig_t *config);

/**
 * @brief 更改风扇的运行模式与目标基准风速
 * @param fan 指向 PwmFanController_t 结构体的指针
 * @param mode 期望设置的目标模式
 * @param base_speed 期望的目标基础风速 (0.0f - 100.0f)，用于普通/自然风模式下
 */
void PwmFan_SetTarget(PwmFanController_t *fan, PwmFanMode_t mode, float base_speed);

/**
 * @brief 风扇算法核心定时处理函数 (必须在定时器中严格周期性调用，推荐每 100ms 一次)
 * @param fan 指向 PwmFanController_t 结构体的指针
 * @param ambient_temp 当前由温湿度传感器读取到的真实环境温度 (°C)
 * @return float 返回当前步进计算完毕后的实际 PWM 占空比 (0.0f 到 100.0f)，直接写入硬件 PWM 寄存器
 */
float PwmFan_ProcessTick100ms(PwmFanController_t *fan, float ambient_temp);

#ifdef __cplusplus
}
#endif

#endif // PWM_FAN_CONTROL_H
