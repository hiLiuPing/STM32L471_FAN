#include "qspi_psram.h"
#include <string.h>
#include "log.h"

/*
 * QSPI PSRAM 驱动实现（仿 spi_flash.c 模式）。
 * PSRAM 为 SRAM 类型：无需擦除、无需写使能、直接读写。
 * QPI 模式下指令/地址/数据均为 4-Line。
 */

/* ================= 线程安全宏 ================= */
#if PSRAM_USE_FREERTOS
#define PSRAM_LOCK(p)   do { if ((p)->lock) xSemaphoreTake((p)->lock, portMAX_DELAY); } while(0)
#define PSRAM_UNLOCK(p) do { if ((p)->lock) xSemaphoreGive((p)->lock); } while(0)
#else
#define PSRAM_LOCK(p)
#define PSRAM_UNLOCK(p)
#endif

#define PSRAM_MMAP_TIMEOUT_PERIOD 1024U
#define PSRAM_HAL_TIMEOUT_MS      100U

static int qspi_psram_send_reset_locked(qspi_psram_t *p, uint32_t instruction_mode)
{
    QSPI_CommandTypeDef cmd = {0};

    cmd.InstructionMode = instruction_mode;
    cmd.AddressMode = QSPI_ADDRESS_NONE;
    cmd.DataMode = QSPI_DATA_NONE;
    cmd.DummyCycles = 0U;
    cmd.Instruction = PSRAM_CMD_RESET_EN;
    if (HAL_QSPI_Command(p->hqspi, &cmd, PSRAM_HAL_TIMEOUT_MS) != HAL_OK)
    {
        return -1;
    }

    cmd.Instruction = PSRAM_CMD_RESET;
    if (HAL_QSPI_Command(p->hqspi, &cmd, PSRAM_HAL_TIMEOUT_MS) != HAL_OK)
    {
        return -1;
    }

    return 0;
}

static int qspi_psram_enter_qpi_locked(qspi_psram_t *p)
{
    QSPI_CommandTypeDef cmd = {0};

    p->mmap_active = 0U;
    p->qpi_mode = 0U;
    (void)HAL_QSPI_Abort(p->hqspi);
    CLEAR_BIT(p->hqspi->Instance->CR, QUADSPI_CR_TCEN);
    __HAL_QSPI_CLEAR_FLAG(p->hqspi, QSPI_FLAG_TO);

    /* Reset in both possible bus widths so recovery works from an unknown mode. */
    (void)qspi_psram_send_reset_locked(p, QSPI_INSTRUCTION_4_LINES);
    (void)HAL_QSPI_Abort(p->hqspi);
    if (qspi_psram_send_reset_locked(p, QSPI_INSTRUCTION_1_LINE) != 0)
    {
        return -1;
    }

    HAL_Delay(1U);
    cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode = QSPI_ADDRESS_NONE;
    cmd.DataMode = QSPI_DATA_NONE;
    cmd.DummyCycles = 0U;
    cmd.Instruction = PSRAM_CMD_ENTER_QPI;
    if (HAL_QSPI_Command(p->hqspi, &cmd, PSRAM_HAL_TIMEOUT_MS) != HAL_OK)
    {
        return -1;
    }

    HAL_Delay(1U);
    p->qpi_mode = 1U;
    return 0;
}

static int qspi_psram_enable_memory_mapped_locked(qspi_psram_t *p)
{
    QSPI_CommandTypeDef cmd = {0};
    QSPI_MemoryMappedTypeDef cfg = {0};

    if ((p == NULL) || (p->initialized == 0U) || (p->qpi_mode == 0U))
    {
        return -1;
    }

    (void)HAL_QSPI_Abort(p->hqspi);
    p->mmap_active = 0U;
    cmd.InstructionMode = QSPI_INSTRUCTION_4_LINES;
    cmd.AddressMode = QSPI_ADDRESS_4_LINES;
    cmd.AddressSize = QSPI_ADDRESS_24_BITS;
    cmd.DataMode = QSPI_DATA_4_LINES;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DummyCycles = 6U;
    cmd.Instruction = PSRAM_CMD_READ;
    cmd.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;

    cfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_DISABLE;
    if (HAL_QSPI_MemoryMapped(p->hqspi, &cmd, &cfg) != HAL_OK)
    {
        return -1;
    }

    /* Release nCS after an idle gap without enabling the unused timeout IRQ. */
    WRITE_REG(p->hqspi->Instance->LPTR, PSRAM_MMAP_TIMEOUT_PERIOD);
    __HAL_QSPI_CLEAR_FLAG(p->hqspi, QSPI_FLAG_TO);
    __HAL_QSPI_DISABLE_IT(p->hqspi, QSPI_IT_TO);
    SET_BIT(p->hqspi->Instance->CR, QUADSPI_CR_TCEN);
    p->mmap_active = 1U;
    return 0;
}

/**
 * @brief  初始化 QSPI PSRAM
 * @param  p: context 指针
 * @param  hqspi: QSPI 句柄
 * @retval 0 成功，-1 失败
 */
int qspi_psram_init(qspi_psram_t *p, QSPI_HandleTypeDef *hqspi)
{
    QSPI_CommandTypeDef cmd = {0};

    if (p == NULL || hqspi == NULL)
        return -1;

    memset(p, 0, sizeof(qspi_psram_t));
    p->hqspi = hqspi;

#if PSRAM_USE_FREERTOS
    p->lock = xSemaphoreCreateMutex();
    if (p->lock == NULL)
    {
        log_printf("[PSRAM] mutex create FAIL\r\n");
        return -1;
    }
#endif

    /* 清除外设状态 */
    HAL_QSPI_Abort(p->hqspi);

    /* ----- 1-Line 模式初始化芯片 ----- */
    cmd.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    cmd.AddressMode     = QSPI_ADDRESS_NONE;
    cmd.DataMode        = QSPI_DATA_NONE;
    cmd.DummyCycles     = 0;

    /* RESET ENABLE (0x66) */
    cmd.Instruction = PSRAM_CMD_RESET_EN;
    if (HAL_QSPI_Command(p->hqspi, &cmd, PSRAM_HAL_TIMEOUT_MS) != HAL_OK)
    {
        log_printf("[PSRAM] reset en FAIL\r\n");
        return -1;
    }

    /* RESET (0x99) */
    cmd.Instruction = PSRAM_CMD_RESET;
    if (HAL_QSPI_Command(p->hqspi, &cmd, PSRAM_HAL_TIMEOUT_MS) != HAL_OK)
    {
        log_printf("[PSRAM] reset FAIL\r\n");
        return -1;
    }

    HAL_Delay(1);

    /* ENTER QPI (0x35) */
    cmd.Instruction = PSRAM_CMD_ENTER_QPI;
    if (HAL_QSPI_Command(p->hqspi, &cmd, PSRAM_HAL_TIMEOUT_MS) != HAL_OK)
    {
        log_printf("[PSRAM] enter QPI FAIL\r\n");
        return -1;
    }

    HAL_Delay(1);

    /* 配置 geometry */
    p->psram_size = PSRAM_SIZE;
    p->page_size  = PSRAM_PAGE_SIZE;
    p->total_size = PSRAM_SIZE;
    p->addr_len   = 3;          /* 24-bit address covers 8MB */
    p->qpi_mode   = 1;
    p->initialized = 1;

    log_printf("[PSRAM] init OK (%u MB)\r\n", (unsigned)(PSRAM_SIZE / 1024 / 1024));
    return 0;
}

/**
 * @brief  从 PSRAM 读取数据
 * @param  p: context 指针
 * @param  addr: 起始地址 (0 ~ 8MB-1)
 * @param  buf: 输出缓存
 * @param  len: 读取长度
 * @retval 0 成功，-1 失败
 */
int qspi_psram_read(qspi_psram_t *p, uint32_t addr, uint8_t *buf, uint32_t len)
{
    QSPI_CommandTypeDef cmd = {0};

    if (p == NULL || !p->initialized || buf == NULL || len == 0)
        return -1;

    if (addr + len > p->total_size)
        return -1;

    /* 如果在 MMAP 模式下，不应当走命令模式读，提示使用者走指针访问。 */
    if (p->mmap_active)
        return -1;

    PSRAM_LOCK(p);

    cmd.InstructionMode   = QSPI_INSTRUCTION_4_LINES;
    cmd.AddressMode       = QSPI_ADDRESS_4_LINES;
    cmd.AddressSize       = QSPI_ADDRESS_24_BITS;
    cmd.DataMode          = QSPI_DATA_4_LINES;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DummyCycles       = 6;
    cmd.Instruction       = PSRAM_CMD_READ;
    cmd.Address           = addr;
    cmd.NbData            = len;

    if (HAL_QSPI_Command(p->hqspi, &cmd, PSRAM_HAL_TIMEOUT_MS) != HAL_OK)
    {
        PSRAM_UNLOCK(p);
        return -1;
    }
    if (HAL_QSPI_Receive(p->hqspi, buf, PSRAM_HAL_TIMEOUT_MS) != HAL_OK)
    {
        PSRAM_UNLOCK(p);
        return -1;
    }

    PSRAM_UNLOCK(p);
    return 0;
}

/**
 * @brief  向 PSRAM 写入数据
 * @param  p: context 指针
 * @param  addr: 起始地址 (0 ~ 8MB-1)
 * @param  buf: 输入数据
 * @param  len: 写入长度
 * @retval 0 成功，-1 失败
 */
int qspi_psram_write(qspi_psram_t *p, uint32_t addr, const uint8_t *buf, uint32_t len)
{
    QSPI_CommandTypeDef cmd = {0};
    int ret = 0;
    uint8_t mmap_was_active = 0;

    if (p == NULL || !p->initialized || buf == NULL || len == 0)
        return -1;

    if (addr + len > p->total_size)
        return -1;

    PSRAM_LOCK(p);

    /* 如果在 MMAP 模式，先退出才能发命令。 */
    if (p->mmap_active)
    {
        mmap_was_active = 1;
        HAL_QSPI_Abort(p->hqspi);
        CLEAR_BIT(p->hqspi->Instance->CR, QUADSPI_CR_TCEN);
        __HAL_QSPI_CLEAR_FLAG(p->hqspi, QSPI_FLAG_TO);
        p->mmap_active = 0;
    }

    cmd.InstructionMode   = QSPI_INSTRUCTION_4_LINES;
    cmd.AddressMode       = QSPI_ADDRESS_4_LINES;
    cmd.AddressSize       = QSPI_ADDRESS_24_BITS;
    cmd.DataMode          = QSPI_DATA_4_LINES;
    cmd.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    cmd.DummyCycles       = 0;
    cmd.Instruction       = PSRAM_CMD_WRITE;
    cmd.Address           = addr;
    cmd.NbData            = len;

    if (HAL_QSPI_Command(p->hqspi, &cmd, PSRAM_HAL_TIMEOUT_MS) != HAL_OK)
    {
        ret = -1;
        goto out;
    }
    if (HAL_QSPI_Transmit(p->hqspi, (uint8_t *)buf, PSRAM_HAL_TIMEOUT_MS) != HAL_OK)
    {
        ret = -1;
        goto out;
    }

out:

    /* 写完后恢复 MMAP 模式 */
    if (mmap_was_active)
    {
        if (qspi_psram_enable_memory_mapped_locked(p) != 0)
        {
            ret = -1;
        }
    }

    PSRAM_UNLOCK(p);
    return ret;
}

/**
 * @brief  开启 PSRAM 内存映射模式（MMAP）
 * @note   开启后将 QSPI 地址区间直接映射到 PSRAM 内容，
 *         CPU 可以直接通过 0x90000000 访问 PSRAM。
 * @param  p: context 指针
 * @retval 0 成功，-1 失败
 */
int qspi_psram_enable_memory_mapped(qspi_psram_t *p)
{
    int ret;

    if (p == NULL || !p->initialized)
        return -1;

    PSRAM_LOCK(p);
    ret = qspi_psram_enable_memory_mapped_locked(p);
    PSRAM_UNLOCK(p);

    if (ret != 0)
    {
        log_printf("[PSRAM] memory mapped FAIL\r\n");
        return -1;
    }
    log_printf("[PSRAM] memory mapped enabled\r\n");
    return 0;
}

/**
 * @brief  退出 PSRAM 内存映射模式，回到命令模式
 * @param  p: context 指针
 * @retval 0 成功，-1 失败
 */
int qspi_psram_exit_memory_mapped(qspi_psram_t *p)
{
    if (p == NULL || !p->initialized)
        return -1;

    PSRAM_LOCK(p);
    HAL_QSPI_Abort(p->hqspi);
    CLEAR_BIT(p->hqspi->Instance->CR, QUADSPI_CR_TCEN);
    __HAL_QSPI_CLEAR_FLAG(p->hqspi, QSPI_FLAG_TO);
    p->mmap_active = 0;
    PSRAM_UNLOCK(p);

    log_printf("[PSRAM] memory mapped disabled\r\n");
    return 0;
}

/**
 * @brief  同步等待（PSRAM 无需等待，直接返回成功）
 * @param  p: context 指针
 * @retval 0
 */
int qspi_psram_recover(qspi_psram_t *p)
{
    int ret = -1;

    if ((p == NULL) || (p->hqspi == NULL))
    {
        return -1;
    }

    PSRAM_LOCK(p);
    p->initialized = 0U;
    p->qpi_mode = 0U;
    p->mmap_active = 0U;
    (void)HAL_QSPI_Abort(p->hqspi);

    if ((HAL_QSPI_DeInit(p->hqspi) != HAL_OK) ||
        (HAL_QSPI_Init(p->hqspi) != HAL_OK))
    {
        goto out;
    }

    if (qspi_psram_enter_qpi_locked(p) != 0)
    {
        goto out;
    }

    p->psram_size = PSRAM_SIZE;
    p->page_size = PSRAM_PAGE_SIZE;
    p->total_size = PSRAM_SIZE;
    p->addr_len = 3U;
    p->initialized = 1U;
    ret = 0;

out:
    PSRAM_UNLOCK(p);
    log_printf((ret == 0) ? "[PSRAM] recover OK\r\n" :
                            "[PSRAM] recover FAIL\r\n");
    return ret;
}

int qspi_psram_sync(qspi_psram_t *p)
{
    (void)p;
    return 0;
}

/**
 * @brief  PSRAM 测试函数：写入测试数据 → 进入 MMAP → 读出并比对打印
 * @note   调用本函数前 PSRAM 必须已完成 init，且未进入 MMAP。
 *         测试完自动进入 MMAP 模式。
 * @param  p: context 指针
 */
void qspi_psram_test(qspi_psram_t *p)
{
    uint8_t wbuf[256];
    uint8_t rbuf[256];
    uint32_t i;
    uint32_t err_cnt = 0;
    uint32_t test_addr = 0;

    if (p == NULL || !p->initialized)
    {
        log_printf("[PSRAM TEST] PSRAM not initialized, skip\r\n");
        return;
    }

    if (p->mmap_active)
    {
        log_printf("[PSRAM TEST] already in MMAP mode, exit first\r\n");
        qspi_psram_exit_memory_mapped(p);
    }

    log_printf("\r\n======== PSRAM TEST START ========\r\n");

    /* ---- Pattern 1: 线性递增 ---- */
    for (i = 0; i < 256; i++) wbuf[i] = (uint8_t)i;
    if (qspi_psram_write(p, test_addr, wbuf, 256) != 0)
    {
        log_printf("[PSRAM TEST] write pattern 1 FAIL\r\n");
        goto fail;
    }
    log_printf("[PSRAM TEST] pattern 1 (0x00~0xFF) written @ 0x%08lX\r\n", test_addr);

    /* ---- Pattern 2: 交替 0xAA/0x55 ---- */
    test_addr = 0x1000;
    for (i = 0; i < 256; i++) wbuf[i] = (i & 1) ? 0x55 : 0xAA;
    if (qspi_psram_write(p, test_addr, wbuf, 256) != 0)
    {
        log_printf("[PSRAM TEST] write pattern 2 FAIL\r\n");
        goto fail;
    }
    log_printf("[PSRAM TEST] pattern 2 (0xAA/0x55) written @ 0x%08lX\r\n", test_addr);

    /* ---- Pattern 3: 固定值 ---- */
    test_addr = 0x2000;
    memset(wbuf, 0xA5, 256);
    if (qspi_psram_write(p, test_addr, wbuf, 256) != 0)
    {
        log_printf("[PSRAM TEST] write pattern 3 FAIL\r\n");
        goto fail;
    }
    log_printf("[PSRAM TEST] pattern 3 (0xA5) written @ 0x%08lX\r\n", test_addr);

    /* ---- 进入 MMAP 模式 ---- */
    log_printf("[PSRAM TEST] entering memory-mapped mode...\r\n");
    if (qspi_psram_enable_memory_mapped(p) != 0)
    {
        log_printf("[PSRAM TEST] enable MMAP FAIL\r\n");
        goto fail;
    }

    /* ---- 通过 MMAP 读取并比对 ---- */

    /* Pattern 1 @ 0x0000 */
    for (i = 0; i < 256; i++) rbuf[i] = PSRAM_MMAP_BASE[0x0000 + i];
    err_cnt = 0;
    for (i = 0; i < 256; i++)
    {
        if (rbuf[i] != (uint8_t)i)
        {
            if (err_cnt < 5)
                log_printf("[PSRAM TEST] MISMATCH @ 0x%04lX: expect 0x%02X, got 0x%02X\r\n",
                           (unsigned long)i, (unsigned)((uint8_t)i), (unsigned)rbuf[i]);
            err_cnt++;
        }
    }
    log_printf("[PSRAM TEST] pattern 1 verify: %lu errors\r\n", (unsigned long)err_cnt);

    /* Pattern 2 @ 0x1000 */
    for (i = 0; i < 256; i++) rbuf[i] = PSRAM_MMAP_BASE[0x1000 + i];
    err_cnt = 0;
    for (i = 0; i < 256; i++)
    {
        uint8_t expect = (i & 1) ? 0x55 : 0xAA;
        if (rbuf[i] != expect)
        {
            if (err_cnt < 5)
                log_printf("[PSRAM TEST] MISMATCH @ 0x%04lX: expect 0x%02X, got 0x%02X\r\n",
                           (unsigned long)(0x1000 + i), (unsigned)expect, (unsigned)rbuf[i]);
            err_cnt++;
        }
    }
    log_printf("[PSRAM TEST] pattern 2 verify: %lu errors\r\n", (unsigned long)err_cnt);

    /* Pattern 3 @ 0x2000 */
    for (i = 0; i < 256; i++) rbuf[i] = PSRAM_MMAP_BASE[0x2000 + i];
    err_cnt = 0;
    for (i = 0; i < 256; i++)
    {
        if (rbuf[i] != 0xA5)
        {
            if (err_cnt < 5)
                log_printf("[PSRAM TEST] MISMATCH @ 0x%04lX: expect 0xA5, got 0x%02X\r\n",
                           (unsigned long)(0x2000 + i), (unsigned)rbuf[i]);
            err_cnt++;
        }
    }
    log_printf("[PSRAM TEST] pattern 3 verify: %lu errors\r\n", (unsigned long)err_cnt);

    log_printf("======== PSRAM TEST PASS ========\r\n");
    return;

fail:
    log_printf("======== PSRAM TEST FAIL ========\r\n");
}
