#include "ui_update.h"

#include "../datastore/ui_binding.h"
#include "../datastore/ui_model.h"
#include "../animation/ui_animation.h"
#include "../core/ui_dirty_region.h"
#include "../effects/ui_fx_emitter.h"
#include "../layer/ui_layer_manager.h"
#include "../navigation/ui_page_manager.h"
#include "../popup/ui_popup.h"
#include "../hal/backend_lvgl/ui_renderer_adapter.h"
#include "../app/ui_app_state.h"

static void UI_Update_DrawBaseLayer(const ui_viewport_t *viewport, void *user_data)
{
  (void)viewport;
  (void)user_data;
  /* Base 层只画当前页面主体，不掺杂弹窗和特效。 */
  UI_PageManager_DrawActivePage();
}

static void UI_Update_DrawClimateLayer(const ui_viewport_t *viewport, void *user_data)
{
  (void)viewport;
  (void)user_data;
}

static void UI_Update_DrawFxLayer(const ui_viewport_t *viewport, void *user_data)
{
  (void)user_data;
  /* FX 层独立绘制，避免粒子和页面内容互相污染。 */
  UI_FXEmitter_Draw(viewport);
}

static void UI_Update_DrawTopLayer(const ui_viewport_t *viewport, void *user_data)
{
  (void)user_data;
  /* 顶层只放模态弹窗和 toast，保证输入拦截和视觉优先级。 */
  UI_Popup_Draw(viewport);
}

void UI_Update_Init(void)
{
  UI_LayerManager_SetDrawCallback(UI_LAYER_BASE, UI_Update_DrawBaseLayer, NULL);
  UI_LayerManager_SetDrawCallback(UI_LAYER_CLIMATE, UI_Update_DrawClimateLayer, NULL);
  UI_LayerManager_SetDrawCallback(UI_LAYER_FX_PARTICLE, UI_Update_DrawFxLayer, NULL);
  UI_LayerManager_SetDrawCallback(UI_LAYER_TOP_WINDOW, UI_Update_DrawTopLayer, NULL);
}

void UI_Update_RunFrame(void)
{
  const ui_app_state_t *state = UI_AppState_GetConst();

  /* 这一段是“先同步、再调度、后绘制”的标准帧流程。 */
  UI_Model_Sync();
  UI_Binding_UpdateDirtyRegions();
  UI_PageManager_ProcessActivePage();
  UI_AnimationScheduler_Update(UI_FRAME_TICK_MS);
  UI_FXEmitter_Update();
  UI_Popup_Process();

  UI_RendererAdapter_BeginFrame();
  UI_LayerManager_Draw(&state->viewport);
  UI_RendererAdapter_EndFrame();
}
