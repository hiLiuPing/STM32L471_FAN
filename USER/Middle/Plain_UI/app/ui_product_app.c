/* 产品 UI 的装配入口：把状态、注册表和页面管理器串起来。 */
#include "ui_product_app.h"

#include "../navigation/ui_page_registry.h"
#include "ui_app_state.h"

#include "../navigation/ui_page_manager.h"

BaseType_t UI_ProductApp_Init(void)
{
  UI_AppState_Init();
  UI_PageRegistry_Init();
  UI_PageManager_Init(UI_PageRegistry_GetPage);
  return UI_PageManager_OpenRoot(UI_APP_PAGE_HOME_ID);
}
