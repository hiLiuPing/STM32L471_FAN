#include "sht40_reg.h"
#include "FreeRTOS.h"
#include "task.h"
#include "main.h" // 获取 HAL_Delay (如果裸机) 或任务延时函数

 uint8_t SHT40_CRC8(uint8_t *data, uint8_t len) {
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


// 1. 负责底层的通信、CRC校验和获取原始数据
int32_t sht40_read_raw(sht40_ctx_t *ctx, uint16_t *rawT, uint16_t *rawH) {
    uint8_t cmd = SHT40_MEAS_HIGH_PRECISION;
    uint8_t rx[6];

    if(ctx->write_reg(ctx->handle, 0, &cmd, 1) != 0) return -1;

    // 2. 等待测量完成 (High Precision 典型耗时 10ms)
    // 调度器运行时用 vTaskDelay 让出 CPU，避免 HAL_Delay 忙等 10ms
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING)
    {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    else
    {
        HAL_Delay(10);
    }

    if(ctx->read_reg(ctx->handle, 0, rx, 6) != 0) return -1;
    if(SHT40_CRC8(&rx[0], 2) != rx[2] || SHT40_CRC8(&rx[3], 2) != rx[5]) return -2;

    *rawT = (rx[0] << 8) | rx[1];
    *rawH = (rx[3] << 8) | rx[4];
    return 0;
}
// 2. 两个独立的解析函数
float sht40_convert_temp(uint16_t rawT) {
    return -45.0f + 175.0f * ((float)rawT / 65535.0f);
}

float sht40_convert_hum(uint16_t rawH) {
    return -6.0f + 125.0f * ((float)rawH / 65535.0f);
}
int32_t sht40_soft_reset(sht40_ctx_t *ctx) {
    uint8_t cmd = SHT40_SOFT_RESET;
    return ctx->write_reg(ctx->handle, 0, &cmd, 1);
}
