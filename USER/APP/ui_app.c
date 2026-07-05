/* 迁移期 facade：保留旧入口名，内部直接转到 Plain_UI。 */
#include "ui_app.h"

#include "../Plain_UI/app/ui_product_app.h"

BaseType_t UI_App_Init(void)
{
  return UI_ProductApp_Init();
}
