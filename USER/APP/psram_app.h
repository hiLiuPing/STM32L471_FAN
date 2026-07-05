#ifndef __PSRAM_APP_H__
#define __PSRAM_APP_H__
#include "main.h"
#include "qspi_psram.h"

extern qspi_psram_t g_psram;
/* =========================
 * 固定PSRAM地址布局（8MB）
 * ========================= */
#define PSRAM_BASE              0x000000




int PSRAM_App_Init(void);

#endif 