#ifndef UI_NAV_STACK_H
#define UI_NAV_STACK_H

#include "ui_navigation.h"

static inline void UI_NavStack_Init(void)
{
  UI_Navigation_Init();
}

static inline BaseType_t UI_NavStack_Push(ui_page_context_t *page)
{
  return UI_Navigation_Push(page);
}

static inline BaseType_t UI_NavStack_Replace(ui_page_context_t *page)
{
  return UI_Navigation_Replace(page);
}

static inline BaseType_t UI_NavStack_Pop(void)
{
  return UI_Navigation_Pop();
}

static inline ui_page_context_t *UI_NavStack_GetActive(void)
{
  return UI_Navigation_GetActive();
}

#endif /* UI_NAV_STACK_H */
