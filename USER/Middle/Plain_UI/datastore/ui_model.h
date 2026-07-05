#ifndef UI_MODEL_H
#define UI_MODEL_H

#include "ui_property_store.h"

static inline void UI_Model_Init(void)
{
  UI_PropertyStore_Init();
}

static inline void UI_Model_Sync(void)
{
  UI_PropertyStore_SyncFront();
}

#endif /* UI_MODEL_H */
