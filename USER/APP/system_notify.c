#include "system_notify.h"

#include "log.h"

static QueueHandle_t s_notify_queue = NULL;

void SystemNotify_AttachQueue(QueueHandle_t queue)
{
    s_notify_queue = queue;
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

    if (xQueueSend(s_notify_queue, &message, 0U) == pdPASS)
    {
        return true;
    }

    if (xQueueReceive(s_notify_queue, &discarded, 0U) == pdPASS)
    {
        log_printf("[Notify] queue full, drop %u", (unsigned)discarded.type);
    }

    return (xQueueSend(s_notify_queue, &message, 0U) == pdPASS);
}

bool SystemNotify_TryReceive(SystemNotifyMessage_t *out)
{
    if ((s_notify_queue == NULL) || (out == NULL))
    {
        return false;
    }

    return (xQueueReceive(s_notify_queue, out, 0U) == pdPASS);
}
