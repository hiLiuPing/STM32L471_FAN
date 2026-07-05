#ifndef UI_TYPES_H
#define UI_TYPES_H

#include <stdint.h>

typedef struct
{
  int16_t x;
  int16_t y;
} ui_point_t;

typedef struct
{
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
} ui_rect_t;

typedef struct
{
  int16_t x;
  int16_t y;
  int16_t width;
  int16_t height;
  int16_t scroll_x;
  int16_t scroll_y;
} ui_viewport_t;

#endif /* UI_TYPES_H */
