#include "ui_modal_manager.h"

#include <string.h>

#include "../core/ui_dirty_region.h"
#include "../hal/backend_lvgl/ui_renderer_adapter.h"

#include "lvgl.h"

typedef struct
{
  uint8_t is_active;
  ui_rect_t rect;
  char title[24];
  char message[56];
  uint16_t timeout_frames;
  ui_modal_event_fn event_cb;
  void *user_data;
  lv_obj_t *root;
  lv_obj_t *title_label;
  lv_obj_t *message_label;
} ui_modal_state_t;

static ui_modal_state_t g_modal;

void UI_ModalManager_Init(void)
{
  memset(&g_modal, 0, sizeof(g_modal));
}

static void UI_ModalManager_EnsureObjects(void)
{
  if (g_modal.root != NULL)
  {
    return;
  }

  g_modal.root = lv_obj_create(lv_layer_top());
  lv_obj_set_style_bg_color(g_modal.root, lv_color_make(15U, 26U, 39U), 0);
  lv_obj_set_style_bg_opa(g_modal.root, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(g_modal.root, lv_color_make(83U, 116U, 150U), 0);
  lv_obj_set_style_border_width(g_modal.root, 1, 0);
  lv_obj_set_style_radius(g_modal.root, 4, 0);
  lv_obj_clear_flag(g_modal.root, LV_OBJ_FLAG_SCROLLABLE);
  g_modal.title_label = lv_label_create(g_modal.root);
  lv_obj_set_pos(g_modal.title_label, 12, 8);
  lv_obj_set_style_text_color(g_modal.title_label, lv_color_make(238U, 244U, 250U), 0);
  g_modal.message_label = lv_label_create(g_modal.root);
  lv_obj_set_pos(g_modal.message_label, 12, 30);
  lv_obj_set_style_text_color(g_modal.message_label, lv_color_make(156U, 172U, 190U), 0);
  lv_obj_add_flag(g_modal.root, LV_OBJ_FLAG_HIDDEN);
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
  UI_ModalManager_EnsureObjects();
  g_modal.is_active = 1U;
  g_modal.rect = *rect;
  g_modal.timeout_frames = timeout_frames;
  g_modal.event_cb = event_cb;
  g_modal.user_data = user_data;
  g_modal.title[0] = '\0';
  g_modal.message[0] = '\0';

  if (title != NULL)
  {
    strncpy(g_modal.title, title, sizeof(g_modal.title) - 1U);
  }
  if (message != NULL)
  {
    strncpy(g_modal.message, message, sizeof(g_modal.message) - 1U);
  }

  if (g_modal.root != NULL)
  {
    lv_obj_set_pos(g_modal.root, g_modal.rect.x, g_modal.rect.y);
    lv_obj_set_size(g_modal.root, g_modal.rect.width, g_modal.rect.height);
    lv_label_set_text(g_modal.title_label, g_modal.title);
    lv_label_set_text(g_modal.message_label, g_modal.message);
    lv_obj_clear_flag(g_modal.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_modal.root);
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

  g_modal.is_active = 0U;
  g_modal.timeout_frames = 0U;
  g_modal.event_cb = NULL;
  g_modal.user_data = NULL;
  g_modal.title[0] = '\0';
  g_modal.message[0] = '\0';
  if (g_modal.root != NULL)
  {
    lv_obj_add_flag(g_modal.root, LV_OBJ_FLAG_HIDDEN);
  }
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
