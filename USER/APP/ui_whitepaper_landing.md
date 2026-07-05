# LVGL 落地与白皮书补充说明

本文档对应当前 `Keil + STM32 + FreeRTOS` 工程中的 UI 实现状态，目标是把白皮书中的通用 UI Framework 设计，落实到可编译、可显示、可继续扩展的 MCU 工程上。

## 1. 当前落地范围

- Renderer Adapter 已切换到 LVGL 8.3.11 后端，底层通过 `lv_canvas` + stripe buffer 做离线绘制。
- 页面层仍保持现有 immediate-mode API，不直接暴露 `lv_obj_t`、LCD BSP 或具体驱动细节。
- 刷屏仍使用 `428 x 142` 实屏尺寸和 `16` 行 stripe，避免常驻全屏 framebuffer。
- UI Task、MessageBus、PropertyStore、PropertyBinding、LayerManager、ModalManager、FXEmitter、Marquee、VirtualList 已接入同一套调度链路。
- 示例工程保留 5 个真实页面：`Home / Controls / List / Data / Settings`。

## 2. 分层边界

页面层允许依赖：

- `ui_renderer_adapter.h`
- `ui_property_store.h`
- `ui_message_bus.h`
- `ui_page_manager.h`
- `ui_types.h`
- `ui_event.h`

页面层不再依赖：

- EmbeddedGUI 头文件
- `egui_*` 运行时接口
- LCD / SPI / OLED BSP 头文件
- 业务驱动和硬件采样驱动
- 直接操作 `lv_obj_t` 的页面代码

这样可以保证后续即便继续演进到底层原生 LVGL 控件树，页面生命周期、消息总线、属性同步和导航框架也不需要整体重写。

## 3. Renderer Adapter 与 LVGL 的关系

当前工程仍对上层暴露统一的 `UI_RendererAdapter_*` 接口，但内部已经完成以下映射：

| 现有接口 | 当前后端实现 |
| --- | --- |
| `DrawText` / `DrawTextInRect` | `lv_canvas_draw_text` |
| `DrawFillRect` / `DrawPanel` | `lv_canvas_draw_rect` |
| `DrawRect` | `lv_canvas_draw_rect` 边框模式 |
| `DrawLine` / `DrawSparkline` | `lv_canvas_draw_line` |
| `DrawButton` / `DrawBadge` / `DrawIconBox` | 组合绘制 |
| `DrawProgressBar` / `DrawSlider` / `DrawToggle` / `DrawCheckbox` | 组合绘制 |
| `EndFrame` | dirty region 驱动的 LVGL canvas stripe 回放 + LCD flush |

这意味着：

- 页面 API 已经与旧 EmbeddedGUI 解耦。
- 当前项目已经不是“LVGL ready”，而是“LVGL backend running”。
- 后续如果要逐步把某些页面改写为原生 `lv_btn`、`lv_bar`、`lv_list`、`lv_chart`，可以在 Adapter 或新 Widget 层逐步接入。

## 4. 5 个页面覆盖点

- `Home`：展示 UI Task、MessageBus、PropertyStore、Renderer Adapter、LCD flush 的关系。
- `Controls`：按钮、开关、复选框、进度条、滑块等常用控件外观。
- `List`：虚拟列表、Provider、选中态、Badge。
- `Data`：PropertyStore 驱动的数据卡片、进度条、折线趋势。
- `Settings`：移植检查项、静态内存、stripe 刷屏、LVGL 后端状态。

## 5. 白皮书能力对应状态

已落地：

- 单 UI Task 串行绘制模型
- InputEventQueue
- MessageBus
- PropertyStore 前后台同步
- PropertyBinding 到局部失效区域
- Base / Climate / FX Particle / Top Window 四层绘制顺序
- ModalManager 顶层弹窗与输入拦截
- FXEmitter 粒子池与行为回调
- Marquee 长文本滚动状态机
- VirtualList Provider 与可见行模型
- Dirty Region 驱动的局部 stripe 刷新

仍可继续增强：

- 真实触摸/编码器输入驱动直接对接 `UI_InputEventQueue_Publish`
- 更完整的字体枚举与多字号映射
- 图片资源、图标资源、位图/图像接口
- 更完整的主题系统和样式表
- 某些控件逐步改为原生 LVGL widget 实现

## 6. 当前实现策略说明

当前没有让页面直接持有 LVGL 控件树，而是选择：

- 保留现有页面 API
- 在 Adapter 内部切换到 LVGL
- 使用 canvas 对命令缓冲进行逐条回放
- 使用 dirty region 控制实际 LCD flush 范围

这个策略更适合当前 MCU 资源条件，也能降低你现有代码的大面积重构风险。

## 7. 下一步建议

如果要继续“把 API 接口都换成 LVGL”的下一阶段，可以按下面顺序推进：

1. 在 `ui_widget` 层增加更贴近 LVGL 的通用组件描述结构。
2. 为按钮、进度条、滑块、开关、复选框、列表提供原生 LVGL widget 版本。
3. 逐步把页面从纯 immediate-mode 迁到“描述层 + Adapter/LVGL 实例层”。
4. 在输入侧补充真实触摸、焦点和手势映射。
5. 完成图片、主题、动画和资源系统的进一步统一。
