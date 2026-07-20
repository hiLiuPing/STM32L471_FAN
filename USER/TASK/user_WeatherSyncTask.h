#ifndef __USER_WEATHERSYNCTASK_H
#define __USER_WEATHERSYNCTASK_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

void WeatherSyncTask(void *argument);
bool WeatherSyncTask_RequestManual(void);

#ifdef __cplusplus
}
#endif

#endif
