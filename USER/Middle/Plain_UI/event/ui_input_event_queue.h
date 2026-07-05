/*
 * 输入事件队列。
 * 平台输入先进入这里，再由 event bridge 分发给页面。
 * 输入采样和页面逻辑通过队列解耦。
 */
#ifndef UI_INPUT_EVENT_QUEUE_H
#define UI_INPUT_EVENT_QUEUE_H

#include "FreeRTOS.h"

#include "ui_event.h"

BaseType_t UI_InputEventQueue_Init(void);
BaseType_t UI_InputEventQueue_Publish(const ui_event_t *event);
BaseType_t UI_InputEventQueue_Receive(ui_event_t *event, TickType_t wait_ticks);

#endif /* UI_INPUT_EVENT_QUEUE_H */
