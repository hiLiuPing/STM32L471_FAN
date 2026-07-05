#include "ui_message_bus.h"

#include "queue.h"
#include "task.h"

#include <string.h>

static StaticQueue_t g_ui_message_queue_cb;
static uint8_t g_ui_message_queue_storage[UI_MESSAGE_QUEUE_LENGTH * sizeof(ui_msg_t)];
static QueueHandle_t g_ui_message_queue = NULL;

BaseType_t UI_MessageBus_Init(void)
{
  if (g_ui_message_queue != NULL)
  {
    return pdPASS;
  }

  g_ui_message_queue = xQueueCreateStatic(UI_MESSAGE_QUEUE_LENGTH,
                                          sizeof(ui_msg_t),
                                          g_ui_message_queue_storage,
                                          &g_ui_message_queue_cb);

  return (g_ui_message_queue != NULL) ? pdPASS : pdFAIL;
}

BaseType_t UI_MessageBus_Publish(uint16_t id, const void *payload, uint16_t len)
{
  ui_msg_t msg;

  if ((g_ui_message_queue == NULL) || (len > UI_MESSAGE_PAYLOAD_SIZE))
  {
    return pdFAIL;
  }

  memset(&msg, 0, sizeof(msg));
  msg.msg_id = id;
  msg.payload_len = len;

  if ((payload != NULL) && (len > 0U))
  {
    memcpy(msg.payload, payload, len);
  }

  if (xPortIsInsideInterrupt() != pdFALSE)
  {
    msg.timestamp = xTaskGetTickCountFromISR();
    return xQueueSendFromISR(g_ui_message_queue, &msg, NULL);
  }

  msg.timestamp = xTaskGetTickCount();
  return xQueueSend(g_ui_message_queue, &msg, 0U);
}

BaseType_t UI_MessageBus_Receive(ui_msg_t *msg, TickType_t wait_ticks)
{
  if ((g_ui_message_queue == NULL) || (msg == NULL))
  {
    return pdFAIL;
  }

  return xQueueReceive(g_ui_message_queue, msg, wait_ticks);
}
