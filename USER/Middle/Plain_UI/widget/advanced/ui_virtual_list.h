/*
 * 虚拟列表抽象。
 * provider 提供条目文本和点击回调，列表自身只维护焦点和可见窗口。
 * 页面可以复用它承载菜单流或大型数据列表。
 */
#ifndef UI_VIRTUAL_LIST_H
#define UI_VIRTUAL_LIST_H

#include "FreeRTOS.h"

#include <stdint.h>

#include "../../event/ui_event.h"
#include "../../hal/backend_lvgl/ui_renderer_adapter.h"
#include "../../core/ui_types.h"

typedef void (*ui_list_get_item_text_fn)(uint32_t index, char *out_buf, uint16_t buf_len);
typedef void (*ui_list_item_click_fn)(uint32_t index);

typedef struct
{
  uint32_t total_items;
  ui_list_get_item_text_fn get_item_text;
  ui_list_item_click_fn on_item_click;
} ui_list_provider_t;

typedef struct
{
  ui_list_provider_t provider;
  uint32_t focus_index;
  uint32_t first_visible_index;
  uint8_t visible_rows;
  int16_t row_height;
} ui_virtual_list_t;

void UI_VirtualList_Init(ui_virtual_list_t *list,
                         const ui_list_provider_t *provider,
                         uint8_t visible_rows,
                         int16_t row_height);
BaseType_t UI_VirtualList_HandleEvent(ui_virtual_list_t *list, const ui_event_t *event);
void UI_VirtualList_Draw(ui_virtual_list_t *list,
                         const ui_viewport_t *viewport,
                         const ui_rect_t *rect,
                         const ui_render_style_t *style);

#endif /* UI_VIRTUAL_LIST_H */
