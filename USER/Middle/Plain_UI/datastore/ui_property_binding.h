/*
 * 属性到矩形区域的绑定层。
 * 它把属性脏标记翻译成局部失效区域，用来减少整屏刷新。
 * 绑定层只关心 property_id 和 rect，不关心具体控件类型。
 */
#ifndef UI_PROPERTY_BINDING_H
#define UI_PROPERTY_BINDING_H

#include "FreeRTOS.h"

#include <stdint.h>

#include "../core/ui_types.h"

void UI_PropertyBinding_Init(void);
BaseType_t UI_PropertyBinding_BindRect(uint16_t property_id, const ui_rect_t *rect);
void UI_PropertyBinding_UpdateDirtyRegions(void);

#endif /* UI_PROPERTY_BINDING_H */
