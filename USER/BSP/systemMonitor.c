/*============================================================
 * File: SystemMonitor.c
 *===========================================================*/
#include "SystemMonitor.h"
#include "string.h"

SystemMonitor_t sys_monitor = {0};

static TickType_t Monitor_MillisecondsToTicks(uint32_t timeout_ms)
{
    uint64_t ticks = ((uint64_t)timeout_ms * (uint64_t)configTICK_RATE_HZ) / 1000ULL;

    if (ticks == 0ULL)
    {
        return 1U;
    }
    if (ticks > (uint64_t)portMAX_DELAY)
    {
        return portMAX_DELAY;
    }
    return (TickType_t)ticks;
}

/* 初始化 */


/* 自动分配创建 */
MonitorID_t Monitor_Create(const char *name,
                           uint32_t timeout_ms,
                           uint8_t auto_start,
                           MonitorCallback_t cb)
{
    uint8_t i;

    for(i = 0; i < MONITOR_MAX_NUM; i++)
    {
        if(sys_monitor.item[i].used == 0)
        {
            return Monitor_CreateEx(i, name, timeout_ms, auto_start, cb);
        }
    }

    return MONITOR_INVALID_ID;
}

/* 指定ID创建 */
MonitorID_t Monitor_CreateEx(MonitorID_t id,
                             const char *name,
                             uint32_t timeout_ms,
                             uint8_t auto_start,
                             MonitorCallback_t cb)
{
    MonitorItem_t *m;
    TickType_t timeout_ticks;

    if ((id >= MONITOR_MAX_NUM) || (name == NULL) || (cb == NULL) || (timeout_ms == 0U))
        return MONITOR_INVALID_ID;

    m = &sys_monitor.item[id];
    if (m->used != 0U)
        return MONITOR_INVALID_ID;

    timeout_ticks = Monitor_MillisecondsToTicks(timeout_ms);

    memset(m, 0, sizeof(MonitorItem_t));

    m->used       = 1;
    m->auto_start = auto_start;
    m->timeout_ms = timeout_ms;
    m->callback   = cb;

    strncpy(m->name, name, sizeof(m->name) - 1);

    m->timer = xTimerCreate(
                m->name,
                timeout_ticks,
                pdFALSE,
                0,
                cb);

    if(m->timer == NULL)
    {
        memset(m, 0, sizeof(MonitorItem_t));
        return MONITOR_INVALID_ID;
    }

    if(auto_start)
    {
        if (xTimerStart(m->timer, 0U) != pdPASS)
        {
            (void)xTimerDelete(m->timer, 0U);
            memset(m, 0, sizeof(MonitorItem_t));
            return MONITOR_INVALID_ID;
        }
    }

    return id;
}

static MonitorItem_t *Monitor_GetItem(MonitorID_t id)
{
    if (id >= MONITOR_MAX_NUM) return NULL;
    if ((sys_monitor.item[id].used == 0U) || (sys_monitor.item[id].timer == NULL)) return NULL;

    return &sys_monitor.item[id];
}

BaseType_t Monitor_Start(MonitorID_t id)
{
    MonitorItem_t *m = Monitor_GetItem(id);

    if (m == NULL) return pdFAIL;
    return xTimerStart(m->timer, 0U);
}

BaseType_t Monitor_Stop(MonitorID_t id)
{
    MonitorItem_t *m = Monitor_GetItem(id);

    if (m == NULL) return pdFAIL;
    return xTimerStop(m->timer, 0U);
}

BaseType_t Monitor_Restart(MonitorID_t id)
{
    MonitorItem_t *m = Monitor_GetItem(id);

    if (m == NULL) return pdFAIL;
    return xTimerReset(m->timer, 0U);
}

BaseType_t Monitor_Delete(MonitorID_t id)
{
    MonitorItem_t *m = Monitor_GetItem(id);
    BaseType_t status;

    if (m == NULL) return pdFAIL;
    status = xTimerDelete(m->timer, 0U);
    if (status == pdPASS)
    {
        memset(m, 0, sizeof(*m));
    }
    return status;
}

BaseType_t Monitor_ChangeTimeout(MonitorID_t id, uint32_t timeout_ms)
{
    MonitorItem_t *m = Monitor_GetItem(id);
    TickType_t timeout_ticks;
    BaseType_t status;

    if ((m == NULL) || (timeout_ms == 0U)) return pdFAIL;
    timeout_ticks = Monitor_MillisecondsToTicks(timeout_ms);

    status = xTimerChangePeriod(m->timer, timeout_ticks, 0U);
    if (status == pdPASS)
    {
        m->timeout_ms = timeout_ms;
    }
    return status;
}

uint8_t Monitor_IsActive(MonitorID_t id)
{
    if(id >= MONITOR_MAX_NUM) return 0;
    if(sys_monitor.item[id].used == 0) return 0;

    return (uint8_t)xTimerIsTimerActive(sys_monitor.item[id].timer);
}
