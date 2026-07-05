#ifndef UI_TOAST_H
#define UI_TOAST_H

#include "ui_popup.h"

static inline BaseType_t UI_Toast_Show(const ui_rect_t *rect, const char *message)
{
  return UI_Popup_Show(rect, "Toast", message, 45U, NULL, NULL);
}

#endif /* UI_TOAST_H */
