/**
 * @file lv_port_indev.c
 */
#if 1 

#include "lv_port_indev_template.h"
#include "../../lvgl.h"
#include "log.h"

#define DISPLAY_ON  1

// 确保定义了此变量，防止链接报错。这里先默认给 DISPLAY_ON
// volatile uint8_t g_app_current_state = DISPLAY_ON; 
extern uint8_t g_app_current_state;

typedef struct {
    bool is_pressed[3];
} hw_button_status_t;

extern volatile hw_button_status_t g_hw_buttons;
extern lv_obj_t * tileview;

lv_indev_t * indev_keypad = NULL;

static void keypad_init(void);
static void keypad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);

void lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;
    keypad_init();

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = keypad_read;

    indev_keypad = lv_indev_drv_register(&indev_drv);
}

static void keypad_init(void) {}

static void keypad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    (void)indev_drv;

    /* 1. 状态拦截 */
    if (g_app_current_state != DISPLAY_ON)
    {
        data->state = LV_INDEV_STATE_REL;
        data->key = 0;
        return;
    }

    /* 2. 按键映射修改：发送 PREV 和 NEXT 信号给 tileview 的事件回调 */
    
    /* KEY0 -> 上一页 (PREV) */
    if (g_hw_buttons.is_pressed[0])
    {
        data->state = LV_INDEV_STATE_PR;
        data->key = LV_KEY_PREV;  // ⚡ 改回 LV_KEY_PREV
        log_printf("KEY0 Pressed -> Send LV_KEY_PREV\r\n");
        return;
    }

    /* KEY1 -> 确认 (ENTER) */
    if (g_hw_buttons.is_pressed[1])
    {
        data->state = LV_INDEV_STATE_PR;
        data->key = LV_KEY_ENTER;
        log_printf("KEY1 Pressed -> Send LV_KEY_ENTER\r\n");
        return;
    }

    /* KEY2 -> 下一页 (NEXT) */
    if (g_hw_buttons.is_pressed[2])
    {
        data->state = LV_INDEV_STATE_PR;
        data->key = LV_KEY_NEXT;  // ⚡ 改回 LV_KEY_NEXT
        log_printf("KEY2 Pressed -> Send LV_KEY_NEXT\r\n");
        return;
    }

    /* 3. 无按键按下 */
    data->state = LV_INDEV_STATE_REL;
    data->key = 0;
}

#endif