#ifndef __SYSTEMMONITOR_APP_H
#define __SYSTEMMONITOR_APP_H

#define MEM_DIAG_ENABLE 0

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "systemMonitor.h"

typedef enum
{
    MON_SCREEN_IDLE = 0,
    MON_WEATHER_SYNC,
    MON_SYSTEM_AUTO_OFF,
    MON_APP_MAX
} AppMonitorID_t;

void UserMonitor_Init(void);
void UserMonitor_Service(void);
void UserMonitor_ApplySettings(void);
void UserMonitor_OnKeyActivity(void);
void UserMonitor_OnDisplayWake(void);
void UserMonitor_OnDisplaySleep(void);
void UserMonitor_RequestWeatherSync(void);
void UserMonitor_RestartWeatherSync(void);
void UserMonitor_StopWeatherSync(void);
void UserMonitor_StopAll(void);
void Key_Event(void);
#if MEM_DIAG_ENABLE
void MemDiag_LogSnapshot(const char *tag);
#else
#define MemDiag_LogSnapshot(tag) ((void)(tag))
#endif

#ifdef __cplusplus
}
#endif

#endif
