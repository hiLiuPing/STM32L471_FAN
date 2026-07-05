#include "ui_property_store.h"

#include "task.h"

#include <string.h>

static ui_store_buffer_t g_prop_backend_buffer;
static ui_store_buffer_t g_prop_frontend_buffer;

/* backend 查找写槽，front 查找读槽。
 * 这样写入和绘制分离，避免一帧内读到“半写入”状态。
 */
static ui_prop_slot_t *UI_PropertyStore_FindWritableSlot(ui_store_buffer_t *buffer, uint16_t property_id)
{
  uint32_t i;
  ui_prop_slot_t *free_slot = NULL;

  for (i = 0U; i < UI_MAX_PROPERTY_COUNT; ++i)
  {
    if (buffer->slots[i].property_id == property_id)
    {
      return &buffer->slots[i];
    }

    if ((free_slot == NULL) && (buffer->slots[i].property_id == 0U))
    {
      free_slot = &buffer->slots[i];
    }
  }

  if (free_slot != NULL)
  {
    free_slot->property_id = property_id;
  }

  return free_slot;
}

static const ui_prop_slot_t *UI_PropertyStore_FindReadableSlot(const ui_store_buffer_t *buffer, uint16_t property_id)
{
  uint32_t i;

  for (i = 0U; i < UI_MAX_PROPERTY_COUNT; ++i)
  {
    if (buffer->slots[i].property_id == property_id)
    {
      return &buffer->slots[i];
    }
  }

  return NULL;
}

void UI_PropertyStore_Init(void)
{
  memset(&g_prop_backend_buffer, 0, sizeof(g_prop_backend_buffer));
  memset(&g_prop_frontend_buffer, 0, sizeof(g_prop_frontend_buffer));
}

void UI_PropertyStore_SyncFront(void)
{
  uint32_t i;

  /* 只把 dirty 项从 backend 推到 front，其余项保持不动。 */
  taskENTER_CRITICAL();
  for (i = 0U; i < UI_MAX_PROPERTY_COUNT; ++i)
  {
    if (g_prop_backend_buffer.slots[i].is_dirty != 0U)
    {
      g_prop_frontend_buffer.slots[i] = g_prop_backend_buffer.slots[i];
      g_prop_frontend_buffer.slots[i].is_dirty = 1U;
      g_prop_backend_buffer.slots[i].is_dirty = 0U;
    }
  }
  taskEXIT_CRITICAL();
}

BaseType_t UI_PropertyStore_SetInt(uint16_t property_id, int32_t value)
{
  ui_prop_slot_t *slot = UI_PropertyStore_FindWritableSlot(&g_prop_backend_buffer, property_id);

  if (slot == NULL)
  {
    return pdFAIL;
  }

  slot->type = UI_PROP_INT;
  slot->val.val_int = value;
  slot->is_dirty = 1U;
  return pdPASS;
}

BaseType_t UI_PropertyStore_SetFloat(uint16_t property_id, float value)
{
  ui_prop_slot_t *slot = UI_PropertyStore_FindWritableSlot(&g_prop_backend_buffer, property_id);

  if (slot == NULL)
  {
    return pdFAIL;
  }

  slot->type = UI_PROP_FLOAT;
  slot->val.val_float = value;
  slot->is_dirty = 1U;
  return pdPASS;
}

BaseType_t UI_PropertyStore_SetString(uint16_t property_id, const char *value)
{
  ui_prop_slot_t *slot = UI_PropertyStore_FindWritableSlot(&g_prop_backend_buffer, property_id);

  if ((slot == NULL) || (value == NULL))
  {
    return pdFAIL;
  }

  slot->type = UI_PROP_STR;
  /* 字符串采用定长缓冲，避免页面侧持有悬空指针。 */
  strncpy(slot->val.val_str, value, UI_PROPERTY_STRING_LENGTH - 1U);
  slot->val.val_str[UI_PROPERTY_STRING_LENGTH - 1U] = '\0';
  slot->is_dirty = 1U;
  return pdPASS;
}

const ui_prop_slot_t *UI_PropertyStore_GetFrontSlot(uint16_t property_id)
{
  return UI_PropertyStore_FindReadableSlot(&g_prop_frontend_buffer, property_id);
}

int32_t UI_PropertyStore_GetFrontInt(uint16_t property_id, int32_t default_value)
{
  const ui_prop_slot_t *slot = UI_PropertyStore_FindReadableSlot(&g_prop_frontend_buffer, property_id);

  if ((slot == NULL) || (slot->type != UI_PROP_INT))
  {
    return default_value;
  }

  return slot->val.val_int;
}

const char *UI_PropertyStore_GetFrontString(uint16_t property_id, const char *default_value)
{
  const ui_prop_slot_t *slot = UI_PropertyStore_FindReadableSlot(&g_prop_frontend_buffer, property_id);

  if ((slot == NULL) || (slot->type != UI_PROP_STR))
  {
    return default_value;
  }

  return slot->val.val_str;
}

uint8_t UI_PropertyStore_IsFrontDirty(uint16_t property_id)
{
  const ui_prop_slot_t *slot = UI_PropertyStore_FindReadableSlot(&g_prop_frontend_buffer, property_id);

  return ((slot != NULL) && (slot->is_dirty != 0U)) ? 1U : 0U;
}

void UI_PropertyStore_ClearFrontDirty(uint16_t property_id)
{
  uint32_t i;

  for (i = 0U; i < UI_MAX_PROPERTY_COUNT; ++i)
  {
    if (g_prop_frontend_buffer.slots[i].property_id == property_id)
    {
      g_prop_frontend_buffer.slots[i].is_dirty = 0U;
      return;
    }
  }
}
