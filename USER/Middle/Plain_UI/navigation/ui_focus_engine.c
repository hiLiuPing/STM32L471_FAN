#include "ui_focus_engine.h"

static ui_widget_t *g_focus_root = NULL;
static ui_widget_t *g_focus_current = NULL;

static ui_widget_t *UI_FocusEngine_FindFirstFocusable(ui_widget_t *widget)
{
  ui_widget_t *child_match;

  if (widget == NULL)
  {
    return NULL;
  }

  if ((widget->is_visible != 0U) && (widget->accepts_focus != 0U))
  {
    return widget;
  }

  child_match = UI_FocusEngine_FindFirstFocusable(widget->first_child);
  if (child_match != NULL)
  {
    return child_match;
  }

  return UI_FocusEngine_FindFirstFocusable(widget->next_sibling);
}

static ui_widget_t *UI_FocusEngine_FindNextSiblingFocusable(ui_widget_t *widget)
{
  ui_widget_t *node = widget;
  ui_widget_t *candidate;

  while (node != NULL)
  {
    candidate = UI_FocusEngine_FindFirstFocusable(node->next_sibling);
    if (candidate != NULL)
    {
      return candidate;
    }
    node = node->parent;
  }

  return UI_FocusEngine_FindFirstFocusable(g_focus_root);
}

static void UI_FocusEngine_SetFocused(ui_widget_t *widget)
{
  if (g_focus_current == widget)
  {
    return;
  }

  if (g_focus_current != NULL)
  {
    g_focus_current->is_focused = 0U;
    g_focus_current->is_dirty = 1U;
  }

  g_focus_current = widget;

  if (g_focus_current != NULL)
  {
    g_focus_current->is_focused = 1U;
    g_focus_current->is_dirty = 1U;
  }
}

void UI_FocusEngine_Init(void)
{
  g_focus_root = NULL;
  g_focus_current = NULL;
}

void UI_FocusEngine_SetRoot(ui_widget_t *root_widget)
{
  g_focus_root = root_widget;
  UI_FocusEngine_SetFocused(UI_FocusEngine_FindFirstFocusable(root_widget));
}

ui_widget_t *UI_FocusEngine_GetFocused(void)
{
  return g_focus_current;
}

BaseType_t UI_FocusEngine_HandleEvent(const ui_event_t *event)
{
  if (event == NULL)
  {
    return pdFAIL;
  }

  if ((event->type == UI_KEY_UP) || (event->type == UI_KEY_DOWN) ||
      (event->type == UI_KEY_LEFT) || (event->type == UI_KEY_RIGHT))
  {
    if (g_focus_current == NULL)
    {
      UI_FocusEngine_SetFocused(UI_FocusEngine_FindFirstFocusable(g_focus_root));
    }
    else
    {
      UI_FocusEngine_SetFocused(UI_FocusEngine_FindNextSiblingFocusable(g_focus_current));
    }
    return (g_focus_current != NULL) ? pdPASS : pdFAIL;
  }

  if ((g_focus_current != NULL) && (g_focus_current->on_event != NULL))
  {
    g_focus_current->on_event(g_focus_current, event);
    return pdPASS;
  }

  return pdFAIL;
}
