/*
 * Plain_UI 统一事件类型。
 * 所有输入最终都被收敛成这个事件结构，再交给导航和页面处理。
 */
#ifndef UI_EVENT_H
#define UI_EVENT_H

#include <stdint.h>

typedef enum
{
  UI_EVENT_NONE = 0,
  UI_KEY_UP,
  UI_KEY_DOWN,
  UI_KEY_LEFT,
  UI_KEY_RIGHT,
  UI_KEY_ENTER,
  UI_KEY_BACK,
  UI_KEY_CLICK,
  UI_KEY_LONG_PRESS
} ui_event_type_t;

typedef struct
{
  ui_event_type_t type;
  int32_t param0;
  int32_t param1;
} ui_event_t;

#endif /* UI_EVENT_H */
