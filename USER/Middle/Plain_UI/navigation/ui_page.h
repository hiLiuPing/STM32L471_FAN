/*
 * 页面上下文定义。
 * 每个页面都通过这一组回调暴露 create/enter/process/draw/exit/destroy 生命周期。
 * 页面管理器只操作这个抽象，不依赖具体页面实现。
 */
#ifndef UI_PAGE_H
#define UI_PAGE_H

#include <stdint.h>

#include "../event/ui_event.h"
#include "../event/ui_message_bus.h"
#include "../widget/base/ui_widget.h"

typedef struct ui_page_context ui_page_context_t;

typedef void (*ui_page_lifecycle_fn)(ui_page_context_t *page);
typedef void (*ui_page_message_fn)(ui_page_context_t *page, const ui_msg_t *msg);
typedef void (*ui_page_event_fn)(ui_page_context_t *page, const ui_event_t *event);

struct ui_page_context
{
  uint16_t page_id;
  uint8_t is_created;
  ui_widget_t *root_widget;
  void *user_data;
  ui_page_lifecycle_fn create;
  ui_page_lifecycle_fn enter;
  ui_page_lifecycle_fn process;
  ui_page_lifecycle_fn draw;
  ui_page_lifecycle_fn exit;
  ui_page_lifecycle_fn destroy;
  ui_page_message_fn handle_message;
  ui_page_event_fn handle_event;
};

#endif /* UI_PAGE_H */
