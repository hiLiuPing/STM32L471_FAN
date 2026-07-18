#include "egui_port_stm32l471_fan.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include "core/egui_core.h"
#include "core/egui_display_driver.h"
#include "core/egui_platform.h"
#include "core/egui_timer.h"
#include "lcd.h"
#include "log.h"
#include "main.h"
#include "page_manager.h"
#include "settings_app.h"
#include "systemMonitor_app.h"
#include "ui.h"

static egui_core_t s_egui_core;
EGUI_CONFIG_PFB_BUFFER_DECLARE(s_egui_pfb);

static bool s_egui_started = false;
static bool s_egui_display_on = true;
static uint32_t s_egui_brightness_check_ms = 0U;

static void egui_port_assert_handler(const char *file, int line)
{
    log_printf("[EGUI] assert %s:%d", file, line);
    Error_Handler();
}

static void egui_port_delay(uint32_t ms)
{
    HAL_Delay(ms);
}

static uint32_t egui_port_get_tick_ms(void)
{
    return HAL_GetTick();
}

static egui_base_t egui_port_interrupt_disable(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    return (egui_base_t)primask;
}

static void egui_port_interrupt_enable(egui_base_t level)
{
    __set_PRIMASK((uint32_t)level);
}

static const egui_platform_ops_t s_egui_platform_ops = {
    .assert_handler = egui_port_assert_handler,
    .delay = egui_port_delay,
    .get_tick_ms = egui_port_get_tick_ms,
    .interrupt_disable = egui_port_interrupt_disable,
    .interrupt_enable = egui_port_interrupt_enable,
    .load_external_resource = NULL,
    .timer_start = NULL,
    .timer_stop = NULL,
};

static egui_platform_t s_egui_platform = {
    .ops = &s_egui_platform_ops,
};

static void egui_port_ui_init(egui_core_t *core)
{
    (void)core;

    ui_init();
}

static void egui_lcd_init(egui_core_t *core)
{
    (void)core;

    LCD_Init();
    LCD_Fill(0U, 0U, (uint16_t)(LCD_W - 1U), (uint16_t)(LCD_H - 1U), BLACK);
}

static void egui_lcd_dma_done(void *user_data)
{
    egui_core_t *core = (egui_core_t *)user_data;

    if ((core != NULL) && (core->render.pfb_mgr.pending_count > 0U))
    {
        egui_pfb_notify_flush_complete(core);
    }
}

static void egui_lcd_draw_area(egui_core_t *core,
                               int16_t x,
                               int16_t y,
                               int16_t w,
                               int16_t h,
                               const egui_color_int_t *data)
{
    int16_t x2;
    int16_t y2;

    if ((data == NULL) || (w <= 0) || (h <= 0))
    {
        egui_lcd_dma_done(core);
        return;
    }

    if ((x < 0) || (y < 0) || (x >= LCD_W) || (y >= LCD_H))
    {
        egui_lcd_dma_done(core);
        return;
    }

    x2 = (int16_t)(x + w - 1);
    y2 = (int16_t)(y + h - 1);
    if ((x2 >= LCD_W) || (y2 >= LCD_H))
    {
        egui_lcd_dma_done(core);
        return;
    }

    if (!LCD_Color_Render_DMA((uint16_t)x, (uint16_t)y, (uint16_t)x2, (uint16_t)y2, data, egui_lcd_dma_done, core))
    {
        LCD_Color_Render((uint16_t)x, (uint16_t)y, (uint16_t)x2, (uint16_t)y2, data);
        egui_lcd_dma_done(core);
    }
}

static void egui_lcd_wait_draw_complete(egui_core_t *core)
{
    (void)core;

    LCD_Color_Render_DMA_Wait();
}

static void egui_lcd_set_power(egui_core_t *core, uint8_t on)
{
    (void)core;

    s_egui_display_on = (on != 0U);
    if (on != 0U)
    {
        LCD_SetBacklightPercent(SettingsApp_GetActiveBrightnessPercent());
    }
    else
    {
        LCD_SetBacklightPercent(0U);
    }
}

static void egui_lcd_set_brightness(egui_core_t *core, uint8_t level)
{
    (void)core;

    if (s_egui_display_on)
    {
        uint8_t percent = (uint8_t)(((uint16_t)level * 100U + 127U) / 255U);
        LCD_SetBacklightPercent(percent);
    }
}

static const egui_display_driver_ops_t s_egui_lcd_ops = {
    .init = egui_lcd_init,
    .draw_area = egui_lcd_draw_area,
    .wait_draw_complete = egui_lcd_wait_draw_complete,
    .flush = NULL,
    .set_brightness = egui_lcd_set_brightness,
    .set_power = egui_lcd_set_power,
    .set_rotation = NULL,
    .fill_rect = NULL,
    .blit = NULL,
    .blend = NULL,
    .wait_vsync = NULL,
};

static egui_display_driver_t s_egui_lcd_driver = {
    .ops = &s_egui_lcd_ops,
    .physical_width = LCD_W,
    .physical_height = LCD_H,
    .rotation = EGUI_DISPLAY_ROTATION_0,
    .brightness = 255U,
    .power_on = 1U,
    .frame_sync_enabled = 0U,
    .frame_sync_ready = 1U,
    .user_data = NULL,
};

void egui_port_start(void)
{
    egui_color_int_t *pfb_buffers[EGUI_CONFIG_PFB_BUFFER_COUNT];
    egui_display_setup_t setup;
    uint8_t brightness_percent;

    if (s_egui_started)
    {
        return;
    }

    egui_platform_register(&s_egui_platform);

    for (int i = 0; i < EGUI_CONFIG_PFB_BUFFER_COUNT; i++)
    {
        pfb_buffers[i] = s_egui_pfb[i];
    }

    setup.screen_width = EGUI_CONFIG_SCREEN_WIDTH;
    setup.screen_height = EGUI_CONFIG_SCREEN_HEIGHT;
    setup.pfb_width = EGUI_CONFIG_PFB_WIDTH;
    setup.pfb_height = EGUI_CONFIG_PFB_HEIGHT;
    setup.pfb_buffers = pfb_buffers;
    setup.pfb_buffer_count = EGUI_CONFIG_PFB_BUFFER_COUNT;
    setup.display_driver = &s_egui_lcd_driver;
    setup.render_config = NULL;
    setup.touch_register = NULL;
    setup.uicode_init = egui_port_ui_init;
    setup.display_id = 0U;

    brightness_percent = SettingsApp_GetActiveBrightnessPercent();
    s_egui_lcd_driver.brightness = (uint8_t)(((uint16_t)brightness_percent * 255U + 50U) / 100U);
    egui_setup_display(&s_egui_core, &setup);

    s_egui_started = true;
    s_egui_display_on = true;
    s_egui_brightness_check_ms = egui_timer_get_current_time();
    UserMonitor_OnDisplayWake();
}

void egui_port_poll(void)
{
    uint32_t now;

    if (!s_egui_started)
    {
        return;
    }

    now = egui_timer_get_current_time();
    ui_page_manager_service();
    if (s_egui_display_on && ((uint32_t)(now - s_egui_brightness_check_ms) >= 60000U))
    {
        s_egui_brightness_check_ms = now;
        LCD_SetBacklightPercent(SettingsApp_GetActiveBrightnessPercent());
    }

    egui_polling_work(&s_egui_core);
}

egui_core_t *egui_port_get_core(void)
{
    return &s_egui_core;
}

void egui_port_handle_key_event(const key_event_t *key_event)
{
    if ((key_event == NULL) || !s_egui_started)
    {
        return;
    }

    Key_Event();
    if (!s_egui_display_on)
    {
        ui_page_manager_wake_to_home();
        egui_port_set_display_power(true);
        return;
    }

    ui_page_manager_handle_key_event((void *)key_event);
}

void egui_port_set_display_power(bool on)
{
    if (!s_egui_started)
    {
        if (on)
        {
            LCD_SetBacklightPercent(SettingsApp_GetActiveBrightnessPercent());
        }
        else
        {
            LCD_SetBacklightPercent(0U);
        }
        s_egui_display_on = on;
        if (on)
        {
            UserMonitor_OnDisplayWake();
        }
        else
        {
            UserMonitor_OnDisplaySleep();
        }
        return;
    }

    egui_display_set_power(&s_egui_core, on ? 1U : 0U);
    if (on)
    {
        s_egui_brightness_check_ms = egui_timer_get_current_time();
        egui_core_force_refresh(&s_egui_core);
        UserMonitor_OnDisplayWake();
    }
    else
    {
        UserMonitor_OnDisplaySleep();
    }
}

bool egui_port_is_display_on(void)
{
    return s_egui_display_on;
}
