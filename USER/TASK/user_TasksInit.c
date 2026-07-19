#include "user_TasksInit.h"

#include "main.h"
#include "user_HardwareInitTask.h"
#include "user_KeyManllegeTask.h"
#include "user_KeyTask.h"
#include "user_LEDTask.h"
#include "user_FanTask.h"
#include "user_EGUITask.h"
#include "user_AppDataTask.h"
#include "user_TransmitTask.h"
#include "user_WeatherSyncTask.h"

#include "fan_app.h"
#include "key.h"
#include "system_notify.h"

TaskHandle_t HardwareInitTaskHandle = NULL;
TaskHandle_t KeyTaskHandle = NULL;
TaskHandle_t KeyManllegeTaskHandle = NULL;
TaskHandle_t LEDTaskHandle = NULL;
TaskHandle_t FanTaskHandle = NULL;
TaskHandle_t EGUIHandlerTaskHandle = NULL;
TaskHandle_t TransmitTaskHandle = NULL;
TaskHandle_t AppDataTaskHandle = NULL;
TaskHandle_t WeatherSyncTaskHandle = NULL;

QueueHandle_t Key_Power_queue = NULL;
QueueHandle_t EGUI_Key_queue = NULL;
QueueHandle_t EGUI_DisplayState_queue = NULL;
QueueHandle_t Fan_Command_queue = NULL;

SemaphoreHandle_t xKeyScanTaskWakeSemaphore = NULL;
SemaphoreHandle_t xLedTaskWakeSemaphore = NULL;
SemaphoreHandle_t xAppDataTaskWakeSemaphore = NULL;
SemaphoreHandle_t xTransmitTaskWakeSemaphore = NULL;
SemaphoreHandle_t xWeatherSyncTaskWakeSemaphore = NULL;

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
    QueueHandle_t system_notify_queue;

    g_system_hw_ready = 0U;

    xKeyScanTaskWakeSemaphore = xSemaphoreCreateBinary();
    xLedTaskWakeSemaphore = xSemaphoreCreateBinary();
    xAppDataTaskWakeSemaphore = xSemaphoreCreateBinary();
    xTransmitTaskWakeSemaphore = xSemaphoreCreateBinary();
    xWeatherSyncTaskWakeSemaphore = xSemaphoreCreateBinary();
    User_Tasks_RequireHandle(xKeyScanTaskWakeSemaphore);
    User_Tasks_RequireHandle(xLedTaskWakeSemaphore);
    User_Tasks_RequireHandle(xAppDataTaskWakeSemaphore);
    User_Tasks_RequireHandle(xTransmitTaskWakeSemaphore);
    User_Tasks_RequireHandle(xWeatherSyncTaskWakeSemaphore);

    Key_Power_queue = xQueueCreate(8U, sizeof(key_event_t));
    User_Tasks_RequireHandle(Key_Power_queue);
    EGUI_Key_queue = xQueueCreate(16U, sizeof(key_event_t));
    User_Tasks_RequireHandle(EGUI_Key_queue);
    EGUI_DisplayState_queue = xQueueCreate(1U, sizeof(EGUI_DisplayState_t));
    User_Tasks_RequireHandle(EGUI_DisplayState_queue);
    Fan_Command_queue = xQueueCreate(8U, sizeof(fan_cmd_t));
    User_Tasks_RequireHandle(Fan_Command_queue);
    FanApp_AttachCommandQueue(Fan_Command_queue);
    system_notify_queue = xQueueCreate(16U, sizeof(SystemNotifyMessage_t));
    User_Tasks_RequireHandle(system_notify_queue);
    SystemNotify_AttachQueue(system_notify_queue);

    User_Tasks_RequireStatus(xTaskCreate(HardwareInitTask,
                                         "HwInitTask",
                                         128U * 10U,
                                         NULL,
                                         tskIDLE_PRIORITY + 5U,
                                         &HardwareInitTaskHandle));

    User_Tasks_RequireStatus(xTaskCreate(KeyTask,
                                         "KeyTask",
                                         256U,
                                         NULL,
                                         tskIDLE_PRIORITY + 2U,
                                         &KeyTaskHandle));

    User_Tasks_RequireStatus(xTaskCreate(KeyManllegeTask,
                                         "KeyMgrTask",
                                         128U * 2U,
                                         NULL,
                                         tskIDLE_PRIORITY + 2U,
                                         &KeyManllegeTaskHandle));

    User_Tasks_RequireStatus(xTaskCreate(LEDTask,
                                         "LEDTask",
                                         128U * 2U,
                                         NULL,
                                         tskIDLE_PRIORITY + 2U,
                                         &LEDTaskHandle));

    User_Tasks_RequireStatus(xTaskCreate(FanTask,
                                         "FanTask",
                                         256U * 2U,
                                         NULL,
                                         tskIDLE_PRIORITY + 2U,
                                         &FanTaskHandle));

    User_Tasks_RequireStatus(xTaskCreate(EGUIHandlerTask,
                                         "EGUIHandler",
                                         128U * 10U,
                                         NULL,
                                         tskIDLE_PRIORITY + 2U,
                                         &EGUIHandlerTaskHandle));

    User_Tasks_RequireStatus(xTaskCreate(TransmitTask,
                                         "TransmitTask",
                                         512U,
                                         NULL,
                                         tskIDLE_PRIORITY + 2U,
                                         &TransmitTaskHandle));

    User_Tasks_RequireStatus(xTaskCreate(AppDataTask,
                                         "AppDataTask",
                                         512U,
                                         NULL,
                                         tskIDLE_PRIORITY + 2U,
                                         &AppDataTaskHandle));

    User_Tasks_RequireStatus(xTaskCreate(WeatherSyncTask,
                                         "WeatherSync",
                                         256U,
                                         NULL,
                                         tskIDLE_PRIORITY + 1U,
                                         &WeatherSyncTaskHandle));
}
