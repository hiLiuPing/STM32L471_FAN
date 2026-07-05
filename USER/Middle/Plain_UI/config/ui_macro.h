#ifndef UI_MACRO_H
#define UI_MACRO_H

#define UI_ARRAY_SIZE(arr) ((uint32_t)(sizeof(arr) / sizeof((arr)[0])))
#define UI_MIN(a, b)       (((a) < (b)) ? (a) : (b))
#define UI_MAX(a, b)       (((a) > (b)) ? (a) : (b))

#endif /* UI_MACRO_H */
