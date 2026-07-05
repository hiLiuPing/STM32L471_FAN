#include "page_manager.h"

#include <stdbool.h>
#include <string.h>

#include "key.h"

typedef struct {
    ui_page_t *pages[UI_PAGE_MANAGER_MAX_PAGES];
    uint8_t count;
    uint8_t current_index;
    uint32_t last_switch_tick;
    bool loaded;
} ui_page_manager_t;

static ui_page_manager_t s_page_manager;

static bool ui_page_manager_can_switch(void)
{
    if (!s_page_manager.loaded)
    {
        return true;
    }

    return lv_tick_elaps(s_page_manager.last_switch_tick) >= UI_PAGE_SWITCH_ANIM_TIME_MS;
}

static void ui_page_manager_load_page(uint8_t index, lv_scr_load_anim_t anim)
{
    ui_page_t *current_page;
    ui_page_t *target_page;

    if ((s_page_manager.count == 0U) || (index >= s_page_manager.count))
    {
        return;
    }

    if (!ui_page_manager_can_switch())
    {
        return;
    }

    current_page = s_page_manager.loaded ? s_page_manager.pages[s_page_manager.current_index] : NULL;
    target_page = s_page_manager.pages[index];

    if ((current_page == target_page) && s_page_manager.loaded)
    {
        return;
    }

    if ((target_page->init != NULL) && ((target_page->page_obj == NULL) || (*target_page->page_obj == NULL)))
    {
        target_page->init();
    }

    if ((target_page->page_obj == NULL) || (*target_page->page_obj == NULL))
    {
        return;
    }

    lv_scr_load_anim(*target_page->page_obj, anim, UI_PAGE_SWITCH_ANIM_TIME_MS, 0U, true);

    if ((current_page != NULL) && (current_page->deinit != NULL))
    {
        current_page->deinit();
    }

    s_page_manager.current_index = index;
    s_page_manager.loaded = true;
    s_page_manager.last_switch_tick = lv_tick_get();
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

void ui_page_manager_init(void)
{
    memset(&s_page_manager, 0, sizeof(s_page_manager));
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
    ui_page_manager_load_page(0U, LV_SCR_LOAD_ANIM_FADE_ON);
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
        ui_page_manager_load_page(next_index, LV_SCR_LOAD_ANIM_MOVE_LEFT);
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
        ui_page_manager_load_page(prev_index, LV_SCR_LOAD_ANIM_MOVE_RIGHT);
    }
}

void ui_page_manager_goto(const char *page_name, uint8_t index)
{
    if (page_name != NULL)
    {
        bool found = false;

        for (uint8_t i = 0U; i < s_page_manager.count; i++)
        {
            if ((s_page_manager.pages[i]->name != NULL) &&
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

    ui_page_manager_load_page(index, LV_SCR_LOAD_ANIM_MOVE_LEFT);
}

ui_page_t *ui_page_manager_get_current(void)
{
    if ((s_page_manager.count == 0U) || !s_page_manager.loaded)
    {
        return NULL;
    }

    return s_page_manager.pages[s_page_manager.current_index];
}

void ui_page_manager_handle_key_event(void *key_event)
{
    ui_page_t *current_page = ui_page_manager_get_current();

    if (current_page == NULL)
    {
        return;
    }

    if (current_page->key_event_handler != NULL)
    {
        current_page->key_event_handler(key_event);
    }
}

void ui_page_handle_default_key_event(void *key_event)
{
    const key_event_t *event = (const key_event_t *)key_event;

    if (event == NULL)
    {
        return;
    }

    if ((event->type != KEY_EVT_CLICK) && (event->type != KEY_EVT_REPEAT))
    {
        return;
    }

    if (event->id == KEY_ID_R)
    {
        ui_page_manager_next();
    }
    else if (event->id == KEY_ID_N)
    {
        ui_page_manager_prev();
    }
}
