#include "user_HardwareInitTask.h"

#include "data_app.h"
#include "fan_app.h"
#include "key.h"
#include "led_app.h"
#include "lptim.h"
#include "lfs_port.h"
#include "log.h"
#include "main.h"
#include "psram_app.h"
#include "sensors_app.h"
#include "settings_app.h"
#include "spi_flash.h"
#include "systemMonitor_app.h"

#include "egui_port_stm32l471_fan.h"
#include "uart_dma.h"
#include "usart.h"
#include "user_TasksInit.h"
#include "weather_app.h"

void HardwareInitTask(void *argument)
{
    (void)argument;

    log_init(&huart1);
    Weather_PowerOn();
    log_printf("start app");
    log_printf("step1: init leds...");
    LED_App_Init();
    log_printf("step2: key init...");
    Key_Init();
    log_printf("step3: app init...");
    (void)APP_Sensors_Init();
    log_printf("step3.1: init fan...");
    FanApp_Init();
    DataApp_Init();
    SettingsApp_Init();
    UserMonitor_Init();


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
    }

    log_printf("step3.7: psram init...");
    if (PSRAM_App_Init() != 0)
    {
        log_printf("psram init FAIL");
    }
    else
    {
        log_printf("psram init OK");
    }

    log_printf("step4: egui init...");
    egui_port_start();
    if (LPTIM1_Start1sTick() != HAL_OK)
    {
        log_printf("lptim1 1s tick start FAIL");
    }
    uart_dma_init(&uart2_admin,
                  &huart2,
                  u2_dma_buf,
                  UART_Transmit_DMA_RX_SIZE,
                  u2_rb_buf,
                  UART_Transmit_LWRB_SIZE);
    log_printf("step5: hw ready");
    //  g_weather_module.first_sync_done = 0U;
    User_Tasks_SetHardwareReady();
    log_printf("step6: delete self");
    // Weather_FillDemoData();
    
    SettingsApp_ApplyActiveBrightness();
    log_printf("fill demo weather data");
    vTaskDelete(NULL);
}
