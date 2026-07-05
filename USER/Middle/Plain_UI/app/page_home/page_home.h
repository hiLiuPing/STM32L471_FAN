#ifndef UI_PAGE_HOME_H
#define UI_PAGE_HOME_H

#include "../../event/ui_event.h"
#include "../../widget/base/ui_widget.h"

void UI_PageHome_Draw(void);
ui_obj_t *UI_PageHome_Create(ui_obj_t *parent);
void UI_PageHome_Enter(void);
void UI_PageHome_Exit(void);
void UI_PageHome_Update(uint32_t elapsed_ms);
void UI_PageHome_HandleEvent(const ui_event_t *event);

#endif /* UI_PAGE_HOME_H */
