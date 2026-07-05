#include "user_TasksInit.h"

#include "main.h"
#include "user_HardwareInitTask.h"
#include "user_KeyManllegeTask.h"
#include "user_KeyTask.h"
#include "user_LEDTask.h"
#include "user_LVGLTask.h"

#include "key.h"

TaskHandle_t HardwareInitTaskHandle = NULL;
TaskHandle_t KeyTaskHandle = NULL;
TaskHandle_t KeyManllegeTaskHandle = NULL;
TaskHandle_t LEDTaskHandle = NULL;
TaskHandle_t LvHandlerTaskHandle = NULL;

QueueHandle_t Key_Power_queue = NULL;

SemaphoreHandle_t xKeyScanTaskWakeSemaphore = NULL;
SemaphoreHandle_t xLedTaskWakeSemaphore = NULL;

volatile uint8_t g_system_hw_ready = 0U;

static void User_Tasks_RequireHandle(const void *handle)
{
    if (handle == NULL)
    {
        Error_Handler();
    }
}

static void User_Tasks_RequireStatus(BaseType_t status)
{
    if (status != pdPASS)
    {
        Error_Handler();
    }
}

void User_Tasks_SetHardwareReady(void)
{
    g_system_hw_ready = 1U;
}

void User_Tasks_WaitForHardwareReady(void)
{
    while (g_system_hw_ready == 0U)
    {
        vTaskDelay(pdMS_TO_TICKS(10U));
    }
}

void User_Tasks_Init(void)
{
    g_system_hw_ready = 0U;

    xKeyScanTaskWakeSemaphore = xSemaphoreCreateBinary();
    xLedTaskWakeSemaphore = xSemaphoreCreateBinary();
    User_Tasks_RequireHandle(xKeyScanTaskWakeSemaphore);
    User_Tasks_RequireHandle(xLedTaskWakeSemaphore);

    Key_Power_queue = xQueueCreate(8U, sizeof(key_event_t));
    User_Tasks_RequireHandle(Key_Power_queue);

    User_Tasks_RequireStatus(xTaskCreate(HardwareInitTask,
                                         "HwInitTask",
                                         128U * 10U,
                                         NULL,
                                         tskIDLE_PRIORITY + 5U,
                                         &HardwareInitTaskHandle));

    User_Tasks_RequireStatus(xTaskCreate(KeyTask,
                                         "KeyTask",
                                         128U * 2U,
                                         NULL,
                                         tskIDLE_PRIORITY + 2U,
                                         &KeyTaskHandle));

    User_Tasks_RequireStatus(xTaskCreate(LEDTask,
                                         "LEDTask",
                                         128U * 2U,
                                         NULL,
                                         tskIDLE_PRIORITY + 2U,
                                         &LEDTaskHandle));

    User_Tasks_RequireStatus(xTaskCreate(LvHandlerTask,
                                         "LvHandlerTask",
                                         1024U,
                                         NULL,
                                         tskIDLE_PRIORITY + 1U,
                                         &LvHandlerTaskHandle));
}
