/* Private includes -----------------------------------------------------------*/
//includes
#include "user_TasksInit.h"
//sys
// #include "sys.h"
#include "stdio.h"

//bsp
#include "key.h"

//gui
// #include "lvgl.h"
// #include "lv_lib_pm.h"

//tasks
#include "user_HardwareInitTask.h"
// #include "user_LVGLTask.h"
// #include "user_PDUFPTask.h"
#include "user_KeyTask.h"
// #include "user_MessageTask.h"

/* Private typedef -----------------------------------------------------------*/

/* Private define ------------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

/* Timers --------------------------------------------------------------------*/
// TimerHandle_t IdleTimerHandle;

/* Tasks ---------------------------------------------------------------------*/
// Hardwares initialization
TaskHandle_t HardwareInitTaskHandle;

// // message receive task
// TaskHandle_t MessageReceiveTaskHandle;

// message send task
// TaskHandle_t MessageSendTaskHandle;

// Key task
TaskHandle_t KeyTaskHandle;

// PDUFP task
// TaskHandle_t PDUFPTaskHandle;

// // LVGL Handler task
// TaskHandle_t LvHandlerTaskHandle;

/* Message queues ------------------------------------------------------------*/

// Key task 中发送出的按键信息的消息队列
// 流向为 KeyTask -> LVGLtask
QueueHandle_t Key_MessageQueue;

// UI layer发送给PDUFPTask任务的命令消息队列
// // 流向为 LVGLtask -> PDUFPTask
// QueueHandle_t PD_cmd_MessageQueue;

// // PDUFPTask任务发送给UI层的通知处理情况的消息队列
// // 流向为 PDUFPTask -> LVGLtask
// QueueHandle_t PD_handle_event_MsgQueue;

// // 数据处理Task任务发送给UI层的电压电流数据的消息队列
// // 流向为 MessageSendTask -> LVGLtask
// QueueHandle_t PowerDataQueue;

/* Private function prototypes -----------------------------------------------*/
// void LvHandlerTask(void *argument);

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void User_Tasks_Init(void)
{
  /* add mutexes, ... */

  /* add semaphores, ... */

  /* start timers, add new ones, ... */

  /* add queues, ... */
	Key_MessageQueue  = xQueueCreate(4, sizeof(key_event_t));

  // PD_cmd_MessageQueue = xQueueCreate(4, sizeof(PD_command_msg_t));
  // PD_handle_event_MsgQueue = xQueueCreate(4, 1); // uint8_t message
  // PowerDataQueue = xQueueCreate(8, sizeof(PowerData_t));

	/* add threads, ... */
  xTaskCreate(HardwareInitTask, "HwInitTask", 128*10, NULL, tskIDLE_PRIORITY + 5, &HardwareInitTaskHandle);
  // xTaskCreate(PDUFPTask, "PDUFPTask", 128*5, NULL, tskIDLE_PRIORITY + 1, &PDUFPTaskHandle);
  xTaskCreate(KeyTask, "KeyTask", 128*2, NULL, tskIDLE_PRIORITY + 2, &KeyTaskHandle);
  // xTaskCreate(MessageReceiveTask, "MsgRecTask", 128*2, NULL, tskIDLE_PRIORITY + 2, &MessageReceiveTaskHandle);
  // xTaskCreate(MessageSendTask, "MsgSendTask", 128*8, NULL, tskIDLE_PRIORITY + 3, &MessageSendTaskHandle);
  // xTaskCreate(LvHandlerTask, "LvHandlerTask", 128*32, NULL, tskIDLE_PRIORITY + 2, &LvHandlerTaskHandle);

  /* add events, ... */

	/* add  others ... */
}