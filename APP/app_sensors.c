#include "app_sensors.h"
#include "i2c.h"

/* =========================================================
 * 通用 I2C 设备
 * ========================================================= */
typedef struct
{
    I2C_Bus_t *bus;
    uint16_t dev_addr;   // 7bit 地址（统一，不移位）
} generic_i2c_t;


/* =========================================================
 * MEM I2C 读写（INA / MAX / BQ / LIS3DH）
 * ========================================================= */
static int32_t generic_i2c_write(
    void *handle,
    uint8_t reg,
    const uint8_t *data,
    uint16_t len)
{
    generic_i2c_t *dev = (generic_i2c_t *)handle;

    return I2C_Mem_Write(
        dev->bus,
        dev->dev_addr,
        reg,
        (uint8_t *)data,
        len);
}

static int32_t generic_i2c_read(
    void *handle,
    uint8_t reg,
    uint8_t *data,
    uint16_t len)
{
    generic_i2c_t *dev = (generic_i2c_t *)handle;

    return I2C_Mem_Read(
        dev->bus,
        dev->dev_addr,
        reg,
        data,
        len);
}


/* =========================================================
 * SHT40 专用（COMMAND MODE）
 * ========================================================= */
static int32_t sht40_write_cmd(generic_i2c_t *dev, uint8_t cmd)
{
    return I2C_Write(dev->bus,
                     dev->dev_addr,
                     &cmd,
                     1);
}

static int32_t sht40_read_data(generic_i2c_t *dev,
                                uint8_t *buf,
                                uint16_t len)
{
    return I2C_Read(dev->bus,
                    dev->dev_addr,
                    buf,
                    len);
}


/* =========================================================
 * I2C BUS
 * ========================================================= */
I2C_Bus_t i2c_bus_1 = { .hi2c = &hi2c1 };
I2C_Bus_t i2c_bus_2 = { .hi2c = &hi2c2 };


/* =========================================================
 * DEVICE LIST（统一 7bit 地址）
 * ========================================================= */
static generic_i2c_t dev_sht40 = {
    .bus = &i2c_bus_2,
    .dev_addr = SHT40_ADDR,
};

static generic_i2c_t dev_lis3dh = {
    .bus = &i2c_bus_1,
    .dev_addr = LIS3DH_ADDR,
};

static generic_i2c_t dev_max17048 = {
    .bus = &i2c_bus_2,
    .dev_addr = MAX17048_ADDR,
};

static generic_i2c_t dev_bq24295 = {
    .bus = &i2c_bus_2,
    .dev_addr = BQ24295_ADDR,
};

static generic_i2c_t dev_ina226 = {
    .bus = &i2c_bus_2,
    .dev_addr = INA226_ADDR,
};


/* =========================================================
 * 全局模块
 * ========================================================= */
bq24295_module_t g_sensors_charger;
ina226_module_t g_sensors_ina226;
battery_module_t g_sensors_battery;
env_module_t g_sensors_environment;
motion_module_t g_sensors_motion;


/* =========================================================
 * LIS3DH init
 * ========================================================= */
static void lis3dh_manual_init(lis3dh_ctx_t *ctx)
{
    uint8_t data;

    data = 0x47;
    ctx->write_reg(ctx->handle, LIS3DH_CTRL_REG1, &data, 1);

    data = 0x00;
    ctx->write_reg(ctx->handle, LIS3DH_CTRL_REG4, &data, 1);
}


/* =========================================================
 * ENV INIT (SHT40)
 * ========================================================= */
static void Init_Env(env_module_t *m)
{
    m->ctx.handle = &dev_sht40;
    m->ctx.write_reg = NULL;
    m->ctx.read_reg  = NULL;
}


/* =========================================================
 * MOTION INIT
 * ========================================================= */
static void Init_Motion(motion_module_t *m)
{
    m->ctx.handle = &dev_lis3dh;
    m->ctx.write_reg = generic_i2c_write;
    m->ctx.read_reg  = generic_i2c_read;

    lis3dh_manual_init(&m->ctx);
}


/* =========================================================
 * BATTERY INIT
 * ========================================================= */
static void Init_Battery(battery_module_t *m)
{
    m->ctx.handle = &dev_max17048;
    m->ctx.write_reg = generic_i2c_write;
    m->ctx.read_reg  = generic_i2c_read;

    max17048_init(&m->ctx);
}


/* =========================================================
 * CHARGER INIT
 * ========================================================= */
static void Init_Charger(bq24295_module_t *m)
{
    m->ctx.handle = &dev_bq24295;
    m->ctx.write_reg = generic_i2c_write;
    m->ctx.read_reg  = generic_i2c_read;

    bq24295_init(&m->ctx);
}


/* =========================================================
 * INA226 INIT
 * ========================================================= */
static void Init_INA226(ina226_module_t *m)
{
    m->ctx.handle = &dev_ina226;
    m->ctx.write_reg = generic_i2c_write;
    m->ctx.read_reg  = generic_i2c_read;

    /* ===== 硬件参数（必须正确） ===== */
    m->ctx.r_shunt = 0.01f;   // 10mΩ（按你板子）

    /* ===== 选择一种模式 ===== */

    /* 模式1：推荐（自动计算） */
    ina226_init(&m->ctx);
}

/* =========================================================
 * ENV UPDATE (SHT40)
 * ========================================================= */
void Update_Env(env_module_t *m)
{
    uint8_t cmd = SHT40_MEAS_HIGH_PRECISION;
    uint8_t rx[6];

    generic_i2c_t *dev = (generic_i2c_t *)m->ctx.handle;

    if (sht40_write_cmd(dev, cmd) != 0)
        return;

    HAL_Delay(10);

    if (sht40_read_data(dev, rx, 6) != 0)
        return;

    if (SHT40_CRC8(&rx[0], 2) != rx[2]) return;
    if (SHT40_CRC8(&rx[3], 2) != rx[5]) return;

    uint16_t t = (rx[0] << 8) | rx[1];
    uint16_t h = (rx[3] << 8) | rx[4];

    m->temp = sht40_convert_temp(t);
    m->hum  = sht40_convert_hum(h);
}


/* =========================================================
 * MOTION UPDATE
 * ========================================================= */
void Update_Motion(motion_module_t *m)
{
    int16_t raw[3];

    if (lis3dh_acceleration_raw_get(&m->ctx, raw) == 0)
    {
        m->x = raw[0];
        m->y = raw[1];
        m->z = raw[2];
    }
}


/* =========================================================
 * BATTERY UPDATE
 * ========================================================= */
void Update_Battery(battery_module_t *m)
{
    float soc, voltage;

    if (max17048_get_soc(&m->ctx, &soc) == 0)
        m->soc = soc;

    if (max17048_get_vcell(&m->ctx, &voltage) == 0)
        m->voltage = voltage / 1000.0f;
}


/* =========================================================
 * CHARGER UPDATE
 * ========================================================= */
void Update_Charger(bq24295_module_t *m)
{
    uint8_t status;
    uint8_t fault;

    /* 1. 读状态 */
    if (m->ctx.read_reg(m->ctx.handle, BQ_REG08_STATUS, &status, 1) != 0)
        return;

    if (m->ctx.read_reg(m->ctx.handle, BQ_REG09_FAULT, &fault, 1) != 0)
        return;

    m->reg_status = status;
    m->reg_fault  = fault;

    /* 2. 输入状态 */
    uint8_t vbus = (status & 0xC0);
    m->input_present = (vbus != 0);

    /* 3. 状态机 */
    uint8_t chg = (status & 0x30);

    switch (chg)
    {
        case 0x10:
            m->state = BQ_CHG_PRECHARGE;
            m->charging = true;
            break;

        case 0x20:
            m->state = BQ_CHG_FAST_CHARGE;
            m->charging = true;
            break;

        case 0x30:
            m->state = BQ_CHG_DONE;
            m->charging = false;
            break;

        default:
            m->state = BQ_CHG_IDLE;
            m->charging = false;
            break;
    }

    /* 4. fault解析 */
    m->fault_thermal  = (fault & (1 << 0));
    m->fault_timer    = (fault & (1 << 1));
    m->fault_watchdog = (fault & (1 << 2));
    m->fault_input    = (fault & (1 << 3));
}



// void Print_Charger(const bq24295_module_t *m)
// {
//     printf("\n[CHARGER]\n");

//     /* 原始寄存器 */
//     printf("REG08 Status : 0x%02X\n", m->reg_status);
//     printf("REG09 Fault  : 0x%02X\n", m->reg_fault);

//     /* 状态机 */
//     const char *state_str = "";

//     switch (m->state)
//     {
//         case BQ_CHG_IDLE:        state_str = "IDLE"; break;
//         case BQ_CHG_PRECHARGE:   state_str = "PRECHG"; break;
//         case BQ_CHG_FAST_CHARGE: state_str = "FAST_CHG"; break;
//         case BQ_CHG_DONE:        state_str = "DONE"; break;
//         case BQ_CHG_SUSPEND:     state_str = "SUSPEND"; break;
//         case BQ_CHG_FAULT:       state_str = "FAULT"; break;
//         default:                 state_str = "UNKNOWN"; break;
//     }

//     printf("State        : %s\n", state_str);
//     printf("Charging     : %s\n", m->charging ? "YES" : "NO");
//     printf("Input Present: %s\n", m->input_present ? "YES" : "NO");

//     /* Fault 解码 */
//     if (m->fault_thermal)
//         printf("Fault        : THERMAL\n");

//     if (m->fault_timer)
//         printf("Fault        : TIMER\n");

//     if (m->fault_watchdog)
//         printf("Fault        : WATCHDOG\n");

//     if (m->fault_input)
//         printf("Fault        : INPUT\n");

//     if (!m->fault_thermal &&
//         !m->fault_timer &&
//         !m->fault_watchdog &&
//         !m->fault_input)
//     {
//         printf("Fault        : NONE\n");
//     }
// }



/* =========================================================
 * INA226 UPDATE
 * ========================================================= */
void Update_INA226(ina226_module_t *m)
{
    uint8_t buf[2];
    uint16_t raw_u;
    int16_t raw_i;

    /* ===== BUS VOLT ===== */
    if (m->ctx.read_reg(m->ctx.handle, INA226_REG_BUS_VOLT, buf, 2) == 0)
    {
        raw_u = ((uint16_t)buf[0] << 8) | buf[1];

        /* 1.25mV LSB */
        m->voltage = raw_u * 1.25f;
    }

    /* ===== CURRENT ===== */
    if (m->ctx.read_reg(m->ctx.handle, INA226_REG_CURRENT, buf, 2) == 0)
    {
        raw_i = (int16_t)((buf[0] << 8) | buf[1]);

        /* ⭐关键修复 */
        m->current = raw_i * m->ctx.current_lsb;
    }

    /* ===== POWER（建议用芯片寄存器） */
    if (m->ctx.read_reg(m->ctx.handle, INA226_REG_POWER, buf, 2) == 0)
    {
        uint16_t raw_p = ((uint16_t)buf[0] << 8) | buf[1];

        m->power = raw_p * (25.0f * m->ctx.current_lsb);
    }
    else
    {
        m->power = m->voltage * m->current;
    }
}







/* =========================================================
 * APP INIT
 * ========================================================= */
int32_t APP_Sensors_Init(void)
{
    I2C_Bus_Init(&i2c_bus_1);
    I2C_Bus_Init(&i2c_bus_2);

    Init_Env(&g_sensors_environment);
    Init_Motion(&g_sensors_motion);
    Init_Battery(&g_sensors_battery);
    Init_Charger(&g_sensors_charger);
    Init_INA226(&g_sensors_ina226);

    return 0;
}


/* =========================================================
 * APP UPDATE
 * ========================================================= */
int32_t APP_Sensors_Update(void)
{
    Update_Env(&g_sensors_environment);
    Update_Motion(&g_sensors_motion);
    Update_Battery(&g_sensors_battery);
    Update_Charger(&g_sensors_charger);
    Update_INA226(&g_sensors_ina226);

    return 0;
}