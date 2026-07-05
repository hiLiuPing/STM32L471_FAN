#include "ui_input_event_queue.h"

#include "queue.h"
#include "task.h"
#include "../config/ui_config.h"

static StaticQueue_t g_input_event_queue_cb;
static uint8_t g_input_event_queue_storage[UI_INPUT_EVENT_QUEUE_LENGTH * sizeof(ui_event_t)];
static QueueHandle_t g_input_event_queue = NULL;

BaseType_t UI_InputEventQueue_Init(void)
{
  if (g_input_event_queue != NULL)
  {
    return pdPASS;
  }

  g_input_event_queue = xQueueCreateStatic(UI_INPUT_EVENT_QUEUE_LENGTH,
                                           sizeof(ui_event_t),
                                           g_input_event_queue_storage,
                                           &g_input_event_queue_cb);
  return (g_input_event_queue != NULL) ? pdPASS : pdFAIL;
}

BaseType_t UI_InputEventQueue_Publish(const ui_event_t *event)
{
  if ((g_input_event_queue == NULL) || (event == NULL))
  {
    return pdFAIL;
  }

  if (xPortIsInsideInterrupt() != pdFALSE)
  {
    return xQueueSendFromISR(g_input_event_queue, event, NULL);
  }

  return xQueueSend(g_input_event_queue, event, 0U);
}

BaseType_t UI_InputEventQueue_Receive(ui_event_t *event, TickType_t wait_ticks)
{
  if ((g_input_event_queue == NULL) || (event == NULL))
  {
    return pdFAIL;
  }

  return xQueueReceive(g_input_event_queue, event, wait_ticks);
}
