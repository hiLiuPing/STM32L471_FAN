#include "psram_app.h"
#include "log.h"
#include <string.h>

/* 外部PSRAM驱动 */
extern QSPI_HandleTypeDef hqspi;
qspi_psram_t g_psram;



/* =========================
 * PSRAM APP INIT
 * ========================= */
int PSRAM_App_Init(void)
{
    log_printf("\r\n========== PSRAM INIT ==========\r\n");

    if (qspi_psram_init(&g_psram, &hqspi) != 0)
    {
        log_printf("[APP] PSRAM init failed\r\n");
        return -1;
    }

    log_printf("[APP] Font init OK\r\n");

    // qspi_psram_test(&g_psram);

    return 0;
}
