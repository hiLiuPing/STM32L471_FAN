/*============================================================
 * File: SystemMonitor.c
 *===========================================================*/
#include "SystemMonitor.h"
#include "string.h"

SystemMonitor_t sys_monitor = {0};

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

    if(id >= MONITOR_MAX_NUM)
        return MONITOR_INVALID_ID;

    m = &sys_monitor.item[id];

    memset(m, 0, sizeof(MonitorItem_t));

    m->used       = 1;
    m->auto_start = auto_start;
    m->timeout_ms = timeout_ms;
    m->callback   = cb;

    strncpy(m->name, name, sizeof(m->name) - 1);

    m->timer = xTimerCreate(
                m->name,
                pdMS_TO_TICKS(timeout_ms),
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
        xTimerStart(m->timer, 0);
    }

    return id;
}

void Monitor_Start(MonitorID_t id)
{
    if(id >= MONITOR_MAX_NUM) return;
    if(sys_monitor.item[id].used == 0) return;

    xTimerStart(sys_monitor.item[id].timer, 0);
}

void Monitor_Stop(MonitorID_t id)
{
    if(id >= MONITOR_MAX_NUM) return;
    if(sys_monitor.item[id].used == 0) return;

    xTimerStop(sys_monitor.item[id].timer, 0);
}

void Monitor_Restart(MonitorID_t id)
{
    if(id >= MONITOR_MAX_NUM) return;
    if(sys_monitor.item[id].used == 0) return;

    xTimerReset(sys_monitor.item[id].timer, 0);
}

void Monitor_Delete(MonitorID_t id)
{
    if(id >= MONITOR_MAX_NUM) return;
    if(sys_monitor.item[id].used == 0) return;

    xTimerDelete(sys_monitor.item[id].timer, 0);
    memset(&sys_monitor.item[id], 0, sizeof(MonitorItem_t));
}

void Monitor_ChangeTimeout(MonitorID_t id, uint32_t timeout_ms)
{
    if(id >= MONITOR_MAX_NUM) return;
    if(sys_monitor.item[id].used == 0) return;

    sys_monitor.item[id].timeout_ms = timeout_ms;

    xTimerChangePeriod(
        sys_monitor.item[id].timer,
        pdMS_TO_TICKS(timeout_ms),
        0);
}

uint8_t Monitor_IsActive(MonitorID_t id)
{
    if(id >= MONITOR_MAX_NUM) return 0;
    if(sys_monitor.item[id].used == 0) return 0;

    return (uint8_t)xTimerIsTimerActive(sys_monitor.item[id].timer);
}