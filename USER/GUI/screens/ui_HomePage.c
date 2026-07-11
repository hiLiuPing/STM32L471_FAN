#include "screens/ui_HomePage.h"

#include <stdio.h>
#include "ui.h"

#include "poetry_app.h"
#include "heiti_lvgl_font.h"
#include "log.h"
#include <stdlib.h>

/* Poem section — screen is 428×142 (landscape) */
#define HOME_PAGE_POEM_INTERVAL   10U
#define HOME_PAGE_POEM_Y0         14U
#define HOME_PAGE_POEM_X_RIGHT    4U
#define HOME_PAGE_POEM_W          220U
#define HOME_PAGE_CN_TEST_TEXT    "\xe4\xb8\xad\xe6\x96\x87\xe6\xb5\x8b\xe8\xaf\x95\n\xe9\xa3\x8e\xe6\x89\x87\xe5\xa4\xa9\xe6\xb0\x94\nLVGL 123"

lv_obj_t *ui_HomePage = NULL;

/* Poem state */
static HeitiFont_Context_t s_heiti_ctx;
static const lv_font_t *s_heiti_lvgl = NULL;
static lv_obj_t *s_poem_headline = NULL;
static lv_obj_t *s_poem_label = NULL;
static lv_timer_t *s_refresh_timer = NULL;
static uint8_t s_poem_buf[POETRY_APP_MAX_TEXT_SIZE];
static uint32_t s_poem_tick = 0;

static void ui_HomePage_refresh(void)
{
    /* Poem update (every ~10 s) */
    s_poem_tick++;
    if (s_poem_tick >= HOME_PAGE_POEM_INTERVAL && s_heiti_lvgl != NULL)
    {
        s_poem_tick = 0;
        PoetryApp_Poem_t poem;
        int ret = PoetryApp_OpenCollection(POETRY_COLL_TANG_300);

        if (ret == POETRY_APP_OK)
        {
            ret = PoetryApp_GetRandomPoem(POETRY_COLL_TANG_300,
                                           s_poem_buf,
                                           sizeof(s_poem_buf),
                                           &poem);
            if (ret == POETRY_APP_OK && s_poem_label != NULL)
            {
                char text[256];
                uint16_t pos = 0;
                for (uint8_t i = 0U; i < poem.line_count && pos < sizeof(text) - 2; i++)
                {
                    if (i > 0 && pos < sizeof(text) - 1)
                    {
                        text[pos++] = '\n';
                    }
                    const char *line = poem.lines[i];
                    while (*line != '\0' && pos < sizeof(text) - 1)
                    {
                        text[pos++] = *line++;
                    }
                }
                text[pos] = '\0';
                lv_label_set_text(s_poem_label, text);
            }
            PoetryApp_CloseCollection(POETRY_COLL_TANG_300);
        }
    }
}

static void ui_HomePage_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_HomePage_refresh();
}

void ui_HomePage_screen_init(void)
{
    if (ui_HomePage != NULL)
    {
        return;
    }

    ui_HomePage = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_HomePage, lv_color_hex(0xF4F7FB), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_HomePage, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    /* Font init */
    HeitiFont_Result_t font_ret = HeitiFont_Open(&s_heiti_ctx, "heiti_4_16.bin");
    if (font_ret == HEITI_FONT_OK)
    {
        s_heiti_lvgl = HeitiLvgl_Register(&s_heiti_ctx);
        if (s_heiti_lvgl == NULL)
        {
            log_printf("[HOME] HeitiLvgl_Register FAIL");
        }
    }
    else
    {
        log_printf("[HOME] HeitiFont_Open FAIL ret=%d", (int)font_ret);
    }

    srand(lv_tick_get());

    /* Poem headline */
    s_poem_headline = lv_label_create(ui_HomePage);
    lv_label_set_text(s_poem_headline, "\xe2\x94\x80 POEM \xe2\x94\x80");
    lv_obj_set_style_text_color(s_poem_headline,
                                 lv_color_hex(0x9CA3AF), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s_poem_headline,
                                 &lv_font_montserrat_12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(s_poem_headline, LV_ALIGN_TOP_RIGHT, -(int16_t)HOME_PAGE_POEM_X_RIGHT, 2);

    /* Poem content */
    s_poem_label = lv_label_create(ui_HomePage);
    lv_label_set_text(s_poem_label, (s_heiti_lvgl != NULL) ? HOME_PAGE_CN_TEST_TEXT : "Heiti font load failed");
    lv_obj_set_width(s_poem_label, HOME_PAGE_POEM_W);
    lv_label_set_long_mode(s_poem_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(s_poem_label,
                                 lv_color_hex(0x374151), LV_PART_MAIN | LV_STATE_DEFAULT);
    if (s_heiti_lvgl != NULL)
    {
        lv_obj_set_style_text_font(s_poem_label,
                                    s_heiti_lvgl, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
    lv_obj_set_style_text_align(s_poem_label,
                                 LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_align(s_poem_label, LV_ALIGN_TOP_RIGHT,
                 -(int16_t)HOME_PAGE_POEM_X_RIGHT,
                 HOME_PAGE_POEM_Y0);

    ui_HomePage_refresh();
    s_refresh_timer = lv_timer_create(ui_HomePage_timer_cb, 1000U, NULL);
}

void ui_HomePage_screen_destroy(void)
{
    if (s_refresh_timer != NULL)
    {
        lv_timer_del(s_refresh_timer);
        s_refresh_timer = NULL;
    }

    s_poem_headline = NULL;
    s_poem_label = NULL;
    s_heiti_lvgl = NULL;
    HeitiFont_Close(&s_heiti_ctx);
    ui_HomePage = NULL;
}

void ui_HomePage_key_handler(void *key_event)
{
    ui_page_handle_default_key_event(key_event);
}
