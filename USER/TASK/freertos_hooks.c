#include "FreeRTOS.h"
#include "task.h"

#include "stm32l4xx.h"

static StaticTask_t g_idle_task_tcb;
static StackType_t g_idle_task_stack[configMINIMAL_STACK_SIZE];

static StaticTask_t g_timer_task_tcb;
static StackType_t g_timer_task_stack[configTIMER_TASK_STACK_DEPTH];

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer = &g_idle_task_tcb;
    *ppxIdleTaskStackBuffer = g_idle_task_stack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    *ppxTimerTaskTCBBuffer = &g_timer_task_tcb;
    *ppxTimerTaskStackBuffer = g_timer_task_stack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}

void vApplicationMallocFailedHook(void)
{
    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}

void vApplicationTickHook(void)
{
}

void vApplicationIdleHook(void)
{
    /* Sleep 模式：内核时钟停、外设/SysTick 继续，任意中断即刻唤醒。
     * 空闲时不再空转烧电，SWD 调试不受影响（仅 STOP/STANDBY 才需处理）。 */
    __WFI();
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    (void)pcTaskName;

    taskDISABLE_INTERRUPTS();
    for (;;)
    {
    }
}
