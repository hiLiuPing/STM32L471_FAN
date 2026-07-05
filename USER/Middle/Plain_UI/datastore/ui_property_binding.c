#include "ui_property_binding.h"

#include <string.h>

#include "../config/ui_config.h"
#include "../core/ui_dirty_region.h"
#include "ui_property_store.h"

typedef struct
{
  uint16_t property_id;
  ui_rect_t rect;
  uint8_t is_used;
} ui_property_binding_t;

static ui_property_binding_t g_property_bindings[UI_PROPERTY_BINDING_CAPACITY];

void UI_PropertyBinding_Init(void)
{
  memset(g_property_bindings, 0, sizeof(g_property_bindings));
}

BaseType_t UI_PropertyBinding_BindRect(uint16_t property_id, const ui_rect_t *rect)
{
  uint32_t i;

  if ((property_id == 0U) || (rect == NULL))
  {
    return pdFAIL;
  }

  for (i = 0U; i < UI_PROPERTY_BINDING_CAPACITY; ++i)
  {
    if ((g_property_bindings[i].is_used != 0U) && (g_property_bindings[i].property_id == property_id))
    {
      g_property_bindings[i].rect = *rect;
      return pdPASS;
    }
  }

  for (i = 0U; i < UI_PROPERTY_BINDING_CAPACITY; ++i)
  {
    if (g_property_bindings[i].is_used == 0U)
    {
      g_property_bindings[i].is_used = 1U;
      g_property_bindings[i].property_id = property_id;
      g_property_bindings[i].rect = *rect;
      return pdPASS;
    }
  }

  return pdFAIL;
}

void UI_PropertyBinding_UpdateDirtyRegions(void)
{
  uint32_t i;

  /* 属性变化只失效绑定矩形，避免无关区域整屏重绘。 */
  for (i = 0U; i < UI_PROPERTY_BINDING_CAPACITY; ++i)
  {
    if ((g_property_bindings[i].is_used != 0U) &&
        (UI_PropertyStore_IsFrontDirty(g_property_bindings[i].property_id) != 0U))
    {
      UI_DirtyRegion_Invalidate(&g_property_bindings[i].rect);
      UI_PropertyStore_ClearFrontDirty(g_property_bindings[i].property_id);
    }
  }
}
