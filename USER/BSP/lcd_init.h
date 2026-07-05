#ifndef _LCD_INIT_H_
#define _LCD_INIT_H_

#include "main.h"

/* ����Һ���ֱ��� */
#define USE_HORIZONTAL 3 // ���ú�������������ʾ 0��1Ϊ���� 2��3Ϊ����
#if USE_HORIZONTAL == 0 || USE_HORIZONTAL == 1
#define LCD_W 142
#define LCD_H 428
#else
#define LCD_W 428
#define LCD_H 142
#endif


#define LCD_RES_Clr() HAL_GPIO_WritePin(SPI1_RST_GPIO_Port, SPI1_RST_Pin, GPIO_PIN_RESET)
#define LCD_RES_Set() HAL_GPIO_WritePin(SPI1_RST_GPIO_Port, SPI1_RST_Pin, GPIO_PIN_SET)

#define LCD_DC_Clr() HAL_GPIO_WritePin(SPI1_DC_GPIO_Port, SPI1_DC_Pin, GPIO_PIN_RESET)
#define LCD_DC_Set() HAL_GPIO_WritePin(SPI1_DC_GPIO_Port, SPI1_DC_Pin, GPIO_PIN_SET)

#define LCD_CS_Clr() HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_RESET)
#define LCD_CS_Set() HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET)

#define LCD_BLK_Clr() HAL_GPIO_WritePin(SPI1_PWM_GPIO_Port, SPI1_PWM_Pin, GPIO_PIN_RESET)
#define LCD_BLK_Set() HAL_GPIO_WritePin(SPI1_PWM_GPIO_Port, SPI1_PWM_Pin, GPIO_PIN_SET)

/* ����˵�� */
// uint16_t LCD_ReadID(void);                                                         // ��ȡ��ĻID
void LCD_SetCursor(uint16_t x, uint16_t y);                                        // ���ù��λ��
void LCD_Address_Set(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye);          // �������꺯��
void LCD_Fill(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color); // ��亯��
void LCD_Init(void);                                                               // ����LCD��ʼ��
void LCD_Color_Render(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, const uint16_t *color_p);
void  MY_LCD_Fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color);
void LCD_WR_Bus(uint8_t dat);
void LCD_WR_DATA8(uint8_t dat);
void LCD_WR_DATA(uint16_t dat);
void LCD_WR_REG(uint8_t reg);

/* ���廭����ɫ */
#define WHITE 0xFFFF
#define BLACK 0x0000
#define BLUE 0x001F
#define BRED 0XF81F
#define GRED 0XFFE0
#define GBLUE 0X07FF
#define RED 0xF800
#define MAGENTA 0xF81F
#define GREEN 0x07E0
#define CYAN 0x7FFF
#define YELLOW 0xFFE0
#define BROWN 0XBC40      // ��ɫ
#define BRRED 0XFC07      // �غ�ɫ
#define GRAY 0X8430       // ��ɫ
#define DARKBLUE 0X01CF   // ����ɫ
#define LIGHTBLUE 0X7D7C  // ǳ��ɫ
#define GRAYBLUE 0X5458   // ����ɫ
#define LIGHTGREEN 0X841F // ǳ��ɫ
#define LGRAY 0XC618      // ǳ��ɫ(PANNEL),���屳��ɫ
#define LGRAYBLUE 0XA651  // ǳ����ɫ(�м����ɫ)
#define LBBLUE 0X2B12     // ǳ����ɫ(ѡ����Ŀ�ķ�ɫ)

#endif
