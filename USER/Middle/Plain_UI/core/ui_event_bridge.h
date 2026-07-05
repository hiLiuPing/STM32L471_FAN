/*
 * 事件桥。
 * 负责把输入事件队列和消息总线统一转交给页面管理器。
 * 它本身不做业务判断，只做搬运和分发。
 */
#ifndef UI_EVENT_BRIDGE_H
#define UI_EVENT_BRIDGE_H

void UI_EventBridge_Pump(void);

#endif /* UI_EVENT_BRIDGE_H */
