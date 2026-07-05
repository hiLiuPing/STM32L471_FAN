# hal/backend_lvgl/stm32

STM32 平台专用 LVGL port。

这里放屏幕 flush、输入采样、FS 适配和 DMA2D 等 STM32 相关实现。

这层允许依赖 BSP，但上层不允许反向依赖这里。
