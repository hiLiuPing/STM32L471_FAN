#include "user_LEDTask.h"

#include "FreeRTOS.h"
#include "task.h"

#include "multi_led.h"
#include "rgb_led.h"
#include "user_TasksInit.h"

extern RGB_Object_t rgb;

void LEDTask(void *pvParameters)
{
    (void)pvParameters;

    User_Tasks_WaitForHardwareReady();

    for (;;)
    {
        LED_Driver_Update();
        RGB_Update(&rgb, 10U);
        vTaskDelay(pdMS_TO_TICKS(10U));
    }
}
