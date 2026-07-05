#ifndef __LV_PORT_DISP_H__
#define __LV_PORT_DISP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

void lv_port_disp_init(void);
void disp_enable_update(void);
void disp_disable_update(void);

#ifdef __cplusplus
}
#endif

#endif /* __LV_PORT_DISP_H__ */
