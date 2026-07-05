#ifndef UI_EVENT_QUEUE_H
#define UI_EVENT_QUEUE_H

#include "ui_input_event_queue.h"

static inline BaseType_t UI_EventQueue_Init(void)
{
  return UI_InputEventQueue_Init();
}

static inline BaseType_t UI_EventQueue_Publish(const ui_event_t *event)
{
  return UI_InputEventQueue_Publish(event);
}

static inline BaseType_t UI_EventQueue_Receive(ui_event_t *event, TickType_t wait_ticks)
{
  return UI_InputEventQueue_Receive(event, wait_ticks);
}

#endif /* UI_EVENT_QUEUE_H */
