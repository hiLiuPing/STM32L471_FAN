/*
 * 渲染适配器。
 * 它把 Plain_UI 的绘制原语翻译为 LVGL 可落地的绘制命令。
 * 页面和控件只能依赖这个抽象，不应直接调用 LVGL 绘制 API。
 */
#ifndef UI_RENDERER_ADAPTER_H
#define UI_RENDERER_ADAPTER_H

#include <stdint.h>

#include "../../core/ui_types.h"

typedef uint16_t ui_color_t;

typedef enum
{
  UI_FONT_SIZE_12 = 12
} ui_font_size_t;

typedef enum
{
  UI_TEXT_ALIGN_LEFT = 0,
  UI_TEXT_ALIGN_CENTER,
  UI_TEXT_ALIGN_RIGHT
} ui_text_align_t;

typedef struct
{
  ui_color_t color;
  ui_font_size_t font_size;
  ui_text_align_t align;
} ui_text_style_t;

typedef struct
{
  ui_color_t text;
  ui_color_t muted;
  ui_color_t background;
  ui_color_t border;
  ui_color_t accent;
  uint8_t radius;
  uint8_t border_width;
} ui_render_style_t;

void UI_RendererAdapter_Init(void);
void UI_RendererAdapter_BeginFrame(void);
void UI_RendererAdapter_EndFrame(void);
int16_t UI_RendererAdapter_GetWidth(void);
int16_t UI_RendererAdapter_GetHeight(void);
uint16_t UI_RendererAdapter_RGB565(uint8_t red, uint8_t green, uint8_t blue);
ui_color_t UI_RendererAdapter_Color(uint8_t red, uint8_t green, uint8_t blue);
void UI_RendererAdapter_DrawLabel(const ui_viewport_t *viewport, int16_t rx, int16_t ry, const char *text);
void UI_RendererAdapter_DrawText(const ui_viewport_t *viewport,
                                 int16_t rx,
                                 int16_t ry,
                                 const char *text,
                                 uint16_t color);
void UI_RendererAdapter_DrawFillRect(const ui_viewport_t *viewport,
                                     int16_t rx,
                                     int16_t ry,
                                     int16_t width,
                                     int16_t height,
                                     uint16_t color);
void UI_RendererAdapter_DrawRect(const ui_viewport_t *viewport,
                                 int16_t rx,
                                 int16_t ry,
                                 int16_t width,
                                 int16_t height,
                                 uint16_t color);
void UI_RendererAdapter_DrawLine(const ui_viewport_t *viewport,
                                 int16_t rx1,
                                 int16_t ry1,
                                 int16_t rx2,
                                 int16_t ry2,
                                 uint16_t color);
void UI_RendererAdapter_DrawTextInRect(const ui_viewport_t *viewport,
                                       const ui_rect_t *rect,
                                       const char *text,
                                       const ui_text_style_t *style);
void UI_RendererAdapter_DrawPanel(const ui_viewport_t *viewport,
                                  const ui_rect_t *rect,
                                  const ui_render_style_t *style);
void UI_RendererAdapter_DrawButton(const ui_viewport_t *viewport,
                                   const ui_rect_t *rect,
                                   const char *text,
                                   const ui_render_style_t *style,
                                   uint8_t pressed);
void UI_RendererAdapter_DrawProgressBar(const ui_viewport_t *viewport,
                                        const ui_rect_t *rect,
                                        uint8_t percent,
                                        const ui_render_style_t *style);
void UI_RendererAdapter_DrawSlider(const ui_viewport_t *viewport,
                                   const ui_rect_t *rect,
                                   int32_t value,
                                   int32_t min_value,
                                   int32_t max_value,
                                   const ui_render_style_t *style);
void UI_RendererAdapter_DrawToggle(const ui_viewport_t *viewport,
                                   const ui_rect_t *rect,
                                   uint8_t is_on,
                                   const ui_render_style_t *style);
void UI_RendererAdapter_DrawCheckbox(const ui_viewport_t *viewport,
                                     const ui_rect_t *rect,
                                     uint8_t is_checked,
                                     const char *text,
                                     const ui_render_style_t *style);
void UI_RendererAdapter_DrawListItem(const ui_viewport_t *viewport,
                                     const ui_rect_t *rect,
                                     const char *title,
                                     const char *subtitle,
                                     uint8_t selected,
                                     const ui_render_style_t *style);
void UI_RendererAdapter_DrawBadge(const ui_viewport_t *viewport,
                                  const ui_rect_t *rect,
                                  const char *text,
                                  const ui_render_style_t *style);
void UI_RendererAdapter_DrawSparkline(const ui_viewport_t *viewport,
                                      const ui_rect_t *rect,
                                      const int16_t *values,
                                      uint8_t count,
                                      int16_t min_value,
                                      int16_t max_value,
                                      const ui_render_style_t *style);
void UI_RendererAdapter_DrawIconBox(const ui_viewport_t *viewport,
                                    const ui_rect_t *rect,
                                    const char *icon_text,
                                    const char *label,
                                    const ui_render_style_t *style);

#endif /* UI_RENDERER_ADAPTER_H */
