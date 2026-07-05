#ifndef UI_WEATHER_EFFECT_H
#define UI_WEATHER_EFFECT_H

#include <stdint.h>

#include "../datastore/ui_model.h"
#include "../widget/base/ui_widget.h"

void UI_WeatherEffect_Init(ui_obj_t *parent);
void UI_WeatherEffect_SetMode(ui_weather_mode_t mode);
void UI_WeatherEffect_Start(void);
void UI_WeatherEffect_Stop(void);
void UI_WeatherEffect_Update(uint32_t elapsed_ms);

#endif /* UI_WEATHER_EFFECT_H */
