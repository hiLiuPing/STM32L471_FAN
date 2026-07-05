#include "ui_virtual_list.h"

#include <string.h>

#include "../../config/ui_config.h"

void UI_VirtualList_Init(ui_virtual_list_t *list,
                         const ui_list_provider_t *provider,
                         uint8_t visible_rows,
                         int16_t row_height)
{
  if (list == NULL)
  {
    return;
  }

  memset(list, 0, sizeof(*list));
  if (provider != NULL)
  {
    list->provider = *provider;
  }

  list->visible_rows = (visible_rows > 0U) ? visible_rows : UI_VIRTUAL_LIST_VISIBLE_ROWS;
  list->row_height = (row_height > 0) ? row_height : 20;
}

BaseType_t UI_VirtualList_HandleEvent(ui_virtual_list_t *list, const ui_event_t *event)
{
  if ((list == NULL) || (event == NULL) || (list->provider.total_items == 0U))
  {
    return pdFAIL;
  }

  /* 焦点窗口与可见窗口一起滚动，保证选中项始终在可见范围内。 */
  if ((event->type == UI_KEY_DOWN) || (event->type == UI_KEY_RIGHT))
  {
    if ((list->focus_index + 1U) < list->provider.total_items)
    {
      list->focus_index++;
    }
  }
  else if ((event->type == UI_KEY_UP) || (event->type == UI_KEY_LEFT))
  {
    if (list->focus_index > 0U)
    {
      list->focus_index--;
    }
  }
  else if ((event->type == UI_KEY_ENTER) || (event->type == UI_KEY_CLICK))
  {
    if (list->provider.on_item_click != NULL)
    {
      list->provider.on_item_click(list->focus_index);
    }
  }
  else
  {
    return pdFAIL;
  }

  if (list->focus_index < list->first_visible_index)
  {
    list->first_visible_index = list->focus_index;
  }
  else if (list->focus_index >= (list->first_visible_index + list->visible_rows))
  {
    list->first_visible_index = list->focus_index - list->visible_rows + 1U;
  }

  return pdPASS;
}

void UI_VirtualList_Draw(ui_virtual_list_t *list,
                         const ui_viewport_t *viewport,
                         const ui_rect_t *rect,
                         const ui_render_style_t *style)
{
  uint8_t row;
  char item_text[40];

  if ((list == NULL) || (viewport == NULL) || (rect == NULL) || (style == NULL))
  {
    return;
  }

  for (row = 0U; row < list->visible_rows; ++row)
  {
    uint32_t item_index = list->first_visible_index + row;
    ui_rect_t row_rect;

    if (item_index >= list->provider.total_items)
    {
      break;
    }

    memset(item_text, 0, sizeof(item_text));
    if (list->provider.get_item_text != NULL)
    {
      list->provider.get_item_text(item_index, item_text, sizeof(item_text));
    }

    row_rect.x = rect->x;
    row_rect.y = rect->y + (int16_t)(row * (uint8_t)list->row_height);
    row_rect.width = rect->width;
    row_rect.height = list->row_height - 2;

    /* 每一行都由 provider 提供内容，列表只负责复用和高亮。 */
    UI_RendererAdapter_DrawListItem(viewport,
                                    &row_rect,
                                    item_text,
                                    (item_index == list->focus_index) ? "focused" : "provider row",
                                    (item_index == list->focus_index) ? 1U : 0U,
                                    style);
  }
}
