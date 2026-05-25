#ifndef __USER_TASKSINIT_H__
#define __USER_TASKSINIT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"  // 原生队列必须加这个头文件

// 用于数据传输的结构体，包含电压和电流
// 流向为 MessageSendTask -> LVGLtask
// typedef struct {
//     float voltage;    // 电压 (V)
//     float current;    // 电流 (uA)
// } PowerData_t;

// 定义一个标志位，表示 ADC 数据已经准备好，在ADC DMA中断中被置位
// 在 MessageSendTask 中等待这个标志位，表示可以发送 ADC 数据了
// #define FLAG_ADC_HALF_READY  0x0001U  // 0000 0001 ADC 半满
// #define FLAG_ADC_FULL_READY  0x0002U  // 0000 0010 ADC 全满
// #define FLAG_USB_UPDATE_REQ  0x0004U  // 0000 0100 USB 请求升级

// 任务句柄声明（原生 FreeRTOS）
// extern TaskHandle_t MessageSendTaskHandle;
// extern TaskHandle_t MessageReceiveTaskHandle;

// 队列句柄声明（原生 FreeRTOS）
extern QueueHandle_t Key_MessageQueue;
// extern QueueHandle_t PD_cmd_MessageQueue;
// extern QueueHandle_t PD_handle_event_MsgQueue;
// extern QueueHandle_t PowerDataQueue;

void User_Tasks_Init(void);

#ifdef __cplusplus
}
#endif

#endif