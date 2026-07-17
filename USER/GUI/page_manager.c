#include "page_manager.h"

#include <string.h>

#include "core/egui_timer.h"
#include "key.h"
#include "ui_poetry_popup.h"
#include "ui_shutdown_popup.h"
#include "ui_system_popup.h"
#include "widget/egui_view.h"

typedef struct
{
    egui_core_t *core;
    ui_page_t *pages[UI_PAGE_MANAGER_MAX_PAGES];
    uint8_t count;
    uint8_t current_index;
    uint32_t last_switch_tick;
    bool loaded;
} ui_page_manager_t;

static ui_page_manager_t s_page_manager;

static bool ui_page_manager_can_switch(void)
{
    uint32_t now;

    if (!s_page_manager.loaded)
    {
        return true;
    }

    now = egui_timer_get_current_time();
    return (uint32_t)(now - s_page_manager.last_switch_tick) >= UI_PAGE_SWITCH_ANIM_TIME_MS;
}

static void ui_page_manager_ensure_page(ui_page_t *page)
{
    if ((page == NULL) || page->initialized)
    {
        return;
    }

    if (page->init != NULL)
    {
        page->init();
    }

    if ((page->page_view != NULL) && (*page->page_view != NULL))
    {
        egui_core_add_user_root_view(*page->page_view);
        egui_view_set_visible(*page->page_view, 0);
        page->initialized = 1U;
    }
}

static void ui_page_manager_load_page(uint8_t index)
{
    ui_page_t *current_page;
    ui_page_t *target_page;

    if ((s_page_manager.core == NULL) ||
        (s_page_manager.count == 0U) ||
        (index >= s_page_manager.count) ||
        !ui_page_manager_can_switch())
    {
        return;
    }

    current_page = s_page_manager.loaded ? s_page_manager.pages[s_page_manager.current_index] : NULL;
    target_page = s_page_manager.pages[index];
    ui_page_manager_ensure_page(target_page);

    if ((target_page == NULL) || (target_page->page_view == NULL) || (*target_page->page_view == NULL))
    {
        return;
    }

    if ((current_page == target_page) && s_page_manager.loaded)
    {
        egui_view_invalidate_full(*target_page->page_view);
        egui_core_force_refresh(s_page_manager.core);
        return;
    }

    ui_poetry_popup_dismiss();

    if ((current_page != NULL) && (current_page->page_view != NULL) && (*current_page->page_view != NULL))
    {
        egui_view_set_visible(*current_page->page_view, 0);
        if (current_page->deinit != NULL)
        {
            current_page->deinit();
        }
    }

    egui_view_set_visible(*target_page->page_view, 1);
    egui_view_invalidate_full(*target_page->page_view);

    s_page_manager.current_index = index;
    s_page_manager.loaded = true;
    s_page_manager.last_switch_tick = egui_timer_get_current_time();
    egui_core_force_refresh(s_page_manager.core);
}

static bool ui_page_manager_find_nav_page(uint8_t start_index, int8_t direction, uint8_t *target_index)
{
    uint8_t index;

    if ((target_index == NULL) || (s_page_manager.count == 0U))
    {
        return false;
    }

    index = start_index;
    for (uint8_t i = 0U; i < s_page_manager.count; i++)
    {
        if (direction > 0)
        {
            index = (uint8_t)((index + 1U) % s_page_manager.count);
        }
        else
        {
            index = (index == 0U) ? (uint8_t)(s_page_manager.count - 1U) : (uint8_t)(index - 1U);
        }

        if ((s_page_manager.pages[index] != NULL) && s_page_manager.pages[index]->nav_enabled)
        {
            *target_index = index;
            return true;
        }
    }

    return false;
}

void ui_page_manager_init(egui_core_t *core)
{
    memset(&s_page_manager, 0, sizeof(s_page_manager));
    s_page_manager.core = core;
}

void ui_page_manager_register(ui_page_t *page)
{
    if ((page == NULL) || (s_page_manager.count >= UI_PAGE_MANAGER_MAX_PAGES))
    {
        return;
    }

    s_page_manager.pages[s_page_manager.count] = page;
    s_page_manager.count++;
}

void ui_page_manager_load_init(void)
{
    ui_page_manager_load_page(0U);
}

void ui_page_manager_next(void)
{
    uint8_t next_index;

    if (s_page_manager.count == 0U)
    {
        return;
    }

    if (ui_page_manager_find_nav_page(s_page_manager.current_index, 1, &next_index))
    {
        ui_page_manager_load_page(next_index);
    }
}

void ui_page_manager_prev(void)
{
    uint8_t prev_index;

    if (s_page_manager.count == 0U)
    {
        return;
    }

    if (ui_page_manager_find_nav_page(s_page_manager.current_index, -1, &prev_index))
    {
        ui_page_manager_load_page(prev_index);
    }
}

void ui_page_manager_goto(const char *page_name, uint8_t index)
{
    if (page_name != NULL)
    {
        bool found = false;

        for (uint8_t i = 0U; i < s_page_manager.count; i++)
        {
            if ((s_page_manager.pages[i] != NULL) &&
                (s_page_manager.pages[i]->name != NULL) &&
                (strcmp(s_page_manager.pages[i]->name, page_name) == 0))
            {
                index = i;
                found = true;
                break;
            }
        }

        if (!found)
        {
            return;
        }
    }

    ui_page_manager_load_page(index);
}

ui_page_t *ui_page_manager_get_current(void)
{
    if ((s_page_manager.count == 0U) || !s_page_manager.loaded)
    {
        return NULL;
    }

    return s_page_manager.pages[s_page_manager.current_index];
}

bool ui_page_manager_is_startup_active(void)
{
    ui_page_t *current = ui_page_manager_get_current();

    return (current != NULL) &&
           (current->name != NULL) &&
           (strcmp(current->name, "START") == 0);
}

static bool ui_page_manager_consume_global_key_event(void *key_event)
{
    const key_event_t *event = (const key_event_t *)key_event;

    if (event == NULL)
    {
        return false;
    }

    if ((event->id == KEY_ID_PWR) && (event->type == KEY_EVT_CLICK))
    {
        ui_page_manager_goto("HOME", 1U);
        return true;
    }

    return false;
}

void ui_page_manager_handle_key_event(void *key_event)
{
    const key_event_t *event = (const key_event_t *)key_event;
    ui_page_t *current_page = ui_page_manager_get_current();
    bool consumed = false;

    if (ui_page_manager_is_startup_active())
    {
        return;
    }

    if ((event != NULL) &&
        (event->id == KEY_ID_PWR) &&
        (event->type == KEY_EVT_LONG))
    {
        ui_shutdown_popup_start();
        return;
    }

    if (ui_shutdown_popup_is_active())
    {
        return;
    }

    if ((event != NULL) && ui_system_popup_is_visible())
    {
        bool allow_home_fan_key = false;

        if ((current_page != NULL) &&
            (current_page->name != NULL) &&
            (strcmp(current_page->name, "HOME") == 0) &&
            ui_system_popup_is_fan_control())
        {
            allow_home_fan_key = ((event->id == KEY_ID_OK) &&
                                  (event->type == KEY_EVT_CLICK)) ||
                                 (((event->id == KEY_ID_UP) ||
                                   (event->id == KEY_ID_DOWN)) &&
                                  ((event->type == KEY_EVT_CLICK) ||
                                   (event->type == KEY_EVT_LONG) ||
                                   (event->type == KEY_EVT_REPEAT)));
        }

        if (!allow_home_fan_key)
        {
            if (event->type == KEY_EVT_CLICK)
            {
                ui_system_popup_dismiss();
            }
            return;
        }
    }

    if ((event != NULL) &&
        (event->type == KEY_EVT_CLICK) &&
        ui_poetry_popup_is_visible())
    {
        ui_poetry_popup_dismiss();
        return;
    }

    if ((current_page != NULL) && (current_page->key_consume != NULL))
    {
        consumed = current_page->key_consume(key_event);
    }

    if (!consumed)
    {
        (void)ui_page_manager_consume_global_key_event(key_event);
    }
}

void ui_page_manager_service(void)
{
    ui_page_t *current_page = ui_page_manager_get_current();

    if ((current_page != NULL) && (current_page->service != NULL))
    {
        current_page->service();
    }
}

bool ui_page_consume_nav_key_event(void *key_event)
{
    const key_event_t *event = (const key_event_t *)key_event;

    if (event == NULL)
    {
        return false;
    }

    if ((event->type != KEY_EVT_CLICK) && (event->type != KEY_EVT_REPEAT))
    {
        return false;
    }

    if (event->id == KEY_ID_DOWN)
    {
        ui_page_manager_next();
        return true;
    }
    else if (event->id == KEY_ID_UP)
    {
        ui_page_manager_prev();
        return true;
    }

    return false;
}
