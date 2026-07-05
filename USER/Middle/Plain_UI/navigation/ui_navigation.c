#include "ui_navigation.h"

typedef struct
{
  ui_page_context_t *stack[UI_NAV_STACK_DEPTH_LIMIT];
  int8_t top_idx;
} ui_nav_stack_t;

static ui_nav_stack_t g_navigation_stack;

void UI_Navigation_Init(void)
{
  uint32_t i;

  for (i = 0U; i < UI_NAV_STACK_DEPTH_LIMIT; ++i)
  {
    g_navigation_stack.stack[i] = NULL;
  }
  g_navigation_stack.top_idx = -1;
}

BaseType_t UI_Navigation_Push(ui_page_context_t *page)
{
  if ((page == NULL) || (g_navigation_stack.top_idx >= (UI_NAV_STACK_DEPTH_LIMIT - 1)))
  {
    return pdFAIL;
  }

  /* Push 时保留旧页在栈中，常用于返回上一层页面。 */
  if (g_navigation_stack.top_idx >= 0)
  {
    ui_page_context_t *active = g_navigation_stack.stack[g_navigation_stack.top_idx];
    if ((active != NULL) && (active->exit != NULL))
    {
      active->exit(active);
    }
  }

  g_navigation_stack.stack[++g_navigation_stack.top_idx] = page;

  if ((page->is_created == 0U) && (page->create != NULL))
  {
    page->create(page);
    page->is_created = 1U;
  }

  if (page->enter != NULL)
  {
    page->enter(page);
  }

  return pdPASS;
}

BaseType_t UI_Navigation_Replace(ui_page_context_t *page)
{
  ui_page_context_t *active;

  if (page == NULL)
  {
    return pdFAIL;
  }

  if (g_navigation_stack.top_idx < 0)
  {
    return UI_Navigation_Push(page);
  }

  active = g_navigation_stack.stack[g_navigation_stack.top_idx];
  if (active == page)
  {
    return pdPASS;
  }

  /* Replace 会销毁当前页，再把新页放到当前栈顶。 */
  if ((active != NULL) && (active->exit != NULL))
  {
    active->exit(active);
  }
  if ((active != NULL) && (active->destroy != NULL))
  {
    active->destroy(active);
    active->is_created = 0U;
  }

  g_navigation_stack.stack[g_navigation_stack.top_idx] = page;

  if ((page->is_created == 0U) && (page->create != NULL))
  {
    page->create(page);
    page->is_created = 1U;
  }

  if (page->enter != NULL)
  {
    page->enter(page);
  }

  return pdPASS;
}

BaseType_t UI_Navigation_Pop(void)
{
  ui_page_context_t *page;
  ui_page_context_t *resume_page;

  /* Pop 返回上一页，当前页会被退出并销毁。 */
  if (g_navigation_stack.top_idx <= 0)
  {
    return pdFAIL;
  }

  page = g_navigation_stack.stack[g_navigation_stack.top_idx];
  if ((page != NULL) && (page->exit != NULL))
  {
    page->exit(page);
  }
  if ((page != NULL) && (page->destroy != NULL))
  {
    page->destroy(page);
    page->is_created = 0U;
  }

  g_navigation_stack.stack[g_navigation_stack.top_idx] = NULL;
  g_navigation_stack.top_idx--;

  resume_page = g_navigation_stack.stack[g_navigation_stack.top_idx];
  if ((resume_page != NULL) && (resume_page->enter != NULL))
  {
    resume_page->enter(resume_page);
  }

  return pdPASS;
}

ui_page_context_t *UI_Navigation_GetActive(void)
{
  if (g_navigation_stack.top_idx < 0)
  {
    return NULL;
  }

  return g_navigation_stack.stack[g_navigation_stack.top_idx];
}
