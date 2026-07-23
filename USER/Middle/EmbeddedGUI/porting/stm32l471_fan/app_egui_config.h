#ifndef __APP_EGUI_CONFIG_H__
#define __APP_EGUI_CONFIG_H__

/* STM32L471_FAN display: ST77903-style 428x142 RGB565 panel. */
#define EGUI_CONFIG_SCREEN_WIDTH  428
#define EGUI_CONFIG_SCREEN_HEIGHT 142
#define EGUI_CONFIG_COLOR_DEPTH   16

/* RGB565 byte order is now handled in hardware: the LCD driver sends pixel
 * payloads as 16-bit SPI frames (MSB first), so no CPU byte swap is needed.
 * See LCD_SPI_SetFrame16() in USER/BSP. */
#define EGUI_CONFIG_COLOR_16_SWAP 0

/* Two PFB buffers let SPI DMA flush one tile while the next tile renders. */
#define EGUI_CONFIG_PFB_WIDTH        EGUI_CONFIG_SCREEN_WIDTH
#define EGUI_CONFIG_PFB_HEIGHT       16
#define EGUI_CONFIG_PFB_BUFFER_COUNT 2

#define EGUI_CONFIG_MAX_FPS          30
#define EGUI_CONFIG_DIRTY_AREA_COUNT 20

/* Enable 8-bit alpha channel pixel accessor for egui_image_std */
#define EGUI_CONFIG_FUNCTION_IMAGE_FORMAT_RGB565_8 1

/* Dynamic-theme cloud frames live in PSRAM and are loaded through bounded
 * SRAM row caches. QSPI and the LCD SPI bus are independent peripherals. */
#define EGUI_CONFIG_FUNCTION_EXTERNAL_RESOURCE 1
#define EGUI_CONFIG_EXTERNAL_RESOURCE_SHARED_DISPLAY_BUS 0
#define EGUI_CONFIG_IMAGE_EXTERNAL_DATA_CACHE_MAX_BYTES  (229U * 2U * 2U)
#define EGUI_CONFIG_IMAGE_EXTERNAL_ALPHA_CACHE_MAX_BYTES (229U * 2U)

/* No touch in this migration round. Physical keys remain enabled. */
#define EGUI_CONFIG_FUNCTION_SUPPORT_TOUCH 0
#define EGUI_CONFIG_FUNCTION_SUPPORT_KEY   1
#define EGUI_CONFIG_FUNCTION_SUPPORT_FOCUS 0

/* Keep the first port small and deterministic. */
#define EGUI_CONFIG_FUNCTION_SUPPORT_ACTIVITY 0
#define EGUI_CONFIG_FUNCTION_SUPPORT_DIALOG   0
#define EGUI_CONFIG_FUNCTION_SUPPORT_TOAST    0
#define EGUI_CONFIG_FUNCTION_SUPPORT_SHADOW   0
#define EGUI_CONFIG_FUNCTION_SUPPORT_MASK     1
#define EGUI_CONFIG_FUNCTION_SUPPORT_SCROLLBAR 0
#define EGUI_CONFIG_FUNCTION_IMAGE_RUNTIME_SVG 0
#define EGUI_CONFIG_FUNCTION_IMAGE_CODEC_QOI   1
/* Frame-local row cache follows the 16-row PFB and avoids cross-tile re-decode. */
#define EGUI_CONFIG_FUNCTION_IMAGE_CODEC_FAST_DRAW 1
/* ARM C heap is only 4KB; keep full-image persistent decode caching disabled. */
#define EGUI_CONFIG_IMAGE_CODEC_PERSISTENT_CACHE_MAX_BYTES 0
#define EGUI_CONFIG_FUNCTION_FONT_TTF          0
#define EGUI_CONFIG_FUNCTION_FONT_STD_FAST_DRAW 0
#define EGUI_CONFIG_FUNCTION_FONT_TRANSFORM_FAST_DRAW 0

#define EGUI_CONFIG_FUNCTION_LABEL_TEXT_FMT  1
#define EGUI_CONFIG_FUNCTION_LABEL_LONG_MODE 1
#define EGUI_CONFIG_FUNCTION_LABEL_WORD_WRAP 1
#define EGUI_CONFIG_LABEL_FMT_BUF_SIZE       96

#define EGUI_CONFIG_DEBUG_LOG_LEVEL 0

/* Debug monitors: FPS/render time (bottom-right) and memory (bottom-left) */
#define EGUI_CONFIG_DEBUG_PERF_MONITOR_SHOW 0
#define EGUI_CONFIG_DEBUG_MEM_MONITOR_SHOW  0

#endif /* __APP_EGUI_CONFIG_H__ */
