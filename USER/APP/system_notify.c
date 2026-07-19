#include "system_notify.h"

#include "log.h"

static QueueHandle_t s_notify_queue = NULL;
static UBaseType_t s_notify_queue_peak = 0U;

static void SystemNotify_TrackQueuePeak(void)
{
    UBaseType_t current = (s_notify_queue != NULL) ? uxQueueMessagesWaiting(s_notify_queue) : 0U;

    if (current > s_notify_queue_peak) s_notify_queue_peak = current;
}

void SystemNotify_AttachQueue(QueueHandle_t queue)
{
    s_notify_queue = queue;
    s_notify_queue_peak = 0U;
}

bool SystemNotify_Post(SystemNotifyType_t type, int16_t value0, int16_t value1)
{
    SystemNotifyMessage_t message;
    SystemNotifyMessage_t discarded;

    if ((s_notify_queue == NULL) || ((uint32_t)type >= (uint32_t)SYSTEM_NOTIFY_COUNT))
    {
        return false;
    }

    message.type = type;
    message.value0 = value0;
    message.value1 = value1;
    SystemNotify_TrackQueuePeak();

    if (xQueueSend(s_notify_queue, &message, 0U) == pdPASS)
    {
        SystemNotify_TrackQueuePeak();
        return true;
    }

    if (xQueueReceive(s_notify_queue, &discarded, 0U) == pdPASS)
    {
        log_printf("[Notify] queue full, drop %u", (unsigned)discarded.type);
    }

    if (xQueueSend(s_notify_queue, &message, 0U) == pdPASS)
    {
        SystemNotify_TrackQueuePeak();
        return true;
    }
    return false;
}

void SystemNotify_GetQueueUsage(UBaseType_t *current, UBaseType_t *peak)
{
    SystemNotify_TrackQueuePeak();
    if (current != NULL)
    {
        *current = (s_notify_queue != NULL) ? uxQueueMessagesWaiting(s_notify_queue) : 0U;
    }
    if (peak != NULL) *peak = s_notify_queue_peak;
}

bool SystemNotify_TryReceive(SystemNotifyMessage_t *out)
{
    if ((s_notify_queue == NULL) || (out == NULL))
    {
        return false;
    }

    return (xQueueReceive(s_notify_queue, out, 0U) == pdPASS);
}
