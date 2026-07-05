#ifndef UI_FOCUS_ENGINE_H
#define UI_FOCUS_ENGINE_H

#include "FreeRTOS.h"

#include "../event/ui_event.h"
#include "../widget/base/ui_widget.h"

void UI_FocusEngine_Init(void);
void UI_FocusEngine_SetRoot(ui_widget_t *root_widget);
ui_widget_t *UI_FocusEngine_GetFocused(void);
BaseType_t UI_FocusEngine_HandleEvent(const ui_event_t *event);

#endif /* UI_FOCUS_ENGINE_H */
