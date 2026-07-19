#ifndef __USER_TASKSINIT_H__
#define __USER_TASKSINIT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "task.h"

#include <stdint.h>

extern TaskHandle_t HardwareInitTaskHandle;
extern TaskHandle_t KeyTaskHandle;
extern TaskHandle_t KeyManllegeTaskHandle;
extern TaskHandle_t LEDTaskHandle;
extern TaskHandle_t FanTaskHandle;
extern TaskHandle_t EGUIHandlerTaskHandle;
extern TaskHandle_t TransmitTaskHandle;
extern TaskHandle_t AppDataTaskHandle;
extern TaskHandle_t WeatherSyncTaskHandle;

extern QueueHandle_t Key_Power_queue;
extern QueueHandle_t EGUI_Key_queue;
extern QueueHandle_t EGUI_DisplayState_queue;
extern QueueHandle_t Fan_Command_queue;

extern SemaphoreHandle_t xTransmitTaskWakeSemaphore;
extern SemaphoreHandle_t xWeatherSyncTaskWakeSemaphore;

extern volatile uint8_t g_system_hw_ready;

void User_Tasks_Init(void);
void User_Tasks_SetHardwareReady(void);
void User_Tasks_WaitForHardwareReady(void);

#ifdef __cplusplus
}
#endif

#endif /* __USER_TASKSINIT_H__ */
