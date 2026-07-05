#include "ui_renderer_adapter.h"

#include "../../config/ui_config.h"
#include "../../core/ui_dirty_region.h"
#include "../ui_hal.h"

#include "lvgl.h"

#include <string.h>

#define UI_RENDERER_COMMAND_CAPACITY 128U
#define UI_RENDERER_TEXT_LENGTH      64U
#define UI_RENDERER_STRIPE_HEIGHT    16U
#define UI_RENDERER_FONT_WIDTH       7
#define UI_RENDERER_FONT_HEIGHT      12

typedef enum
{
  UI_RENDER_CMD_TEXT = 0,
  UI_RENDER_CMD_LINE,
  UI_RENDER_CMD_FILL_RECT,
  UI_RENDER_CMD_RECT
} ui_render_cmd_type_t;

typedef struct
{
  ui_render_cmd_type_t type;
  int16_t x1;
  int16_t y1;
  int16_t x2;
  int16_t y2;
  int16_t width;
  int16_t height;
  uint16_t color;
  char text[UI_RENDERER_TEXT_LENGTH];
} ui_render_cmd_t;

static ui_render_cmd_t g_render_commands[UI_RENDERER_COMMAND_CAPACITY];
static uint16_t g_render_command_count;
static lv_color_t g_render_stripe[LCD_W * UI_RENDERER_STRIPE_HEIGHT];
static lv_color_t g_lvgl_disp_buffer[LCD_W * UI_RENDERER_STRIPE_HEIGHT];
static lv_disp_draw_buf_t g_lvgl_draw_buf;
static lv_disp_drv_t g_lvgl_disp_drv;
static lv_disp_t *g_lvgl_display;
static lv_obj_t *g_lvgl_canvas;
static uint8_t g_lvgl_ready;

static void UI_RendererAdapter_LVGLFlush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
  int32_t width;
  int32_t height;

  if ((disp_drv == NULL) || (area == NULL) || (color_p == NULL))
  {
    if (disp_drv != NULL)
    {
      lv_disp_flush_ready(disp_drv);
    }
    return;
  }

  if ((area->x2 < 0) || (area->y2 < 0) || (area->x1 >= LCD_W) || (area->y1 >= LCD_H))
  {
    lv_disp_flush_ready(disp_drv);
    return;
  }

  width = area->x2 - area->x1 + 1;
  height = area->y2 - area->y1 + 1;
  if ((width <= 0) || (height <= 0))
  {
    lv_disp_flush_ready(disp_drv);
    return;
  }

  /* flush 只做最终落屏，不承担业务绘制逻辑。 */
  UI_Hal_FlushRGB565((uint16_t)area->x1,
                     (uint16_t)area->y1,
                     (uint16_t)width,
                     (uint16_t)height,
                     (const uint16_t *)color_p);
  lv_disp_flush_ready(disp_drv);
}

static int UI_RendererAdapter_StripeIntersectsDirty(uint16_t y,
                                                    uint16_t height,
                                                    const ui_rect_t *dirty_rects,
                                                    size_t dirty_count)
{
  size_t i;
  int16_t stripe_top = (int16_t)y;
  int16_t stripe_bottom = (int16_t)(y + height);

  for (i = 0U; i < dirty_count; ++i)
  {
    int16_t dirty_top = dirty_rects[i].y;
    int16_t dirty_bottom = (int16_t)(dirty_rects[i].y + dirty_rects[i].height);

    if ((dirty_rects[i].height > 0) && (stripe_bottom > dirty_top) && (stripe_top < dirty_bottom))
    {
      return 1;
    }
  }

  return 0;
}

static uint16_t UI_RendererAdapter_BackgroundColor(void)
{
  return UI_RendererAdapter_RGB565(8U, 18U, 30U);
}

static lv_color_t UI_RendererAdapter_ToLvColor(uint16_t color)
{
  uint8_t red5 = (uint8_t)((color >> 11) & 0x1FU);
  uint8_t green6 = (uint8_t)((color >> 5) & 0x3FU);
  uint8_t blue5 = (uint8_t)(color & 0x1FU);
  uint8_t red8 = (uint8_t)((red5 << 3) | (red5 >> 2));
  uint8_t green8 = (uint8_t)((green6 << 2) | (green6 >> 4));
  uint8_t blue8 = (uint8_t)((blue5 << 3) | (blue5 >> 2));

  return lv_color_make(red8, green8, blue8);
}

static int16_t UI_RendererAdapter_MaxInt16(int16_t a, int16_t b)
{
  return (a > b) ? a : b;
}

static int16_t UI_RendererAdapter_MinInt16(int16_t a, int16_t b)
{
  return (a < b) ? a : b;
}

static uint8_t UI_RendererAdapter_ClampPercent(uint8_t percent)
{
  return (percent > 100U) ? 100U : percent;
}

static int16_t UI_RendererAdapter_EstimateTextWidth(const char *text)
{
  uint16_t len;

  if (text == NULL)
  {
    return 0;
  }

  len = (uint16_t)strlen(text);
  if (len > 120U)
  {
    len = 120U;
  }

  return (int16_t)(len * UI_RENDERER_FONT_WIDTH);
}

static int16_t UI_RendererAdapter_ClampInt16(int32_t value, int16_t min_value, int16_t max_value)
{
  if (value < min_value)
  {
    return min_value;
  }

  if (value > max_value)
  {
    return max_value;
  }

  return (int16_t)value;
}

static int UI_RendererAdapter_ClipRect(const ui_viewport_t *viewport,
                                       int16_t *x,
                                       int16_t *y,
                                       int16_t *width,
                                       int16_t *height)
{
  int16_t x1;
  int16_t y1;
  int16_t x2;
  int16_t y2;
  int16_t clip_x1;
  int16_t clip_y1;
  int16_t clip_x2;
  int16_t clip_y2;

  if ((viewport == NULL) || (x == NULL) || (y == NULL) || (width == NULL) || (height == NULL))
  {
    return 0;
  }

  if ((*width <= 0) || (*height <= 0) || (viewport->width <= 0) || (viewport->height <= 0))
  {
    return 0;
  }

  x1 = *x;
  y1 = *y;
  x2 = x1 + *width;
  y2 = y1 + *height;

  clip_x1 = UI_RendererAdapter_MaxInt16(viewport->x, 0);
  clip_y1 = UI_RendererAdapter_MaxInt16(viewport->y, 0);
  clip_x2 = UI_RendererAdapter_MinInt16((int16_t)(viewport->x + viewport->width), LCD_W);
  clip_y2 = UI_RendererAdapter_MinInt16((int16_t)(viewport->y + viewport->height), LCD_H);

  x1 = UI_RendererAdapter_MaxInt16(x1, clip_x1);
  y1 = UI_RendererAdapter_MaxInt16(y1, clip_y1);
  x2 = UI_RendererAdapter_MinInt16(x2, clip_x2);
  y2 = UI_RendererAdapter_MinInt16(y2, clip_y2);

  if ((x2 <= x1) || (y2 <= y1))
  {
    return 0;
  }

  *x = x1;
  *y = y1;
  *width = x2 - x1;
  *height = y2 - y1;
  return 1;
}

static uint8_t UI_RendererAdapter_LineOutCode(const ui_viewport_t *viewport, int16_t x, int16_t y)
{
  uint8_t code = 0U;
  int16_t left = viewport->x;
  int16_t top = viewport->y;
  int16_t right = viewport->x + viewport->width - 1;
  int16_t bottom = viewport->y + viewport->height - 1;

  if (x < left)
  {
    code |= 1U;
  }
  else if (x > right)
  {
    code |= 2U;
  }

  if (y < top)
  {
    code |= 4U;
  }
  else if (y > bottom)
  {
    code |= 8U;
  }

  return code;
}

static int UI_RendererAdapter_ClipLine(const ui_viewport_t *viewport,
                                       int16_t *x1,
                                       int16_t *y1,
                                       int16_t *x2,
                                       int16_t *y2)
{
  uint8_t out1;
  uint8_t out2;
  int32_t x;
  int32_t y;
  ui_viewport_t clipped;

  if ((viewport == NULL) || (x1 == NULL) || (y1 == NULL) || (x2 == NULL) || (y2 == NULL))
  {
    return 0;
  }

  if ((viewport->width <= 0) || (viewport->height <= 0))
  {
    return 0;
  }

  clipped.x = UI_RendererAdapter_MaxInt16(viewport->x, 0);
  clipped.y = UI_RendererAdapter_MaxInt16(viewport->y, 0);
  clipped.width = UI_RendererAdapter_MinInt16((int16_t)(viewport->x + viewport->width - clipped.x), (int16_t)(LCD_W - clipped.x));
  clipped.height = UI_RendererAdapter_MinInt16((int16_t)(viewport->y + viewport->height - clipped.y), (int16_t)(LCD_H - clipped.y));
  clipped.scroll_x = 0;
  clipped.scroll_y = 0;

  if ((clipped.width <= 0) || (clipped.height <= 0))
  {
    return 0;
  }

  for (;;)
  {
    int16_t left = clipped.x;
    int16_t top = clipped.y;
    int16_t right = clipped.x + clipped.width - 1;
    int16_t bottom = clipped.y + clipped.height - 1;

    out1 = UI_RendererAdapter_LineOutCode(&clipped, *x1, *y1);
    out2 = UI_RendererAdapter_LineOutCode(&clipped, *x2, *y2);

    if ((out1 | out2) == 0U)
    {
      return 1;
    }

    if ((out1 & out2) != 0U)
    {
      return 0;
    }

    if ((out1 != 0U) && ((out1 & 8U) != 0U))
    {
      x = *x1 + ((int32_t)(*x2 - *x1) * (bottom - *y1)) / (*y2 - *y1);
      y = bottom;
      *x1 = (int16_t)x;
      *y1 = (int16_t)y;
    }
    else if ((out1 != 0U) && ((out1 & 4U) != 0U))
    {
      x = *x1 + ((int32_t)(*x2 - *x1) * (top - *y1)) / (*y2 - *y1);
      y = top;
      *x1 = (int16_t)x;
      *y1 = (int16_t)y;
    }
    else if ((out1 != 0U) && ((out1 & 2U) != 0U))
    {
      y = *y1 + ((int32_t)(*y2 - *y1) * (right - *x1)) / (*x2 - *x1);
      x = right;
      *x1 = (int16_t)x;
      *y1 = (int16_t)y;
    }
    else if (out1 != 0U)
    {
      y = *y1 + ((int32_t)(*y2 - *y1) * (left - *x1)) / (*x2 - *x1);
      x = left;
      *x1 = (int16_t)x;
      *y1 = (int16_t)y;
    }
    else if ((out2 & 8U) != 0U)
    {
      x = *x1 + ((int32_t)(*x2 - *x1) * (bottom - *y1)) / (*y2 - *y1);
      y = bottom;
      *x2 = (int16_t)x;
      *y2 = (int16_t)y;
    }
    else if ((out2 & 4U) != 0U)
    {
      x = *x1 + ((int32_t)(*x2 - *x1) * (top - *y1)) / (*y2 - *y1);
      y = top;
      *x2 = (int16_t)x;
      *y2 = (int16_t)y;
    }
    else if ((out2 & 2U) != 0U)
    {
      y = *y1 + ((int32_t)(*y2 - *y1) * (right - *x1)) / (*x2 - *x1);
      x = right;
      *x2 = (int16_t)x;
      *y2 = (int16_t)y;
    }
    else
    {
      y = *y1 + ((int32_t)(*y2 - *y1) * (left - *x1)) / (*x2 - *x1);
      x = left;
      *x2 = (int16_t)x;
      *y2 = (int16_t)y;
    }
  }
}

static ui_render_cmd_t *UI_RendererAdapter_AllocCommand(ui_render_cmd_type_t type)
{
  ui_render_cmd_t *cmd;

  if (g_render_command_count >= UI_RENDERER_COMMAND_CAPACITY)
  {
    return NULL;
  }

  cmd = &g_render_commands[g_render_command_count++];
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = type;
  return cmd;
}

static void UI_RendererAdapter_FillStripe(lv_color_t color, uint16_t count)
{
  uint16_t i;

  for (i = 0U; i < count; ++i)
  {
    g_render_stripe[i] = color;
  }
}

static const lv_font_t *UI_RendererAdapter_GetFont(ui_font_size_t font_size)
{
  switch (font_size)
  {
  case UI_FONT_SIZE_12:
  default:
    return &lv_font_montserrat_12;
  }
}

static void UI_RendererAdapter_DrawCommand(lv_obj_t *canvas, const ui_render_cmd_t *cmd, int16_t stripe_y)
{
  lv_color_t color;
  int16_t local_x1;
  int16_t local_y1;
  int16_t local_x2;
  int16_t local_y2;

  if ((canvas == NULL) || (cmd == NULL))
  {
    return;
  }

  color = UI_RendererAdapter_ToLvColor(cmd->color);
  local_x1 = cmd->x1;
  local_y1 = (int16_t)(cmd->y1 - stripe_y);
  local_x2 = cmd->x2;
  local_y2 = (int16_t)(cmd->y2 - stripe_y);

  switch (cmd->type)
  {
  case UI_RENDER_CMD_TEXT:
  {
    lv_draw_label_dsc_t text_dsc;

    lv_draw_label_dsc_init(&text_dsc);
    text_dsc.color = color;
    text_dsc.opa = LV_OPA_COVER;
    text_dsc.font = UI_RendererAdapter_GetFont(UI_FONT_SIZE_12);
    text_dsc.align = LV_TEXT_ALIGN_LEFT;
    lv_canvas_draw_text(canvas, local_x1, local_y1, LCD_W, &text_dsc, cmd->text);
    break;
  }

  case UI_RENDER_CMD_LINE:
  {
    lv_draw_line_dsc_t line_dsc;
    lv_point_t points[2];

    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = color;
    line_dsc.width = 1;
    line_dsc.opa = LV_OPA_COVER;
    points[0].x = local_x1;
    points[0].y = local_y1;
    points[1].x = local_x2;
    points[1].y = local_y2;
    lv_canvas_draw_line(canvas, points, 2U, &line_dsc);
    break;
  }

  case UI_RENDER_CMD_FILL_RECT:
  {
    lv_draw_rect_dsc_t rect_dsc;

    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.radius = 0;
    rect_dsc.bg_color = color;
    rect_dsc.bg_opa = LV_OPA_COVER;
    rect_dsc.border_opa = LV_OPA_TRANSP;
    lv_canvas_draw_rect(canvas, local_x1, local_y1, cmd->width, cmd->height, &rect_dsc);
    break;
  }

  case UI_RENDER_CMD_RECT:
  {
    lv_draw_rect_dsc_t rect_dsc;

    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.radius = 0;
    rect_dsc.bg_opa = LV_OPA_TRANSP;
    rect_dsc.border_width = 1;
    rect_dsc.border_color = color;
    rect_dsc.border_opa = LV_OPA_COVER;
    rect_dsc.border_side = LV_BORDER_SIDE_FULL;
    lv_canvas_draw_rect(canvas, local_x1, local_y1, cmd->width, cmd->height, &rect_dsc);
    break;
  }

  default:
    break;
  }
}

void UI_RendererAdapter_Init(void)
{
  if (g_lvgl_ready != 0U)
  {
    return;
  }

  UI_Hal_Init();
  lv_init();

  lv_disp_draw_buf_init(&g_lvgl_draw_buf,
                        g_lvgl_disp_buffer,
                        NULL,
                        (uint32_t)(LCD_W * UI_RENDERER_STRIPE_HEIGHT));

  lv_disp_drv_init(&g_lvgl_disp_drv);
  g_lvgl_disp_drv.hor_res = UI_Hal_GetWidth();
  g_lvgl_disp_drv.ver_res = UI_Hal_GetHeight();
  g_lvgl_disp_drv.flush_cb = UI_RendererAdapter_LVGLFlush;
  g_lvgl_disp_drv.draw_buf = &g_lvgl_draw_buf;
  g_lvgl_display = lv_disp_drv_register(&g_lvgl_disp_drv);

  if (g_lvgl_display != NULL)
  {
    g_lvgl_canvas = lv_canvas_create(lv_scr_act());
    lv_obj_add_flag(g_lvgl_canvas, LV_OBJ_FLAG_HIDDEN);
    lv_canvas_set_buffer(g_lvgl_canvas,
                         g_render_stripe,
                         LCD_W,
                         UI_RENDERER_STRIPE_HEIGHT,
                         LV_IMG_CF_TRUE_COLOR);
  }

  g_render_command_count = 0U;
  g_lvgl_ready = 1U;
}

void UI_RendererAdapter_BeginFrame(void)
{
  g_render_command_count = 0U;
}

void UI_RendererAdapter_EndFrame(void)
{
  uint16_t y;
  uint16_t command_index;
  ui_rect_t dirty_rects[UI_DIRTY_REGION_CAPACITY];
  size_t dirty_count;
  lv_color_t background;

  dirty_count = UI_DirtyRegion_Take(dirty_rects, UI_DIRTY_REGION_CAPACITY);

  if ((dirty_count == 0U) || (g_lvgl_canvas == NULL))
  {
    return;
  }

  /* 采用 stripe 方式合成：只处理脏条带，降低整屏重绘成本。 */
  background = UI_RendererAdapter_ToLvColor(UI_RendererAdapter_BackgroundColor());

  for (y = 0U; y < LCD_H; y = (uint16_t)(y + UI_RENDERER_STRIPE_HEIGHT))
  {
    uint16_t stripe_height = ((y + UI_RENDERER_STRIPE_HEIGHT) <= LCD_H) ? UI_RENDERER_STRIPE_HEIGHT : (uint16_t)(LCD_H - y);

    if (UI_RendererAdapter_StripeIntersectsDirty(y, stripe_height, dirty_rects, dirty_count) == 0)
    {
      continue;
    }

    UI_RendererAdapter_FillStripe(background, (uint16_t)(LCD_W * UI_RENDERER_STRIPE_HEIGHT));
    lv_canvas_fill_bg(g_lvgl_canvas, background, LV_OPA_COVER);

    for (command_index = 0U; command_index < g_render_command_count; ++command_index)
    {
      UI_RendererAdapter_DrawCommand(g_lvgl_canvas, &g_render_commands[command_index], (int16_t)y);
    }

    UI_Hal_FlushRGB565(0U, y, LCD_W, stripe_height, (const uint16_t *)g_render_stripe);
  }
}

int16_t UI_RendererAdapter_GetWidth(void)
{
  return UI_Hal_GetWidth();
}

int16_t UI_RendererAdapter_GetHeight(void)
{
  return UI_Hal_GetHeight();
}

uint16_t UI_RendererAdapter_RGB565(uint8_t red, uint8_t green, uint8_t blue)
{
  return (uint16_t)((((uint16_t)red & 0xF8U) << 8) |
                    (((uint16_t)green & 0xFCU) << 3) |
                    ((uint16_t)blue >> 3));
}

ui_color_t UI_RendererAdapter_Color(uint8_t red, uint8_t green, uint8_t blue)
{
  return UI_RendererAdapter_RGB565(red, green, blue);
}

void UI_RendererAdapter_DrawLabel(const ui_viewport_t *viewport, int16_t rx, int16_t ry, const char *text)
{
  UI_RendererAdapter_DrawText(viewport, rx, ry, text, UI_RendererAdapter_RGB565(232U, 238U, 245U));
}

void UI_RendererAdapter_DrawText(const ui_viewport_t *viewport,
                                 int16_t rx,
                                 int16_t ry,
                                 const char *text,
                                 uint16_t color)
{
  ui_render_cmd_t *cmd;
  int16_t x;
  int16_t y;

  if ((viewport == NULL) || (text == NULL))
  {
    return;
  }

  x = viewport->x + rx - viewport->scroll_x;
  y = viewport->y + ry - viewport->scroll_y;

  if ((x < viewport->x) || (y < viewport->y) || (x >= (viewport->x + viewport->width)) || (y >= (viewport->y + viewport->height)))
  {
    return;
  }

  cmd = UI_RendererAdapter_AllocCommand(UI_RENDER_CMD_TEXT);
  if (cmd == NULL)
  {
    return;
  }

  cmd->x1 = x;
  cmd->y1 = y;
  cmd->color = color;
  strncpy(cmd->text, text, sizeof(cmd->text) - 1U);
  cmd->text[sizeof(cmd->text) - 1U] = '\0';
}

void UI_RendererAdapter_DrawFillRect(const ui_viewport_t *viewport,
                                     int16_t rx,
                                     int16_t ry,
                                     int16_t width,
                                     int16_t height,
                                     uint16_t color)
{
  ui_render_cmd_t *cmd;
  int16_t x;
  int16_t y;

  if (viewport == NULL)
  {
    return;
  }

  x = viewport->x + rx - viewport->scroll_x;
  y = viewport->y + ry - viewport->scroll_y;

  if (UI_RendererAdapter_ClipRect(viewport, &x, &y, &width, &height) == 0)
  {
    return;
  }

  cmd = UI_RendererAdapter_AllocCommand(UI_RENDER_CMD_FILL_RECT);
  if (cmd == NULL)
  {
    return;
  }

  cmd->x1 = x;
  cmd->y1 = y;
  cmd->width = width;
  cmd->height = height;
  cmd->color = color;
}

void UI_RendererAdapter_DrawRect(const ui_viewport_t *viewport,
                                 int16_t rx,
                                 int16_t ry,
                                 int16_t width,
                                 int16_t height,
                                 uint16_t color)
{
  ui_render_cmd_t *cmd;
  int16_t x;
  int16_t y;

  if (viewport == NULL)
  {
    return;
  }

  x = viewport->x + rx - viewport->scroll_x;
  y = viewport->y + ry - viewport->scroll_y;

  if (UI_RendererAdapter_ClipRect(viewport, &x, &y, &width, &height) == 0)
  {
    return;
  }

  cmd = UI_RendererAdapter_AllocCommand(UI_RENDER_CMD_RECT);
  if (cmd == NULL)
  {
    return;
  }

  cmd->x1 = x;
  cmd->y1 = y;
  cmd->width = width;
  cmd->height = height;
  cmd->color = color;
}

void UI_RendererAdapter_DrawLine(const ui_viewport_t *viewport,
                                 int16_t rx1,
                                 int16_t ry1,
                                 int16_t rx2,
                                 int16_t ry2,
                                 uint16_t color)
{
  ui_render_cmd_t *cmd;
  int16_t x1;
  int16_t y1;
  int16_t x2;
  int16_t y2;

  if (viewport == NULL)
  {
    return;
  }

  x1 = viewport->x + rx1 - viewport->scroll_x;
  y1 = viewport->y + ry1 - viewport->scroll_y;
  x2 = viewport->x + rx2 - viewport->scroll_x;
  y2 = viewport->y + ry2 - viewport->scroll_y;

  if (UI_RendererAdapter_ClipLine(viewport, &x1, &y1, &x2, &y2) == 0)
  {
    return;
  }

  cmd = UI_RendererAdapter_AllocCommand(UI_RENDER_CMD_LINE);
  if (cmd == NULL)
  {
    return;
  }

  cmd->x1 = x1;
  cmd->y1 = y1;
  cmd->x2 = x2;
  cmd->y2 = y2;
  cmd->color = color;
}

void UI_RendererAdapter_DrawTextInRect(const ui_viewport_t *viewport,
                                       const ui_rect_t *rect,
                                       const char *text,
                                       const ui_text_style_t *style)
{
  int16_t text_x;
  int16_t text_y;
  int16_t text_width;
  uint16_t color;

  if ((viewport == NULL) || (rect == NULL) || (text == NULL) || (style == NULL))
  {
    return;
  }

  (void)style->font_size;
  text_width = UI_RendererAdapter_EstimateTextWidth(text);
  text_x = rect->x + 4;

  if (style->align == UI_TEXT_ALIGN_CENTER)
  {
    text_x = rect->x + (int16_t)((rect->width - text_width) / 2);
  }
  else if (style->align == UI_TEXT_ALIGN_RIGHT)
  {
    text_x = rect->x + rect->width - text_width - 4;
  }

  text_x = UI_RendererAdapter_ClampInt16(text_x, rect->x, (int16_t)(rect->x + rect->width - 1));
  text_y = rect->y + (int16_t)((rect->height - UI_RENDERER_FONT_HEIGHT) / 2);
  if (text_y < rect->y)
  {
    text_y = rect->y;
  }

  color = style->color;
  UI_RendererAdapter_DrawText(viewport, text_x, text_y, text, color);
}

void UI_RendererAdapter_DrawPanel(const ui_viewport_t *viewport,
                                  const ui_rect_t *rect,
                                  const ui_render_style_t *style)
{
  if ((viewport == NULL) || (rect == NULL) || (style == NULL))
  {
    return;
  }

  (void)style->radius;
  UI_RendererAdapter_DrawFillRect(viewport, rect->x, rect->y, rect->width, rect->height, style->background);
  if (style->border_width > 0U)
  {
    UI_RendererAdapter_DrawRect(viewport, rect->x, rect->y, rect->width, rect->height, style->border);
  }
}

void UI_RendererAdapter_DrawButton(const ui_viewport_t *viewport,
                                   const ui_rect_t *rect,
                                   const char *text,
                                   const ui_render_style_t *style,
                                   uint8_t pressed)
{
  ui_text_style_t text_style;
  ui_rect_t text_rect;
  ui_color_t fill_color;

  if ((viewport == NULL) || (rect == NULL) || (style == NULL))
  {
    return;
  }

  fill_color = (pressed != 0U) ? style->accent : style->background;
  UI_RendererAdapter_DrawFillRect(viewport, rect->x, rect->y, rect->width, rect->height, fill_color);
  UI_RendererAdapter_DrawRect(viewport, rect->x, rect->y, rect->width, rect->height, style->border);
  UI_RendererAdapter_DrawFillRect(viewport, rect->x, rect->y, 3, rect->height, style->accent);

  text_style.color = style->text;
  text_style.font_size = UI_FONT_SIZE_12;
  text_style.align = UI_TEXT_ALIGN_CENTER;
  text_rect = *rect;
  UI_RendererAdapter_DrawTextInRect(viewport, &text_rect, text, &text_style);
}

void UI_RendererAdapter_DrawProgressBar(const ui_viewport_t *viewport,
                                        const ui_rect_t *rect,
                                        uint8_t percent,
                                        const ui_render_style_t *style)
{
  int16_t fill_width;

  if ((viewport == NULL) || (rect == NULL) || (style == NULL))
  {
    return;
  }

  percent = UI_RendererAdapter_ClampPercent(percent);
  fill_width = (int16_t)(((int32_t)(rect->width - 4) * percent) / 100);
  if (fill_width < 0)
  {
    fill_width = 0;
  }

  UI_RendererAdapter_DrawFillRect(viewport, rect->x, rect->y, rect->width, rect->height, style->background);
  UI_RendererAdapter_DrawRect(viewport, rect->x, rect->y, rect->width, rect->height, style->border);
  UI_RendererAdapter_DrawFillRect(viewport, (int16_t)(rect->x + 2), (int16_t)(rect->y + 2), fill_width, (int16_t)(rect->height - 4), style->accent);
}

void UI_RendererAdapter_DrawSlider(const ui_viewport_t *viewport,
                                   const ui_rect_t *rect,
                                   int32_t value,
                                   int32_t min_value,
                                   int32_t max_value,
                                   const ui_render_style_t *style)
{
  int32_t range;
  int16_t knob_x;
  int16_t track_y;

  if ((viewport == NULL) || (rect == NULL) || (style == NULL) || (max_value <= min_value))
  {
    return;
  }

  value = (value < min_value) ? min_value : value;
  value = (value > max_value) ? max_value : value;
  range = max_value - min_value;
  track_y = rect->y + (int16_t)(rect->height / 2) - 2;
  knob_x = rect->x + (int16_t)(((int32_t)(rect->width - 12) * (value - min_value)) / range);

  UI_RendererAdapter_DrawFillRect(viewport, rect->x, track_y, rect->width, 4, style->background);
  UI_RendererAdapter_DrawFillRect(viewport, rect->x, track_y, (int16_t)(knob_x - rect->x + 6), 4, style->accent);
  UI_RendererAdapter_DrawRect(viewport, knob_x, (int16_t)(track_y - 4), 12, 12, style->border);
  UI_RendererAdapter_DrawFillRect(viewport, (int16_t)(knob_x + 2), (int16_t)(track_y - 2), 8, 8, style->accent);
}

void UI_RendererAdapter_DrawToggle(const ui_viewport_t *viewport,
                                   const ui_rect_t *rect,
                                   uint8_t is_on,
                                   const ui_render_style_t *style)
{
  int16_t knob_x;

  if ((viewport == NULL) || (rect == NULL) || (style == NULL))
  {
    return;
  }

  knob_x = (is_on != 0U) ? (int16_t)(rect->x + rect->width - rect->height + 2) : (int16_t)(rect->x + 2);

  UI_RendererAdapter_DrawFillRect(viewport, rect->x, rect->y, rect->width, rect->height, (is_on != 0U) ? style->accent : style->background);
  UI_RendererAdapter_DrawRect(viewport, rect->x, rect->y, rect->width, rect->height, style->border);
  UI_RendererAdapter_DrawFillRect(viewport, knob_x, (int16_t)(rect->y + 2), (int16_t)(rect->height - 4), (int16_t)(rect->height - 4), style->text);
}

void UI_RendererAdapter_DrawCheckbox(const ui_viewport_t *viewport,
                                     const ui_rect_t *rect,
                                     uint8_t is_checked,
                                     const char *text,
                                     const ui_render_style_t *style)
{
  ui_text_style_t text_style;
  ui_rect_t label_rect;
  int16_t box_size;

  if ((viewport == NULL) || (rect == NULL) || (style == NULL))
  {
    return;
  }

  box_size = (rect->height < 16) ? rect->height : 16;
  UI_RendererAdapter_DrawRect(viewport, rect->x, (int16_t)(rect->y + 1), box_size, box_size, style->border);
  if (is_checked != 0U)
  {
    UI_RendererAdapter_DrawLine(viewport, (int16_t)(rect->x + 3), (int16_t)(rect->y + 8), (int16_t)(rect->x + 7), (int16_t)(rect->y + 13), style->accent);
    UI_RendererAdapter_DrawLine(viewport, (int16_t)(rect->x + 7), (int16_t)(rect->y + 13), (int16_t)(rect->x + 14), (int16_t)(rect->y + 4), style->accent);
  }

  label_rect.x = rect->x + box_size + 6;
  label_rect.y = rect->y;
  label_rect.width = rect->width - box_size - 6;
  label_rect.height = rect->height;
  text_style.color = style->text;
  text_style.font_size = UI_FONT_SIZE_12;
  text_style.align = UI_TEXT_ALIGN_LEFT;
  UI_RendererAdapter_DrawTextInRect(viewport, &label_rect, text, &text_style);
}

void UI_RendererAdapter_DrawListItem(const ui_viewport_t *viewport,
                                     const ui_rect_t *rect,
                                     const char *title,
                                     const char *subtitle,
                                     uint8_t selected,
                                     const ui_render_style_t *style)
{
  ui_color_t fill_color;

  if ((viewport == NULL) || (rect == NULL) || (style == NULL))
  {
    return;
  }

  fill_color = (selected != 0U) ? style->background : UI_RendererAdapter_RGB565(10U, 24U, 38U);
  UI_RendererAdapter_DrawFillRect(viewport, rect->x, rect->y, rect->width, rect->height, fill_color);
  UI_RendererAdapter_DrawRect(viewport, rect->x, rect->y, rect->width, rect->height, style->border);
  UI_RendererAdapter_DrawFillRect(viewport, rect->x, rect->y, 3, rect->height, (selected != 0U) ? style->accent : style->border);
  UI_RendererAdapter_DrawText(viewport, (int16_t)(rect->x + 10), (int16_t)(rect->y + 6), title, style->text);
  UI_RendererAdapter_DrawText(viewport, (int16_t)(rect->x + 10), (int16_t)(rect->y + 22), subtitle, style->muted);
}

void UI_RendererAdapter_DrawBadge(const ui_viewport_t *viewport,
                                  const ui_rect_t *rect,
                                  const char *text,
                                  const ui_render_style_t *style)
{
  ui_text_style_t text_style;

  if ((viewport == NULL) || (rect == NULL) || (style == NULL))
  {
    return;
  }

  UI_RendererAdapter_DrawFillRect(viewport, rect->x, rect->y, rect->width, rect->height, style->accent);
  UI_RendererAdapter_DrawRect(viewport, rect->x, rect->y, rect->width, rect->height, style->border);
  text_style.color = style->text;
  text_style.font_size = UI_FONT_SIZE_12;
  text_style.align = UI_TEXT_ALIGN_CENTER;
  UI_RendererAdapter_DrawTextInRect(viewport, rect, text, &text_style);
}

void UI_RendererAdapter_DrawSparkline(const ui_viewport_t *viewport,
                                      const ui_rect_t *rect,
                                      const int16_t *values,
                                      uint8_t count,
                                      int16_t min_value,
                                      int16_t max_value,
                                      const ui_render_style_t *style)
{
  uint8_t i;
  int32_t range;
  int16_t prev_x;
  int16_t prev_y;

  if ((viewport == NULL) || (rect == NULL) || (values == NULL) || (style == NULL) || (count < 2U) || (max_value <= min_value))
  {
    return;
  }

  range = max_value - min_value;
  UI_RendererAdapter_DrawRect(viewport, rect->x, rect->y, rect->width, rect->height, style->border);
  prev_x = rect->x;
  prev_y = rect->y + rect->height - 2 - (int16_t)(((int32_t)(rect->height - 4) * (values[0] - min_value)) / range);

  for (i = 1U; i < count; ++i)
  {
    int16_t x = rect->x + (int16_t)(((int32_t)(rect->width - 2) * i) / (count - 1U));
    int16_t y = rect->y + rect->height - 2 - (int16_t)(((int32_t)(rect->height - 4) * (values[i] - min_value)) / range);

    y = UI_RendererAdapter_ClampInt16(y, (int16_t)(rect->y + 1), (int16_t)(rect->y + rect->height - 2));
    UI_RendererAdapter_DrawLine(viewport, prev_x, prev_y, x, y, style->accent);
    prev_x = x;
    prev_y = y;
  }
}

void UI_RendererAdapter_DrawIconBox(const ui_viewport_t *viewport,
                                    const ui_rect_t *rect,
                                    const char *icon_text,
                                    const char *label,
                                    const ui_render_style_t *style)
{
  ui_text_style_t text_style;
  ui_rect_t icon_rect;
  ui_rect_t label_rect;

  if ((viewport == NULL) || (rect == NULL) || (style == NULL))
  {
    return;
  }

  UI_RendererAdapter_DrawPanel(viewport, rect, style);
  icon_rect.x = rect->x + 6;
  icon_rect.y = rect->y + 6;
  icon_rect.width = 34;
  icon_rect.height = rect->height - 12;
  UI_RendererAdapter_DrawFillRect(viewport, icon_rect.x, icon_rect.y, icon_rect.width, icon_rect.height, style->accent);

  text_style.color = style->text;
  text_style.font_size = UI_FONT_SIZE_12;
  text_style.align = UI_TEXT_ALIGN_CENTER;
  UI_RendererAdapter_DrawTextInRect(viewport, &icon_rect, icon_text, &text_style);

  label_rect.x = rect->x + 46;
  label_rect.y = rect->y;
  label_rect.width = rect->width - 50;
  label_rect.height = rect->height;
  text_style.align = UI_TEXT_ALIGN_LEFT;
  UI_RendererAdapter_DrawTextInRect(viewport, &label_rect, label, &text_style);
}
