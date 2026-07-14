#ifndef __EGUI_PORT_STM32L471_FAN_H__
#define __EGUI_PORT_STM32L471_FAN_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "core/egui_core.h"
#include "key.h"
#include <stdbool.h>

void egui_port_start(void);
void egui_port_poll(void);
egui_core_t *egui_port_get_core(void);
void egui_port_handle_key_event(const key_event_t *key_event);
void egui_port_set_display_power(bool on);

#ifdef __cplusplus
}
#endif

#endif /* __EGUI_PORT_STM32L471_FAN_H__ */
