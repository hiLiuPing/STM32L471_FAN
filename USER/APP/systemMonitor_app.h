#ifndef __SYSTEMMONITOR_APP_H
#define __SYSTEMMONITOR_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "systemMonitor.h"

typedef enum
{
    MON_KEY_IDLE = 0,
    MON_SENSOR_LOG,
    MON_APP_MAX
} AppMonitorID_t;

void UserMonitor_Init(void);
void Key_Event(void);
void MemDiag_LogSnapshot(const char *tag);

#ifdef __cplusplus
}
#endif

#endif
