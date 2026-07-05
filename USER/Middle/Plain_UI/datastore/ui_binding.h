#ifndef UI_BINDING_H
#define UI_BINDING_H

#include "ui_property_binding.h"

static inline void UI_Binding_Init(void)
{
  UI_PropertyBinding_Init();
}

static inline BaseType_t UI_Binding_BindRect(uint16_t property_id, const ui_rect_t *rect)
{
  return UI_PropertyBinding_BindRect(property_id, rect);
}

static inline void UI_Binding_UpdateDirtyRegions(void)
{
  UI_PropertyBinding_UpdateDirtyRegions();
}

#endif /* UI_BINDING_H */
