#ifndef __QSPI_PSRAM_H__
#define __QSPI_PSRAM_H__

/*
 * QSPI PSRAM 驱动（仿 spi_flash 模式）。
 * 芯片：ESP-PSRAM64H（8MB / 64Mbit，QPI 模式）。
 */

#include "main.h"
#include <stdint.h>
#include "quadspi.h"

/* ================= OS SWITCH ================= */
#define PSRAM_USE_FREERTOS   1

#if PSRAM_USE_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#endif

/* ================= CMD (QPI 模式) ================= */
#define PSRAM_CMD_RESET_EN      0x66
#define PSRAM_CMD_RESET         0x99
#define PSRAM_CMD_ENTER_QPI     0x35
#define PSRAM_CMD_READ_ID       0x9F
#define PSRAM_CMD_READ          0xEB
#define PSRAM_CMD_WRITE         0x38

/* ================= chip size ================= */
#define PSRAM_SIZE              (8 * 1024 * 1024)   /* 8 MB */
#define PSRAM_PAGE_SIZE         1024
#define PSRAM_ID_SIZE           8U

/* ================= context ================= */
typedef struct
{
    QSPI_HandleTypeDef *hqspi;

    uint32_t psram_size;

    /* geometry */
    uint32_t page_size;
    uint32_t total_size;
    uint8_t  addr_len;

    uint8_t  qpi_mode;          /* 1 = QPI mode active */
    uint8_t  mmap_active;       /* 1 = memory-mapped mode active */
    uint8_t  initialized;
    uint8_t  id_valid;
    uint8_t  id[PSRAM_ID_SIZE];

#if PSRAM_USE_FREERTOS
    SemaphoreHandle_t lock;
#endif

} qspi_psram_t;

/* ================= API ================= */
int qspi_psram_init(qspi_psram_t *p, QSPI_HandleTypeDef *hqspi);
int qspi_psram_read(qspi_psram_t *p, uint32_t addr, uint8_t *buf, uint32_t len);
int qspi_psram_write(qspi_psram_t *p, uint32_t addr, const uint8_t *buf, uint32_t len);
int qspi_psram_enable_memory_mapped(qspi_psram_t *p);
int qspi_psram_exit_memory_mapped(qspi_psram_t *p);
int qspi_psram_recover(qspi_psram_t *p);
int qspi_psram_sync(qspi_psram_t *p);
/* Returns the ID captured in 1-line SPI mode before qspi_psram_init() enters QPI. */
int qspi_psram_read_id(qspi_psram_t *p, uint8_t *id, uint32_t len);
void qspi_psram_log_id(qspi_psram_t *p);

/* 测试函数：写入测试数据 → 进入 MMAP → 读出并比对打印。 */
void qspi_psram_test(qspi_psram_t *p);

/* MMAP 基地址（L476 的 QUADSPI 映射区）。 */
#define PSRAM_MMAP_BASE         ((volatile uint8_t *)0x90000000)

#endif /* __QSPI_PSRAM_H__ */
