/*
 * UI 消息总线。
 * 用于跨页面、跨模块的异步通信。
 * 这里传输的是轻量消息，不直接承载复杂对象引用。
 */
#ifndef UI_MESSAGE_BUS_H
#define UI_MESSAGE_BUS_H

#include "FreeRTOS.h"

#include <stdint.h>

#include "../config/ui_config.h"

typedef struct
{
  uint16_t msg_id;
  uint16_t payload_len;
  uint32_t timestamp;
  uint8_t payload[UI_MESSAGE_PAYLOAD_SIZE];
} ui_msg_t;

BaseType_t UI_MessageBus_Init(void);
BaseType_t UI_MessageBus_Publish(uint16_t id, const void *payload, uint16_t len);
BaseType_t UI_MessageBus_Receive(ui_msg_t *msg, TickType_t wait_ticks);

#endif /* UI_MESSAGE_BUS_H */
