#ifndef UI_DIALOG_H
#define UI_DIALOG_H

#include "ui_popup.h"

static inline BaseType_t UI_Dialog_Show(const ui_rect_t *rect, const char *title, const char *message)
{
  return UI_Popup_Show(rect, title, message, 90U, NULL, NULL);
}

#endif /* UI_DIALOG_H */
