#include "ui_dirty_region.h"

#include <string.h>

static ui_rect_t g_dirty_regions[UI_DIRTY_REGION_CAPACITY];
static size_t g_dirty_region_count;

static int UI_DirtyRegion_OverlapsOrTouches(const ui_rect_t *a, const ui_rect_t *b)
{
  int16_t a_right = a->x + a->width;
  int16_t a_bottom = a->y + a->height;
  int16_t b_right = b->x + b->width;
  int16_t b_bottom = b->y + b->height;

  if ((a_right < (b->x - 1)) || (b_right < (a->x - 1)))
  {
    return 0;
  }

  if ((a_bottom < (b->y - 1)) || (b_bottom < (a->y - 1)))
  {
    return 0;
  }

  return 1;
}

static void UI_DirtyRegion_Merge(ui_rect_t *dst, const ui_rect_t *src)
{
  int16_t x1 = (dst->x < src->x) ? dst->x : src->x;
  int16_t y1 = (dst->y < src->y) ? dst->y : src->y;
  int16_t x2 = ((dst->x + dst->width) > (src->x + src->width)) ? (dst->x + dst->width) : (src->x + src->width);
  int16_t y2 = ((dst->y + dst->height) > (src->y + src->height)) ? (dst->y + dst->height) : (src->y + src->height);

  dst->x = x1;
  dst->y = y1;
  dst->width = x2 - x1;
  dst->height = y2 - y1;
}

void UI_DirtyRegion_Init(void)
{
  memset(g_dirty_regions, 0, sizeof(g_dirty_regions));
  g_dirty_region_count = 0U;
}

void UI_DirtyRegion_Invalidate(const ui_rect_t *rect)
{
  size_t i;

  if (rect == NULL)
  {
    return;
  }

  for (i = 0U; i < g_dirty_region_count; ++i)
  {
    if (UI_DirtyRegion_OverlapsOrTouches(&g_dirty_regions[i], rect) != 0)
    {
      UI_DirtyRegion_Merge(&g_dirty_regions[i], rect);
      return;
    }
  }

  if (g_dirty_region_count < UI_DIRTY_REGION_CAPACITY)
  {
    g_dirty_regions[g_dirty_region_count++] = *rect;
  }
  else
  {
    UI_DirtyRegion_Merge(&g_dirty_regions[0], rect);
  }
}

size_t UI_DirtyRegion_Take(ui_rect_t *rects, size_t max_rect_count)
{
  size_t i;
  size_t count = (g_dirty_region_count < max_rect_count) ? g_dirty_region_count : max_rect_count;

  if ((rects != NULL) && (count > 0U))
  {
    for (i = 0U; i < count; ++i)
    {
      rects[i] = g_dirty_regions[i];
    }
  }

  g_dirty_region_count = 0U;
  return count;
}
