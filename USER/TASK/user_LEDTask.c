#include "user_LEDTask.h"

#include "FreeRTOS.h"
#include "task.h"

#include "led_app.h"
#include "multi_led.h"
#include "user_TasksInit.h"

void LEDTask(void *pvParameters)
{
    const uint32_t loop_delay_ms = 10U;

    (void)pvParameters;
    User_Tasks_WaitForHardwareReady();

    for (;;)
    {
        LED_Driver_Update();
        LED_App_Update(loop_delay_ms);
        vTaskDelay(pdMS_TO_TICKS(loop_delay_ms));
    }
}
