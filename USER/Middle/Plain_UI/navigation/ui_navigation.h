#ifndef UI_NAVIGATION_H
#define UI_NAVIGATION_H

#include "FreeRTOS.h"

#include "../config/ui_config.h"
#include "ui_page.h"

void UI_Navigation_Init(void);
BaseType_t UI_Navigation_Push(ui_page_context_t *page);
BaseType_t UI_Navigation_Replace(ui_page_context_t *page);
BaseType_t UI_Navigation_Pop(void);
ui_page_context_t *UI_Navigation_GetActive(void);

#endif /* UI_NAVIGATION_H */
