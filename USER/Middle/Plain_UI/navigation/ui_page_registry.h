#ifndef UI_PAGE_REGISTRY_H
#define UI_PAGE_REGISTRY_H

#include "ui_page.h"

void UI_PageRegistry_Init(void);
ui_page_context_t *UI_PageRegistry_GetPage(uint16_t page_id);

#endif /* UI_PAGE_REGISTRY_H */
