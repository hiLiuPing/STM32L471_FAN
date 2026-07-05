#include "ui_update.h"

#include "../animation/ui_animation.h"
#include "../config/ui_config.h"
#include "../datastore/ui_binding.h"
#include "../datastore/ui_model.h"
#include "../effects/ui_weather_effect.h"
#include "../navigation/ui_page_manager.h"
#include "../popup/ui_popup.h"

#include "lvgl.h"

void UI_Update_Init(void)
{
}

void UI_Update_RunFrame(void)
{
  UI_Model_Sync();
  UI_Binding_UpdateDirtyRegions();
  UI_PageManager_Update();
  UI_AnimationScheduler_Update(UI_FRAME_TICK_MS);
  UI_WeatherEffect_Update(UI_FRAME_TICK_MS);
  UI_Popup_Process();
  (void)lv_timer_handler();
}
