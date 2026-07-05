#ifndef UI_SNAPSHOT_H
#define UI_SNAPSHOT_H

#include "ui_property_store.h"

typedef ui_store_buffer_t ui_snapshot_t;

static inline const ui_prop_slot_t *UI_Snapshot_GetSlot(uint16_t property_id)
{
  return UI_PropertyStore_GetFrontSlot(property_id);
}

#endif /* UI_SNAPSHOT_H */
