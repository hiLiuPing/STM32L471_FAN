/*
 * 产品 UI 装配入口。
 * 它负责把页面状态、页面注册表和页面管理器串起来。
 * 这里不做硬件初始化，只做业务 UI 的装配。
 */
#ifndef UI_PRODUCT_APP_H
#define UI_PRODUCT_APP_H

#include "FreeRTOS.h"

BaseType_t UI_ProductApp_Init(void);

#endif /* UI_PRODUCT_APP_H */
