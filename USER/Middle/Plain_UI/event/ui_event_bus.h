#ifndef UI_EVENT_BUS_H
#define UI_EVENT_BUS_H

#include "ui_message_bus.h"

static inline BaseType_t UI_EventBus_Init(void)
{
  return UI_MessageBus_Init();
}

static inline BaseType_t UI_EventBus_Publish(uint16_t id, const void *payload, uint16_t len)
{
  return UI_MessageBus_Publish(id, payload, len);
}

static inline BaseType_t UI_EventBus_Receive(ui_msg_t *msg, TickType_t wait_ticks)
{
  return UI_MessageBus_Receive(msg, wait_ticks);
}

#endif /* UI_EVENT_BUS_H */
