#include "user_HardwareInitTask.h"

#include "data_app.h"
#include "heiti_font_drv.h"
#include "key.h"
#include "led_app.h"
#include "lfs_port.h"
#include "log.h"
#include "main.h"
#include "sensors_app.h"
#include "spi_flash.h"
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

    log_printf("step3.5: spi flash init...");
    if (spi_flash_init(&g_spi_flash, &hspi2, SPI2_CS_GPIO_Port, SPI2_CS_Pin) != 0)
    {
        log_printf("spi flash init FAIL");
    }
    else
    {
        log_printf("spi flash init OK");
    }

    log_printf("step3.6: littlefs mount...");
    if (lfs_port_init(&g_spi_flash) != 0)
    {
        log_printf("littlefs mount FAIL");
    }
    else
    {
        log_printf("littlefs mount OK");
        HeitiFont_Init();
    }
    log_printf("step4: lvgl init...");
    lv_init();
    lv_port_disp_init();
    ui_init();

    log_printf("step5: hw ready");
    User_Tasks_SetHardwareReady();
    log_printf("step6: delete self");
    vTaskDelete(NULL);
}
