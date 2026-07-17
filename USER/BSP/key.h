#ifndef __KEY_H__
#define __KEY_H__

#ifdef __cplusplus
extern "C" {
#endif

#pragma once

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

// 时间参数（单位：ms）


#define OS_SUPPORT 1

#define KEY_LONG_THRESHOLD_MS    500 // 长按判定时间
#define KEY_REPEAT_INTERVAL_MS   150 // 连发间隔时间
#define KEY_DEBOUNCE_MS          3   // 消抖时间

/* 掩码工具（位索引即 key_id_t） */
#define KEY_BIT(id)             (1U << (uint8_t)(id))
#define KEY_ALL_MASK            ((uint32_t)((KEY_ID_MAX >= 32) ? 0xFFFFFFFFu : ((1u << KEY_ID_MAX) - 1)))

// #define PWR_SW_Pin GPIO_PIN_2
// #define PWR_SW_GPIO_Port GPIOA
// #define SW_DOWN_Pin GPIO_PIN_6
// #define SW_DOWN_GPIO_Port GPIOE
// #define SW_UP_Pin GPIO_PIN_13
// #define SW_UP_GPIO_Port GPIOC
// #define SW_MIDLE_Pin GPIO_PIN_6
// #define SW_MIDLE_GPIO_Port GPIOC
//KEYP
#define KEYB_PORT	PWR_SW_GPIO_Port
#define KEYB_PIN	PWR_SW_Pin

//KEYU
#define KEYN_PORT	SW_UP_GPIO_Port
#define KEYN_PIN	SW_UP_Pin

//KEYM
#define KEYL_PORT	SW_MIDLE_GPIO_Port
#define KEYL_PIN	SW_MIDLE_Pin

//KEYD
#define KEYR_PORT	SW_DOWN_GPIO_Port
#define KEYR_PIN	SW_DOWN_Pin




// //KEYY
// #define KEYY_PORT	GPIOA
// #define KEYY_PIN	GPIO_PIN_1
// KEY_ID_PWR   →  电源键
// KEY_ID_OK   →  中键
// KEY_ID_DOWN   →  下键
// KEY_ID_UP   →  上键
/**
 * @brief 按键ID（与掩码位索引一致）
 */
typedef enum {
    KEY_ID_PWR = 0,  // KEYB
    // KEY_ID_Y,      // KEYY
    KEY_ID_OK,      // KEYL
    KEY_ID_DOWN,      // KEYR
    KEY_ID_UP,      // KEYN
    KEY_ID_MAX
} key_id_t;

/**
 * @brief 事件类型
 */
typedef enum {
    KEY_EVT_NONE    = 0,
    KEY_EVT_DOWN    = 1,   // 去抖后稳定按下
    KEY_EVT_UP      = 2,   // 去抖后稳定释放
    KEY_EVT_CLICK   = 3,   // 短按（一次按下->释放，且未触发长按）
    KEY_EVT_LONG    = 4,   // 达到长按阈值触发一次
    KEY_EVT_REPEAT  = 5    // 长按连发周期事件
} key_evt_type_t;

/**
 * @brief 事件对象
 */
typedef struct {
    key_id_t        id;
    key_evt_type_t  type;
} key_event_t;

/**
 * @brief 扫描模式（可用位或"|"组合）
 */
typedef enum {
    KEY_MODE_REPORT_DOWN_UP   = 0x01,   // 上报 DOWN/UP
    KEY_MODE_CLICK_ON_RELEASE = 0x02,   // 释放时若未长按，上报 CLICK
    KEY_MODE_LONG_ONCE        = 0x04,   // 达到阈值上报一次 LONG
    KEY_MODE_REPEAT           = 0x08    // 达到阈值后周期性上报 REPEAT
} key_mode_t;

/**
 * @brief 时序参数（单位：ms），可运行时修改
 */
typedef struct {
    uint16_t debounce_ms;        // 消抖时间，建议 3~20
    uint16_t long_ms;            // 长按判定时间，建议 400~800
    uint16_t repeat_ms;          // 连发间隔，建议 80~200
} key_timing_t;


void Key_Init(void);
bool Key_Scan(key_event_t* out_evt);
void Key_SetMode(uint8_t mode_flags);
uint8_t Key_GetMode(void);
void Key_SetLongEnableMask(uint32_t mask);
uint32_t Key_GetLongEnableMask(void);
uint32_t Key_GetPressedMask(void);
uint32_t Key_GetRawMask(void);
void Key_SetTiming(const key_timing_t* timing);

#ifdef __cplusplus
}
#endif
#endif
