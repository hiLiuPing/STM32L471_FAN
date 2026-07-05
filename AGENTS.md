项目总览
项目为基于 LVGL、面向 STM32/ESP32 嵌入式平台的 Metro 风格 UI 框架 Plain_UI，采用分层解耦架构，分离底层驱动、UI 核心、业务页面、硬件抽象层，内置事件总线、数据绑定、动画调度、图层、弹窗、虚拟列表、页面导航全套能力。
Plain_UI/
├─ config/                       # 全局配置中心
│  ├─ ui_config.h                # UI尺寸、缓存大小、FPS、动画开关
│  ├─ ui_macro.h                 # 常用宏、断言、容器宏
│  └─ ui_version.h               # 版本信息
├─ core/                         # UI核心驱动层
│  ├─ ui_main.c                  # UI初始化入口
│  ├─ ui_main.h
│  ├─ ui_update.c                # UI逻辑更新
│  ├─ ui_update.h
│  ├─ ui_tick.c                  # 系统时基驱动
│  ├─ ui_tick.h
│  ├─ ui_event_bridge.c          # LVGL事件桥接
│  └─ ui_event_bridge.h
├─ event/                        # 全局事件总线
│  ├─ ui_event_type.h            # 统一事件枚举
│  ├─ ui_event_bus.c             # 发布订阅机制
│  ├─ ui_event_bus.h
│  ├─ ui_event_queue.c           # 事件队列
│  └─ ui_event_queue.h
├─ datastore/                    # UI数据中心（重点模块）
│  ├─ ui_model.c                 # 全局数据模型
│  ├─ ui_model.h
│  ├─ ui_binding.c               # 数据绑定
│  ├─ ui_binding.h
│  ├─ ui_snapshot.c              # 数据快照
│  └─ ui_snapshot.h
├─ animation/                    # 基础动画引擎
│  ├─ ui_anim.c
│  ├─ ui_anim.h
│  ├─ ui_anim_scheduler.c        # 动画调度器
│  ├─ ui_anim_scheduler.h
│  ├─ ui_anim_curves.c           # 缓动曲线
│  └─ ui_anim_curves.h
├─ effects/                      # Metro风格高级特效
│  ├─ effect_tile_flip.c         # 磁贴翻转
│  ├─ effect_tile_flip.h
│  ├─ effect_turnstile.c         # WP页面进场
│  ├─ effect_turnstile.h
│  ├─ effect_tilt.c              # 点击倾斜
│  ├─ effect_tilt.h
│  ├─ effect_weather.c           # 天气动画控制器
│  ├─ effect_rain.c              # 雨滴
│  ├─ effect_snow.c              # 雪花
│  ├─ effect_cloud.c             # 云层漂浮
│  ├─ effect_marquee.c           # 跑马灯
│  └─ effect_marquee.h
├─ layer/                        # 图层系统
│  ├─ ui_layer_wallpaper.c       # 背景层
│  ├─ ui_layer_wallpaper.h
│  ├─ ui_overlay.c               # 顶层覆盖层
│  ├─ ui_overlay.h
│  ├─ ui_parallax.c              # 视差滚动
│  └─ ui_parallax.h
├─ popup/                        # 弹窗系统
│  ├─ ui_popup_pool.c            # 静态对象池
│  ├─ ui_popup_pool.h
│  ├─ ui_popup.c                 # 弹窗管理
│  ├─ ui_popup.h
│  ├─ ui_dialog.c                # 对话框
│  ├─ ui_dialog.h
│  ├─ ui_toast.c                 # Toast提示
│  └─ ui_toast.h
├─ widget/                       # Metro控件库
│  ├─ base/
│  │  ├─ ui_widget.c             # 控件基类
│  │  └─ ui_widget.h
│  ├─ basic/
│  │  ├─ ui_button.c
│  │  ├─ ui_button.h
│  │  ├─ ui_label.c
│  │  ├─ ui_label.h
│  │  ├─ ui_image.c
│  │  ├─ ui_image.h
│  │  ├─ ui_progress.c
│  │  └─ ui_progress.h
│  └─ advanced/
│     ├─ ui_tile.c               # Metro磁贴
│     ├─ ui_tile.h
│     ├─ ui_weather_tile.c       # 天气磁贴
│     ├─ ui_weather_tile.h
│     ├─ ui_music_tile.c         # 音乐磁贴
│     ├─ ui_music_tile.h
│     ├─ ui_clock_tile.c         # 时钟磁贴
│     ├─ ui_clock_tile.h
│     ├─ ui_album_tile.c         # 专辑封面磁贴
│     ├─ ui_album_tile.h
│     ├─ ui_list_virtual.c       # 虚拟列表
│     └─ ui_list_virtual.h
├─ navigation/                   # 页面导航系统
│  ├─ ui_page_manager.c          # 页面生命周期
│  ├─ ui_page_manager.h
│  ├─ ui_page_registry.c         # 页面注册
│  ├─ ui_page_registry.h
│  ├─ ui_nav_stack.c             # 导航栈
│  ├─ ui_nav_stack.h
│  ├─ ui_transition.c            # 页面切换动画
│  └─ ui_transition.h
├─ hal/                          # LVGL硬件抽象层
│  ├─ ui_hal.h
│  └─ backend_lvgl/
│      ├─ stm32/
│      │  ├─ ui_lv_port_disp.c
│      │  ├─ ui_lv_port_indev.c
│      │  ├─ ui_lv_port_fs.c
│      │  └─ ui_lv_port_dma2d.c
│      └─ esp32/
│          ├─ ui_lv_port_disp.c
│          ├─ ui_lv_port_indev.c
│          ├─ ui_lv_port_fs.c
│          └─ ui_lv_port_psram.c
├─ app/                          # 业务页面层
│  ├─ page_home/
│  │  ├─ page_home.c
│  │  └─ page_home.h
│  ├─ page_music/
│  │  ├─ page_music.c
│  │  └─ page_music.h
│  ├─ page_playlist/
│  │  ├─ page_playlist.c
│  │  └─ page_playlist.h
│  ├─ page_file_browser/
│  │  ├─ page_file_browser.c
│  │  └─ page_file_browser.h
│  ├─ page_weather/
│  │  ├─ page_weather.c
│  │  └─ page_weather.h
│  ├─ page_setting/
│  │  ├─ page_setting.c
│  │  └─ page_setting.h
│  └─ page_about/
│      ├─ page_about.c
│      └─ page_about.h
└─ resource/                     # 静态资源
   ├─ font/                      # 字体
   ├─ image/                     # 图片
   ├─ icon/                      # 图标
   ├─ theme/                     # Metro主题
   └─ resource_export.h


架构分层规则（AI 编码约束）
依赖流向：app 业务层 → widget/navigation/effects → core/event/datastore/animation/layer/popup → config → hal，禁止底层反向依赖业务页面。
硬件适配：屏幕、输入、文件系统、PSRAM/DMA2D 相关移植代码统一放置 hal/backend_lvgl，业务层不直接调用 LVGL 底层接口。
数据规范：全局状态统一存放 datastore/ui_model，页面禁止定义大量全局变量，数据变更通过 ui_binding 自动刷新控件。
事件规范：跨页面、跨模块通讯全部使用 event 事件总线，禁止模块间直接函数耦合调用。
控件开发：自定义控件继承 widget/base 基类，基础控件放入 basic，磁贴 / 虚拟列表等复杂组件放入 advanced。
页面规范：所有业务页面统一放在 app/page_xxx，页面生命周期交由 navigation/page_manager 统一管理。
资源管理：图片、字体、主题资源全部归入 resource，资源导出宏统一在 resource_export.h。
代码编写约束
全部采用 C 标准编写，头文件加头文件保护宏。
结构体、枚举、接口统一加 ui_ 前缀区分 UI 模块。
新增动画、特效优先复用 animation 调度器，不单独创建 LVGL 原生动画。
弹窗使用 popup_pool 对象池，禁止动态频繁创建销毁控件。
页面切换动画复用 transition 模块，自定义转场放入 effects。
尺寸、帧率、缓存参数全部读取 ui_config.h，禁止代码内硬编码数值。

禁止修改区域
hal/backend_lvgl 下平台移植底层驱动，无硬件需求不改动。
config 内全局宏配置，修改参数需同步注释说明影响范围。
widget/base 控件基类核心逻辑，不可随意修改继承结构。
