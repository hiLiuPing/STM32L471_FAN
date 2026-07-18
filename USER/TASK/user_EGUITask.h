#ifndef __USER_EGUITASK_H__
#define __USER_EGUITASK_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef enum
{
    EGUI_DISPLAY_STATE_WAKE = 0,
    EGUI_DISPLAY_STATE_SLEEP
} EGUI_DisplayState_t;

uint8_t EGUIHandlerTask_PostDisplayState(EGUI_DisplayState_t state);
void EGUIHandlerTask_ClearDisplayState(void);
void EGUIHandlerTask(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __USER_EGUITASK_H__ */
