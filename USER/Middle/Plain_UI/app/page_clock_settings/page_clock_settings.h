#ifndef UI_PAGE_CLOCK_SETTINGS_H
#define UI_PAGE_CLOCK_SETTINGS_H

#include "../../event/ui_event.h"
#include "../../widget/base/ui_widget.h"

ui_obj_t *UI_PageClockSettings_Create(ui_obj_t *parent);
void UI_PageClockSettings_Enter(void);
void UI_PageClockSettings_Exit(void);
void UI_PageClockSettings_Update(uint32_t elapsed_ms);
void UI_PageClockSettings_HandleEvent(const ui_event_t *event);

#endif /* UI_PAGE_CLOCK_SETTINGS_H */
