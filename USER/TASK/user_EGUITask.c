#include "user_EGUITask.h"

#include "FreeRTOS.h"
#include "task.h"

#include "egui_port_stm32l471_fan.h"
#include "user_TasksInit.h"

void EGUIHandlerTask(void *argument)
{
    (void)argument;

    User_Tasks_WaitForHardwareReady();

    for (;;)
    {
        egui_port_poll();
        vTaskDelay(pdMS_TO_TICKS(5U));
    }
}
