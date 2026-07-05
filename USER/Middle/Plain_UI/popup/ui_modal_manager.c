#include "ui_modal_manager.h"

#include <string.h>

#include "../core/ui_dirty_region.h"
#include "../hal/backend_lvgl/ui_renderer_adapter.h"

typedef struct
{
  uint8_t is_active;
  ui_rect_t rect;
  char title[24];
  char message[56];
  uint16_t timeout_frames;
  ui_modal_event_fn event_cb;
  void *user_data;
} ui_modal_state_t;

static ui_modal_state_t g_modal;

void UI_ModalManager_Init(void)
{
  memset(&g_modal, 0, sizeof(g_modal));
}

BaseType_t UI_ModalManager_Show(const ui_rect_t *rect,
                                const char *title,
                                const char *message,
                                uint16_t timeout_frames,
                                ui_modal_event_fn event_cb,
                                void *user_data)
{
  if (rect == NULL)
  {
    return pdFAIL;
  }

  /* 弹窗对象只有一个活动实例，新的 Show 会覆盖旧弹窗。 */
  memset(&g_modal, 0, sizeof(g_modal));
  g_modal.is_active = 1U;
  g_modal.rect = *rect;
  g_modal.timeout_frames = timeout_frames;
  g_modal.event_cb = event_cb;
  g_modal.user_data = user_data;

  if (title != NULL)
  {
    strncpy(g_modal.title, title, sizeof(g_modal.title) - 1U);
  }
  if (message != NULL)
  {
    strncpy(g_modal.message, message, sizeof(g_modal.message) - 1U);
  }

  UI_DirtyRegion_Invalidate(&g_modal.rect);
  return pdPASS;
}

void UI_ModalManager_Dismiss(void)
{
  if (g_modal.is_active != 0U)
  {
    UI_DirtyRegion_Invalidate(&g_modal.rect);
  }

  memset(&g_modal, 0, sizeof(g_modal));
}

uint8_t UI_ModalManager_IsActive(void)
{
  return g_modal.is_active;
}

BaseType_t UI_ModalManager_HandleEvent(const ui_event_t *event)
{
  if (g_modal.is_active == 0U)
  {
    return pdFAIL;
  }

  /* 模态态下事件只给弹窗看，页面不再收到同一输入。 */
  if (g_modal.event_cb != NULL)
  {
    g_modal.event_cb(event, g_modal.user_data);
  }

  if ((event != NULL) &&
      ((event->type == UI_KEY_BACK) || (event->type == UI_KEY_ENTER) || (event->type == UI_KEY_CLICK)))
  {
    UI_ModalManager_Dismiss();
  }

  return pdPASS;
}

void UI_ModalManager_Process(void)
{
  if ((g_modal.is_active != 0U) && (g_modal.timeout_frames > 0U))
  {
    g_modal.timeout_frames--;
    if (g_modal.timeout_frames == 0U)
    {
      UI_ModalManager_Dismiss();
    }
  }
}

void UI_ModalManager_Draw(const ui_viewport_t *viewport)
{
  ui_render_style_t style;
  ui_text_style_t text_style;
  ui_rect_t title_rect;
  ui_rect_t message_rect;

  if ((viewport == NULL) || (g_modal.is_active == 0U))
  {
    return;
  }

  style.text = UI_RendererAdapter_Color(238U, 244U, 250U);
  style.muted = UI_RendererAdapter_Color(156U, 172U, 190U);
  style.background = UI_RendererAdapter_Color(15U, 26U, 39U);
  style.border = UI_RendererAdapter_Color(83U, 116U, 150U);
  style.accent = UI_RendererAdapter_Color(246U, 184U, 82U);
  style.radius = 4U;
  style.border_width = 1U;

  UI_RendererAdapter_DrawFillRect(viewport, g_modal.rect.x, g_modal.rect.y, g_modal.rect.width, g_modal.rect.height, style.background);
  UI_RendererAdapter_DrawRect(viewport, g_modal.rect.x, g_modal.rect.y, g_modal.rect.width, g_modal.rect.height, style.border);
  UI_RendererAdapter_DrawFillRect(viewport, g_modal.rect.x, g_modal.rect.y, 4, g_modal.rect.height, style.accent);

  text_style.color = style.text;
  text_style.font_size = UI_FONT_SIZE_12;
  text_style.align = UI_TEXT_ALIGN_LEFT;

  title_rect.x = g_modal.rect.x + 12;
  title_rect.y = g_modal.rect.y + 8;
  title_rect.width = g_modal.rect.width - 20;
  title_rect.height = 16;
  message_rect.x = g_modal.rect.x + 12;
  message_rect.y = g_modal.rect.y + 30;
  message_rect.width = g_modal.rect.width - 20;
  message_rect.height = 18;

  UI_RendererAdapter_DrawTextInRect(viewport, &title_rect, g_modal.title, &text_style);
  text_style.color = style.muted;
  UI_RendererAdapter_DrawTextInRect(viewport, &message_rect, g_modal.message, &text_style);
}
