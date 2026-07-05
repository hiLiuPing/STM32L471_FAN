#ifndef UI_PAGE_FAN_STATUS_H
#define UI_PAGE_FAN_STATUS_H

#include "../../event/ui_event.h"
#include "../../widget/base/ui_widget.h"

ui_obj_t *UI_PageFanStatus_Create(ui_obj_t *parent);
void UI_PageFanStatus_Enter(void);
void UI_PageFanStatus_Exit(void);
void UI_PageFanStatus_Update(uint32_t elapsed_ms);
void UI_PageFanStatus_HandleEvent(const ui_event_t *event);

#endif /* UI_PAGE_FAN_STATUS_H */
