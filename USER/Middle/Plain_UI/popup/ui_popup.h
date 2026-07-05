#ifndef UI_POPUP_H
#define UI_POPUP_H

#include "ui_modal_manager.h"

static inline void UI_Popup_Init(void)
{
  UI_ModalManager_Init();
}

static inline BaseType_t UI_Popup_Show(const ui_rect_t *rect,
                                       const char *title,
                                       const char *message,
                                       uint16_t timeout_frames,
                                       ui_modal_event_fn event_cb,
                                       void *user_data)
{
  return UI_ModalManager_Show(rect, title, message, timeout_frames, event_cb, user_data);
}

static inline void UI_Popup_Dismiss(void)
{
  UI_ModalManager_Dismiss();
}

static inline uint8_t UI_Popup_IsActive(void)
{
  return UI_ModalManager_IsActive();
}

static inline BaseType_t UI_Popup_HandleEvent(const ui_event_t *event)
{
  return UI_ModalManager_HandleEvent(event);
}

static inline void UI_Popup_Process(void)
{
  UI_ModalManager_Process();
}

static inline void UI_Popup_Draw(const ui_viewport_t *viewport)
{
  UI_ModalManager_Draw(viewport);
}

#endif /* UI_POPUP_H */
