#include "lv_port_disp.h"

#include <stdbool.h>

#include "lcd.h"
#include "lcd_init.h"

#define LV_PORT_DISP_BUF_LINES 20U

static void disp_init(void);
static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);
static void disp_flush_dma_done(void *user_data);

static lv_disp_draw_buf_t s_disp_draw_buf;
static lv_color_t s_disp_buf[LCD_W * LV_PORT_DISP_BUF_LINES];
static volatile bool s_disp_flush_enabled = true;

void lv_port_disp_init(void)
{
    disp_init();

    lv_disp_draw_buf_init(&s_disp_draw_buf,
                          s_disp_buf,
                          NULL,
                          (uint32_t)(LCD_W * LV_PORT_DISP_BUF_LINES));

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_W;
    disp_drv.ver_res = LCD_H;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &s_disp_draw_buf;
    lv_disp_drv_register(&disp_drv);
}

void disp_enable_update(void)
{
    s_disp_flush_enabled = true;
}

void disp_disable_update(void)
{
    s_disp_flush_enabled = false;
}

static void disp_init(void)
{
    LCD_Init();
    LCD_Fill(0U, 0U, LCD_W, LCD_H, BLACK);
}

static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    if (!s_disp_flush_enabled)
    {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    if ((area->x1 < 0) || (area->y1 < 0) ||
        (area->x2 >= LCD_W) || (area->y2 >= LCD_H) ||
        (area->x2 < area->x1) || (area->y2 < area->y1))
    {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    if (!LCD_Color_Render_DMA((uint16_t)area->x1,
                              (uint16_t)area->y1,
                              (uint16_t)area->x2,
                              (uint16_t)area->y2,
                              (const uint16_t *)color_p,
                              disp_flush_dma_done,
                              disp_drv))
    {
        lv_disp_flush_ready(disp_drv);
    }
}

static void disp_flush_dma_done(void *user_data)
{
    lv_disp_drv_t *disp_drv = (lv_disp_drv_t *)user_data;

    if (disp_drv != NULL)
    {
        lv_disp_flush_ready(disp_drv);
    }
}
