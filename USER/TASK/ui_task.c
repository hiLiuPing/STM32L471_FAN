/* 迁移期 facade：旧任务入口保留，实际由 Plain_UI 主入口接管。 */
#include "ui_task.h"

#include "../Plain_UI/core/ui_main.h"

BaseType_t UI_Task_Create(void)
{
  return UI_Main_Init();
}
