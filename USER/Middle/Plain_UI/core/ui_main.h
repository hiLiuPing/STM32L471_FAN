/*
 * UI 主入口。
 * 只负责初始化 Plain_UI 运行时并创建 UI 任务。
 * 业务层通过 facade 间接调用它，不直接依赖具体 backend。
 */
#ifndef UI_MAIN_H
#define UI_MAIN_H

#include "FreeRTOS.h"

BaseType_t UI_Main_Init(void);

#endif /* UI_MAIN_H */
