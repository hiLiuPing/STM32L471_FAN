# Plain_UI

`Plain_UI` 是当前工程唯一正式 UI 目录。它把页面、事件、数据、动画、特效、图层、弹窗、导航和 LVGL 后端全部收口到同一套分层架构中，目标是让后续移植、扩展和替换 backend 都尽量不改业务页。

## 运行逻辑

启动顺序固定为 `ui_main -> ui_tick -> ui_update -> ui_event_bridge`。

`ui_main` 负责初始化事件总线、数据层、脏区、渲染器、动画、特效和图层管理器，并启动 UI 任务。

`ui_tick` 只把系统节拍转给 HAL，保证 LVGL 与平台时基一致。

`ui_event_bridge` 负责从输入队列和消息队列取数据，再统一分发给页面管理器。

`ui_update` 负责每帧同步模型、更新绑定、处理页面生命周期、执行动画和特效，最后统一绘制。

## 数据与绘制流

1. 平台输入进入 `event/`。
2. 业务数据进入 `datastore/`，通过脏标记触发局部刷新。
3. 页面由 `navigation/` 统一注册和切换。
4. 绘制由 `layer/` 分层调度，页面内容在底层层绘制，弹窗和 Toast 走顶层层。
5. `hal/backend_lvgl/` 负责把业务绘制结果落到 LVGL 与 LCD。

## 移植步骤

1. 先接好 `hal/`，保证屏幕、输入、时基和 flush 可用。
2. 再确认 `config/` 的尺寸、队列、绑定和帧率参数。
3. 然后移植 `datastore/`、`event/`、`navigation/` 和 `widget/`。
4. 最后接入 `app/` 的 5 个页面。
5. 如果要替换 backend，只改 `hal/backend_*`，上层不直接依赖 BSP 或 `lvgl.h`。

## 目录说明

各子目录的职责说明见对应目录下的 `README.md`。
