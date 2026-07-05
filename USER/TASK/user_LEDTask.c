#include "user_LEDTask.h"

#include "FreeRTOS.h"
#include "task.h"

#include "led_app.h"
#include "multi_led.h"
#include "user_TasksInit.h"
#include "log.h"

void LEDTask(void *pvParameters)
{
    (void)pvParameters;

    User_Tasks_WaitForHardwareReady();

    uint32_t logTickCnt = 0; // 计数变量，放循环外只初始化一次
    const uint32_t LOG_PERIOD_MS = 1000U;
    const uint32_t LOOP_DELAY_MS = 10U;
    const uint32_t LOG_CNT_MAX = LOG_PERIOD_MS / LOOP_DELAY_MS; // 100次打印一次日志

    for (;;)
    {
        LED_Driver_Update();
        RGB_Update(&rgb, 10U);

        // 计数累加，满100次=100*10ms=1s打印日志
        logTickCnt++;
        if (logTickCnt >= LOG_CNT_MAX)
        {
            logTickCnt = 0;
            // 你的打印日志代码
            log_printf("LED Task running, RGB status print log every 1s\r\n");
        }

        vTaskDelay(pdMS_TO_TICKS(LOOP_DELAY_MS));
    }
}
