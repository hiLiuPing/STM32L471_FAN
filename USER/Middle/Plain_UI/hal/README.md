# hal

硬件抽象层。

这里统一屏幕、输入、文件系统、DMA2D 和 LVGL port。

上层业务只依赖 `ui_hal` 和 `backend_lvgl` 的抽象，不直接碰 BSP。
