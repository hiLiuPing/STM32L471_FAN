# STM32L471_FAN 仓库说明与 AI 编码约束

## 项目总览

本仓库是面向 STM32L471 的风扇控制器固件工程，不是纯 LVGL/Plain_UI 框架项目。实际结构为：

- STM32CubeMX/HAL 生成的 MCU 工程基础。
- FreeRTOS 多任务应用。
- `USER/Middle/EmbeddedGUI` 图形中间件。
- `USER/GUI` 基于 EmbeddedGUI 的业务页面。
- `USER/APP` 风扇、传感器、天气、数据、EEPROM、LED、PSRAM、字体和资源等业务逻辑。
- `USER/BSP` 板级外设驱动。

图形栈以 EmbeddedGUI 的 `egui_*` API 为核心，业务页面包括启动页、首页、风扇页、天气页和设置页。请不要按 Plain_UI、LVGL backend、Metro widget/page_xxx 那套目录假设来生成代码。

## 当前目录结构

```text
STM32L471_FAN/
├─ Core/                         # STM32CubeMX 生成的 HAL 初始化、中断、外设入口
│  ├─ Inc/
│  └─ Src/
├─ Drivers/                      # CMSIS 与 STM32L4 HAL Driver
├─ MDK-ARM/                      # Keil MDK 工程文件
│  └─ STM32L471_FAN.uvprojx
├─ Tools/                        # 工具脚本或辅助资源
├─ USER/
│  ├─ APP/                       # 业务应用层
│  │  ├─ fan_app.*               # 风扇控制业务
│  │  ├─ data_app.*              # 应用数据中心
│  │  ├─ weather_app.*           # 天气数据业务
│  │  ├─ sensors_app.*           # 传感器采集业务
│  │  ├─ led_app.*               # LED 状态业务
│  │  ├─ eeprom_app.*            # EEPROM 数据业务
│  │  ├─ psram_app.*             # PSRAM 初始化封装
│  │  ├─ poetry_app.*            # 诗词数据业务
│  │  └─ *_res.* / icons.*       # GUI 静态资源代码
│  ├─ BSP/                       # 板级驱动与芯片寄存器封装
│  │  ├─ lcd.* / lcd_init.*      # LCD 驱动
│  │  ├─ key.*                   # 按键驱动
│  │  ├─ rgb_led.* / multi_led.* # LED 驱动
│  │  ├─ qspi_psram.*            # QSPI PSRAM
│  │  ├─ spi_flash.*             # SPI Flash
│  │  ├─ ee24.*                  # EEPROM
│  │  ├─ *_reg.*                 # BQ24295、INA226、LIS3DH、MAX17048、SHT40 等器件寄存器封装
│  │  └─ log.*                   # 串口日志
│  ├─ GUI/                       # EmbeddedGUI 业务 UI 层
│  │  ├─ ui.*                    # UI 注册入口
│  │  ├─ page_manager.*          # 业务页面管理器
│  │  ├─ ui_common.h             # UI 公共定义
│  │  ├─ ui_poetry_popup.*       # 诗词弹窗
│  │  └─ screens/
│  │     ├─ ui_StartPage.*
│  │     ├─ ui_HomePage.*
│  │     ├─ ui_FanPage.*
│  │     ├─ ui_WeatherPage.*
│  │     └─ ui_SettingPage.*
│  ├─ Middle/
│  │  ├─ EmbeddedGUI/            # EmbeddedGUI 框架源码、平台适配和第三方渲染库
│  │  ├─ FreeRTOS/               # FreeRTOS 内核源码
│  │  └─ Transfer/               # I2C 总线、UART DMA、littlefs、ring buffer 等通用中间件
│  ├─ Res/                       # 字库、天气图片、诗词索引等离线资源
│  └─ TASK/                      # FreeRTOS 任务入口与任务间同步
│     ├─ user_TasksInit.*        # 任务、队列、信号量创建
│     ├─ user_HardwareInitTask.* # 硬件与应用初始化任务
│     ├─ user_EGUITask.*         # EmbeddedGUI 轮询任务
│     ├─ user_FanTask.*
│     ├─ user_KeyTask.*
│     ├─ user_KeyManllegeTask.*
│     ├─ user_LEDTask.*
│     ├─ user_AppDataTask.*
│     ├─ user_TransmitTask.*
│     └─ user_WeatherSyncTask.*
├─ STM32L471_FAN.ioc             # 当前 CubeMX 配置
└─ STM32L471_FAN.code-workspace
```

## 运行与初始化链路

1. `Core/Src/main.c` 执行 HAL 初始化和 `MX_*` 外设初始化。
2. `main.c` 调用 `User_Tasks_Init()` 创建 FreeRTOS 任务、队列和信号量。
3. `HardwareInitTask` 初始化日志、LED、风扇、按键、传感器、数据、UART DMA、SPI Flash、littlefs、PSRAM 和 EmbeddedGUI port。
4. 硬件准备完成后设置 `g_system_hw_ready`。
5. `EGUIHandlerTask` 等待硬件就绪后循环调用 `egui_port_poll()`。
6. `USER/GUI/ui.c` 注册业务页面，并由 `page_manager` 管理页面加载、切换和按键分发。

## 架构边界

- `Core/` 和 `Drivers/` 主要由 CubeMX、HAL 和 CMSIS 维护。除外设配置确实变化外，不要手工大改生成代码。
- `USER/Middle/EmbeddedGUI` 是 GUI 框架层，包含 `egui_*` 核心、控件、动画、字体、图片、资源、平台适配和第三方渲染库。
- `USER/GUI` 是本产品的页面层，只写风扇控制器相关 UI，不把通用 GUI 框架逻辑塞到页面里。
- `USER/APP` 是业务状态和业务服务层，页面读取或触发业务时优先通过这里的接口完成。
- `USER/BSP` 是板级外设驱动层，业务页面不要直接散落 HAL 寄存器操作。
- `USER/TASK` 是任务调度层，跨任务通信优先使用已有队列、信号量和 APP 层接口。
- `USER/Res` 放离线资源文件，生成后的 C 资源应放在合适的 `USER/APP` 或 GUI 资源文件中。

## 编码约束

- 全部使用 C 语言风格，头文件必须有 include guard，并保持现有命名风格。
- 新增 EmbeddedGUI UI 代码优先使用 `egui_*` API 和现有页面管理器。
- 新页面放在 `USER/GUI/screens/`，并在 `USER/GUI/ui.c` 的页面表注册。
- 页面公共行为放在 `USER/GUI/page_manager.*` 或 `ui_common.h`，不要在各页面复制大量状态机。
- 业务数据、传感器数据、天气数据、风扇状态等优先放在 `USER/APP` 对应模块，不要在页面文件里堆全局变量。
- 外设驱动和芯片寄存器封装放在 `USER/BSP`，应用逻辑通过 `USER/APP` 调用。
- 新增 FreeRTOS 任务需要在 `USER/TASK/user_TasksInit.*` 中集中创建和管理句柄。
- 任务间通信优先复用现有队列、信号量、`User_Tasks_WaitForHardwareReady()` 和 APP 层接口。
- LCD、QSPI、SPI Flash、EEPROM、传感器、充电/电量芯片等底层驱动无明确硬件需求不随意改。
- 修改 CubeMX 相关外设配置时，同步检查 `STM32L471_FAN.ioc`、`Core/`、`MDK-ARM/` 工程配置。
- 不要引入 LVGL/Plain_UI 专属目录、类型或依赖，除非用户明确要求迁移图形栈。
- 不要把 `USER/Middle/EmbeddedGUI` 当作业务页面目录；框架改动应保持通用性。

## 常见开发入口

- 调整页面布局：`USER/GUI/screens/ui_*.c`
- 注册或切换页面：`USER/GUI/ui.c`、`USER/GUI/page_manager.*`
- 改风扇业务：`USER/APP/fan_app.*`、`USER/TASK/user_FanTask.*`
- 改天气业务：`USER/APP/weather_app.*`、`USER/TASK/user_WeatherSyncTask.*`
- 改数据同步：`USER/APP/data_app.*`、`USER/TASK/user_AppDataTask.*`
- 改串口通信：`USER/Middle/Transfer/uart_dma.*`、`USER/TASK/user_TransmitTask.*`
- 改 LCD/显示底层：`USER/BSP/lcd.*`、`USER/BSP/lcd_init.*`、`USER/Middle/EmbeddedGUI/porting/stm32l471_fan/`
- 改资源：`USER/Res/`、`USER/APP/icons.*`、`USER/APP/home_scene_res.*`

## 禁止误用的旧描述

以下描述不适用于当前仓库：

- 纯 LVGL 工程。
- Plain_UI 框架。
- `Plain_UI/config/core/event/datastore/widget/navigation/hal/backend_lvgl` 分层。
- `app/page_home`、`page_music`、`page_playlist` 等页面目录。
- STM32/ESP32 双平台 LVGL HAL backend。

当前项目应按 STM32L471 固件、EmbeddedGUI 中间件、业务页面和 FreeRTOS 多任务应用来理解和维护。
