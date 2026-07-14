# STM32L471_FAN

STM32L471_FAN 是一个基于 STM32L471 的风扇控制器固件工程。仓库实际形态是 STM32CubeMX/HAL + FreeRTOS + EmbeddedGUI + 业务页面，不是纯 LVGL 或 Plain_UI 框架项目。

## 主要能力

- 风扇控制、按键输入、RGB/多路 LED 状态控制。
- 温湿度、电量、电流/电压、姿态等传感器或电源管理器件接入。
- SPI Flash + littlefs、QSPI PSRAM、EEPROM 等存储支持。
- 基于 EmbeddedGUI 的启动页、首页、风扇页、天气页、设置页。
- 天气数据、诗词弹窗、字体和图片资源显示。
- FreeRTOS 多任务调度，包含硬件初始化、GUI 轮询、风扇、按键、LED、通信、数据和天气同步任务。

## 仓库结构

```text
STM32L471_FAN/
├─ Core/                    # CubeMX 生成的 HAL 初始化和外设代码
├─ Drivers/                 # CMSIS 与 STM32L4 HAL Driver
├─ .eide/                   # VSCode EIDE 插件工程配置
├─ .vscode/                 # VSCode 辅助任务配置
├─ MDK-ARM/                 # Keil MDK 工程，当前主要作为兼容/参考入口
├─ Tools/                   # 辅助工具
├─ USER/
│  ├─ APP/                  # 业务应用层：风扇、数据、天气、传感器、LED、资源等
│  ├─ BSP/                  # 板级驱动：LCD、按键、Flash、PSRAM、EEPROM、传感器芯片等
│  ├─ GUI/                  # EmbeddedGUI 业务页面和页面管理器
│  ├─ Middle/
│  │  ├─ EmbeddedGUI/       # EmbeddedGUI 框架源码、平台适配、第三方渲染库
│  │  ├─ FreeRTOS/          # FreeRTOS 内核
│  │  └─ Transfer/          # UART DMA、I2C bus、littlefs、ring buffer 等中间件
│  ├─ Res/                  # 字库、天气图片、诗词索引等资源
│  └─ TASK/                 # FreeRTOS 任务入口和同步对象管理
├─ STM32L471_FAN.ioc        # 当前 CubeMX 配置
└─ STM32L471_FAN.code-workspace
```

## 启动链路

1. `Core/Src/main.c` 完成 HAL 和 `MX_*` 外设初始化。
2. `User_Tasks_Init()` 创建 FreeRTOS 任务、队列和信号量。
3. `HardwareInitTask` 初始化日志、LED、风扇、按键、传感器、数据、UART DMA、SPI Flash、littlefs、PSRAM 和 EmbeddedGUI port。
4. `EGUIHandlerTask` 等待硬件就绪后周期调用 `egui_port_poll()`。
5. `USER/GUI/ui.c` 注册启动页、首页、风扇页、天气页和设置页，由 `page_manager` 负责页面加载、切换和按键分发。

## 图形界面

GUI 框架位于 `USER/Middle/EmbeddedGUI`，统一使用 `egui_*` API。业务页面位于 `USER/GUI/screens`：

- `ui_StartPage.*`
- `ui_HomePage.*`
- `ui_FanPage.*`
- `ui_WeatherPage.*`
- `ui_SettingPage.*`

新增页面时，将页面源文件放入 `USER/GUI/screens/`，并在 `USER/GUI/ui.c` 的页面表中注册。

## 构建方式

- VSCode + EIDE：当前首选构建方式。打开 `STM32L471_FAN.code-workspace`，通过 EIDE 插件的 Project Explorer 执行 Build、Rebuild、Clean 和 Download。
- EIDE 配置：工程配置位于 `.eide/eide.yml`，文件选项位于 `.eide/files.options.yml`。新增或移除源码、头文件路径、宏定义时需要同步检查这些配置。
- Keil MDK：`MDK-ARM/STM32L471_FAN.uvprojx` 保留为兼容或参考入口，不是当前主要编译入口。
- CubeMX：使用 `STM32L471_FAN.ioc` 维护外设配置。修改外设后需要同步检查生成代码、EIDE 配置、Keil 工程配置和 `USER` 层适配。

## 开发约定

- `Core/`、`Drivers/` 尽量保持 CubeMX/HAL 生成代码风格。
- 页面逻辑放在 `USER/GUI`，业务状态和服务放在 `USER/APP`，板级驱动放在 `USER/BSP`。
- 任务创建和同步对象集中维护在 `USER/TASK/user_TasksInit.*`。
- 页面不要直接操作底层 HAL 寄存器，优先调用 APP 或 BSP 层接口。
- 不要按 Plain_UI/LVGL backend 的旧描述新增目录或类型；当前项目图形栈是 EmbeddedGUI。
