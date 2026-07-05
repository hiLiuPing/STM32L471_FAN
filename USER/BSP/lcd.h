#ifndef _LCD_H_
#define _LCD_H_

#include <stdint.h>

void LCD_Color_Render(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, const uint16_t *color_p);

#include "lcd_init.h"

void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);                                                                                // »­µãº¯Êý
void LCD_DrawLine(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color);                                                     // »®Ïßº¯Êý
void LCD_DrawRectangle(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color);                                                // »æÖÆ¿ÕÐÄ¾ØÐÎ
void LCD_DrawFillRectangle(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color);                                            // »æÖÆÊµÐÄ¾ØÐÎ
void Draw_Circle(int xc, int yc, int x, int y, uint16_t color);                                                                            // 8·Ö·¨»­Ô²ÄÚ²¿µ÷ÓÃ
void LCD_DrawCircle(uint16_t xc, uint16_t yc, uint16_t r, uint16_t color, uint16_t mode);                                                  // »æÖÆÔ²ÐÎ
void LCD_DrawTriangel(uint16_t x, uint16_t y, uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color);                         // »æÖÆ¿ÕÐÄÈý½ÇÐÎ
void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode);                             // ÏÔÊ¾×Ö·ûº¯Êý
void LCD_ShowString(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint16_t sizey, uint8_t mode);                           // ÏÔÊ¾×Ö·û´®
uint32_t mypow(uint8_t m, uint8_t n);                                                                                                      // ÃÝÔËËãÄÚ²¿µ÷ÓÃ
void LCD_ShowNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode);                // ÏÔÊ¾Êý×Ö
void LCD_ShowFloatNum(uint16_t x, uint16_t y, float num, uint8_t pre, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode); // ÏÔÊ¾¸¡µãÊý
void LCD_ShowChinese12x12(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode);                      // ÏÔÊ¾12x12×Ö·û´®
void LCD_ShowChinese16x16(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode);                      // ÏÔÊ¾16x16×Ö·û´®
void LCD_ShowChinese24x24(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode);                      // ÏÔÊ¾24x24×Ö·û´®
void LCD_ShowChinese32x32(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode);                      // ÏÔÊ¾32x32×Ö·û´®
void LCD_ShowChinese(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode);                           // ÏÔÊ¾ÖÐÎÄ×Ö·û´®
void LCD_ShowStr(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode);                               // ÖÐÓ¢»ìÏÔ
void LCD_StrCenter(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode);                             // ¾ÓÖÐÏÔÊ¾
void LCD_ShowPicture(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t pic[]);                                        // ÏÔÊ¾Í¼Æ¬
#endif
