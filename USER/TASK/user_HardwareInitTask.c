#include "user_HardwareInitTask.h"

#include "data_app.h"
#include "fan_app.h"
#include "key.h"
#include "led_app.h"
#include "lfs_port.h"
#include "log.h"
#include "main.h"
#include "home_theme2_cloud_cache.h"
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
    uint32_t sensor_init_mask;
    (void)argument;

    log_init(&huart1);
   
    Weather_PowerOn();
    log_printf("start app");
     WeatherApp_Init();
    log_printf("step1: init leds...");
    LED_App_Init();
    log_printf("step2: key init...");
    Key_Init();
    log_printf("step3: app init...");
    log_printf("step3.0: init storage...");
    if (APP_Storage_Init() != 0)
    {
        log_printf("storage init FAIL");
    }
    else
    {
        log_printf("storage init OK");
    }
    sensor_init_mask = APP_Sensors_Init();
    if (sensor_init_mask != SENSORS_INIT_ALL_OK)
    {
        log_printf("[Sensors] init mask=0x%02lX missing=0x%02lX",
                   (unsigned long)sensor_init_mask,
                   (unsigned long)(SENSORS_INIT_ALL_OK & ~sensor_init_mask));
        if ((sensor_init_mask & SENSORS_INIT_MOTION_OK) == 0U) log_printf("[Sensors] LIS3DH init fail");
        if ((sensor_init_mask & SENSORS_INIT_ENV_OK) == 0U) log_printf("[Sensors] SHT40 init fail");
        if ((sensor_init_mask & SENSORS_INIT_BATTERY_OK) == 0U) log_printf("[Sensors] MAX17048 init fail");
        if ((sensor_init_mask & SENSORS_INIT_CHARGER_OK) == 0U) log_printf("[Sensors] BQ24295 init fail");
        if ((sensor_init_mask & SENSORS_INIT_INA226_OK) == 0U) log_printf("[Sensors] INA226 init fail");
    }
    log_printf("step3.1: init fan...");
    FanApp_Init();
    DataApp_Init();
    SettingsApp_Init();
    UserMonitor_Init();

    log_printf("step3.4: init psram/home cloud cache...");
    if (PSRAM_App_Init() != 0)
    {
        log_printf("psram init FAIL, dynamic cloud QOI fallback");
    }
    else if (!HomeTheme2CloudCache_Init())
    {
        log_printf("home cloud cache FAIL, dynamic cloud QOI fallback");
    }
    else
    {
        log_printf("home cloud cache OK");
    }

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

    log_printf("step4: egui init...");
    egui_port_start();
    uart_dma_init(&uart2_admin,
                  &huart2,
                  u2_dma_buf,
                  UART_Transmit_DMA_RX_SIZE,
                  u2_rb_buf,
                  UART_Transmit_LWRB_SIZE);
    log_printf("step5: hw ready");
    User_Tasks_SetHardwareReady();
    log_printf("step6: delete self");

    SettingsApp_ApplyActiveBrightness();
    MemDiag_LogSnapshot("hw-ready");
    HardwareInitTaskHandle = NULL;
    vTaskDelete(NULL);
}
