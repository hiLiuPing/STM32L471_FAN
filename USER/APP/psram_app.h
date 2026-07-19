#ifndef __PSRAM_APP_H__
#define __PSRAM_APP_H__
#include "main.h"
#include "qspi_psram.h"

extern qspi_psram_t g_psram;
/* =========================
 * 固定PSRAM地址布局（8MB）
 * ========================= */
#define PSRAM_BASE              0x000000
#define PSRAM_UI_CACHE_BASE     (PSRAM_BASE)
#define PSRAM_UI_CACHE_SIZE     (8U * 1024U * 1024U)




int PSRAM_App_Init(void);

#endif
