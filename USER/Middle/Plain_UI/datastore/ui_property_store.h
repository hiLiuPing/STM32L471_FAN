/*
 * 属性双缓冲存储。
 * backend buffer 负责接收写入，front buffer 负责给绘制读取。
 * 调用约定是“先写 backend，再在帧更新阶段同步到 front”。
 */
#ifndef UI_PROPERTY_STORE_H
#define UI_PROPERTY_STORE_H

#include "FreeRTOS.h"

#include <stdint.h>

#include "../config/ui_config.h"

typedef enum
{
  UI_PROP_NONE = 0,
  UI_PROP_INT,
  UI_PROP_FLOAT,
  UI_PROP_STR
} ui_prop_type_t;

typedef struct
{
  uint16_t property_id;
  ui_prop_type_t type;
  uint8_t is_dirty;
  union
  {
    int32_t val_int;
    float val_float;
    char val_str[UI_PROPERTY_STRING_LENGTH];
  } val;
} ui_prop_slot_t;

typedef struct
{
  ui_prop_slot_t slots[UI_MAX_PROPERTY_COUNT];
} ui_store_buffer_t;

void UI_PropertyStore_Init(void);
void UI_PropertyStore_SyncFront(void);
BaseType_t UI_PropertyStore_SetInt(uint16_t property_id, int32_t value);
BaseType_t UI_PropertyStore_SetFloat(uint16_t property_id, float value);
BaseType_t UI_PropertyStore_SetString(uint16_t property_id, const char *value);
const ui_prop_slot_t *UI_PropertyStore_GetFrontSlot(uint16_t property_id);
int32_t UI_PropertyStore_GetFrontInt(uint16_t property_id, int32_t default_value);
const char *UI_PropertyStore_GetFrontString(uint16_t property_id, const char *default_value);
uint8_t UI_PropertyStore_IsFrontDirty(uint16_t property_id);
void UI_PropertyStore_ClearFrontDirty(uint16_t property_id);

#endif /* UI_PROPERTY_STORE_H */
