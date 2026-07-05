#ifndef UI_PAGE_FAN_SETTINGS_H
#define UI_PAGE_FAN_SETTINGS_H

#include "../../event/ui_event.h"
#include "../../widget/base/ui_widget.h"

ui_obj_t *UI_PageFanSettings_Create(ui_obj_t *parent);
void UI_PageFanSettings_Enter(void);
void UI_PageFanSettings_Exit(void);
void UI_PageFanSettings_Update(uint32_t elapsed_ms);
void UI_PageFanSettings_HandleEvent(const ui_event_t *event);

#endif /* UI_PAGE_FAN_SETTINGS_H */
