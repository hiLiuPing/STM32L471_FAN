#include "user_HardwareInitTask.h"

#include "data_app.h"
#include "key.h"
#include "led_app.h"
#include "log.h"
#include "main.h"
#include "sensors_app.h"
#include "systemMonitor_app.h"

#include "lv_port_disp.h"
#include "lvgl.h"
#include "ui.h"
#include "uart_dma.h"
#include "usart.h"
#include "user_TasksInit.h"
#include "weather_app.h"

void HardwareInitTask(void *argument)
{
    (void)argument;

    log_init(&huart1);

    log_printf("start app");
    log_printf("step1: init leds...");
    LED_App_Init();
    log_printf("step2: key init...");
    Key_Init();
    log_printf("step3: app init...");
    (void)APP_Sensors_Init();
    DataApp_Init();
    UserMonitor_Init();
    uart_dma_init(&uart2_admin,
                  &huart2,
                  u2_dma_buf,
                  sizeof(u2_dma_buf),
                  u2_rb_buf,
                  sizeof(u2_rb_buf));
    log_printf("step4: lvgl init...");
    lv_init();
    lv_port_disp_init();
    ui_init();

    log_printf("step5: hw ready");
    User_Tasks_SetHardwareReady();
    log_printf("step6: delete self");
    vTaskDelete(NULL);
}
