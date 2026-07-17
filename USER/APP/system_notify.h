#ifndef __SYSTEM_NOTIFY_H__
#define __SYSTEM_NOTIFY_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"

typedef enum
{
    SYSTEM_NOTIFY_POWER_IN = 0,
    SYSTEM_NOTIFY_POWER_OUT,
    SYSTEM_NOTIFY_CHARGE_COMPLETE,
    SYSTEM_NOTIFY_LOW_BATTERY,
    SYSTEM_NOTIFY_WEATHER_SYNC_START,
    SYSTEM_NOTIFY_WEATHER_SYNC_COMPLETE,
    SYSTEM_NOTIFY_ENVIRONMENT_ALERT,
    SYSTEM_NOTIFY_FAN_ON,
    SYSTEM_NOTIFY_FAN_OFF,
    SYSTEM_NOTIFY_FAN_SPEED,
    SYSTEM_NOTIFY_COUNT
} SystemNotifyType_t;

typedef struct
{
    SystemNotifyType_t type;
    int16_t value0;
    int16_t value1;
} SystemNotifyMessage_t;

void SystemNotify_AttachQueue(QueueHandle_t queue);
bool SystemNotify_Post(SystemNotifyType_t type, int16_t value0, int16_t value1);
bool SystemNotify_TryReceive(SystemNotifyMessage_t *out);

#ifdef __cplusplus
}
#endif

#endif /* __SYSTEM_NOTIFY_H__ */
