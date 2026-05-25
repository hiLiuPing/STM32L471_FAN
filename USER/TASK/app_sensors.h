/* app_sensors.h */

#ifndef __APP_SENSORS_H
#define __APP_SENSORS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "i2c_bus.h"

#include "sht40_reg.h"
#include "lis3dh_reg.h"
#include "bq24295_reg.h"
#include "max17048_reg.h"
#include "ina226_reg.h"


/* =========================
 * 模块结构体
 * ========================= */
typedef enum
{
    BQ_CHG_IDLE = 0,
    BQ_CHG_PRECHARGE,
    BQ_CHG_FAST_CHARGE,
    BQ_CHG_DONE,
    BQ_CHG_SUSPEND,
    BQ_CHG_FAULT
} bq24295_state_t;
/* BQ24295 */
typedef struct
{
    /* ===== 原始寄存器 ===== */
    uint8_t reg_status;     // REG08
    uint8_t reg_fault;      // REG09

    /* ===== 解码后的状态（给UI/逻辑用） ===== */
    bq24295_state_t state;  // 状态机
    bool charging;          // 是否充电中
    bool power_good;        // 输入电源是否正常
    bool input_present;     // 是否插入电源

    /* ===== 电源参数（关键控制量） ===== */
    uint16_t vin_limit_mA;      // 输入限流
    uint16_t ichg_mA;           // 充电电流
    uint16_t vreg_mV;           // 充电电压

    /* ===== 故障标志（从 fault 解码） ===== */
    bool fault_thermal;     // 过温
    bool fault_timer;       // 安全定时器
    bool fault_watchdog;    // 看门狗
    bool fault_input;       // 输入异常

    /* ===== 驱动上下文 ===== */
    bq24295_ctx_t ctx;

} bq24295_module_t;


/* INA226 */
typedef struct
{
    float current;

    float voltage;

    float power;

    ina226_ctx_t ctx;

} ina226_module_t;


/* MAX17048 */
typedef struct
{
    float soc;

    float voltage;

    max17048_ctx_t ctx;

} battery_module_t;


/* SHT40 */
typedef struct
{
    float temp;

    float hum;

    sht40_ctx_t ctx;

} env_module_t;


/* LIS3DH */
typedef struct
{
    int16_t x;

    int16_t y;

    int16_t z;

    lis3dh_ctx_t ctx;

} motion_module_t;


/* =========================
 * 全局模块实例
 * ========================= */

extern bq24295_module_t g_sensors_charger;

extern ina226_module_t g_sensors_ina226;

extern battery_module_t g_sensors_battery;

extern env_module_t g_sensors_environment;

extern motion_module_t g_sensors_motion;


/* =========================
 * APP 接口
 * ========================= */

int32_t APP_Sensors_Init(void);

int32_t APP_Sensors_Update(void);


/* =========================
 * 单独更新接口
 * ========================= */

void Update_Env(env_module_t *m);

void Update_Motion(motion_module_t *m);

void Update_Battery(battery_module_t *m);

void Update_Charger(bq24295_module_t *m);

void Update_INA226(ina226_module_t *m);


#ifdef __cplusplus
}
#endif

#endif