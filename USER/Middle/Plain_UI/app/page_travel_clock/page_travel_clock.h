#ifndef UI_PAGE_TRAVEL_CLOCK_H
#define UI_PAGE_TRAVEL_CLOCK_H

#include "../../event/ui_event.h"
#include "../../widget/base/ui_widget.h"

ui_obj_t *UI_PageTravelClock_Create(ui_obj_t *parent);
void UI_PageTravelClock_Enter(void);
void UI_PageTravelClock_Exit(void);
void UI_PageTravelClock_Update(uint32_t elapsed_ms);
void UI_PageTravelClock_HandleEvent(const ui_event_t *event);

#endif /* UI_PAGE_TRAVEL_CLOCK_H */
