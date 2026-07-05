#ifndef UI_DIRTY_REGION_H
#define UI_DIRTY_REGION_H

#include <stddef.h>

#include "../config/ui_config.h"
#include "ui_types.h"

void UI_DirtyRegion_Init(void);
void UI_DirtyRegion_Invalidate(const ui_rect_t *rect);
size_t UI_DirtyRegion_Take(ui_rect_t *rects, size_t max_rect_count);

#endif /* UI_DIRTY_REGION_H */
