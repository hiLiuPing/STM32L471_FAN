#include "sht40_reg.h"
#include "main.h" // 获取 HAL_Delay (如果裸机) 或任务延时函数

static uint8_t SHT40_CRC8(uint8_t *data, uint8_t len) {
    uint8_t crc = 0xFF;
    for(uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for(uint8_t bit = 0; bit < 8; bit++) {
            if(crc & 0x80) crc = (crc << 1) ^ 0x31;
            else crc <<= 1;
        }
    }
    return crc;
}

int32_t sht40_read_data(sht40_ctx_t *ctx, SHT40_Data_t *data) {
    uint8_t cmd = SHT40_MEAS_HIGH_PRECISION;
    uint8_t rx[6];

    // 1. 发送测量命令 (SHT40 不需要写寄存器地址，直接发命令即可)
    if(ctx->write_reg(ctx->handle, 0, &cmd, 1) != 0) return -1;

    // 2. 等待测量完成 (High Precision 典型耗时 10ms)
    // 根据你的环境，此处可以兼容使用 vTaskDelay 或 HAL_Delay
    #ifdef USE_FREERTOS
        vTaskDelay(pdMS_TO_TICKS(10));
    #else
        HAL_Delay(10);
    #endif

    // 3. 读取数据
    if(ctx->read_reg(ctx->handle, 0, rx, 6) != 0) return -1;

    // 4. CRC 校验
    if(SHT40_CRC8(&rx[0], 2) != rx[2] || SHT40_CRC8(&rx[3], 2) != rx[5]) return -2;

    // 5. 数据转换
    uint16_t rawT = (rx[0] << 8) | rx[1];
    uint16_t rawH = (rx[3] << 8) | rx[4];
    
    data->temperature = -45.0f + 175.0f * ((float)rawT / 65535.0f);
    data->humidity    = -6.0f + 125.0f * ((float)rawH / 65535.0f);

    return 0;
}

int32_t sht40_soft_reset(sht40_ctx_t *ctx) {
    uint8_t cmd = SHT40_SOFT_RESET;
    return ctx->write_reg(ctx->handle, 0, &cmd, 1);
}