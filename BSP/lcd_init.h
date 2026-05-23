#ifndef __LCD_INIT_H
#define __LCD_INIT_H
#include "main.h"

#define USE_HORIZONTAL 2  //设置横屏或者竖屏显示 0或1为竖屏 2或3为横屏

#if USE_HORIZONTAL==0||USE_HORIZONTAL==1
#define LCD_W 80
#define LCD_H 160
#else
#define LCD_W 160
#define LCD_H 80
#endif
// #define SPI1_CS_Pin GPIO_PIN_4
// #define SPI1_CS_GPIO_Port GPIOA
// #define SPI1_PWM_Pin GPIO_PIN_6
// #define SPI1_PWM_GPIO_Port GPIOA
// #define SPI1_DC_Pin GPIO_PIN_4
// #define SPI1_DC_GPIO_Port GPIOC
// #define SPI1_RST_Pin GPIO_PIN_5
// #define SPI1_RST_GPIO_Port GPIOC

//-----------------LCD端口定义----------------
// 请确保在 CubeMX 中将该引脚命名为 SPI1_CS 或自行修改下方 Label
#define LCD_CS_Clr()  HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_RESET)
#define LCD_CS_Set()  HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET)

#define LCD_RES_Clr() HAL_GPIO_WritePin(SPI1_RST_GPIO_Port,SPI1_RST_Pin,GPIO_PIN_RESET)
#define LCD_RES_Set() HAL_GPIO_WritePin(SPI1_RST_GPIO_Port,SPI1_RST_Pin,GPIO_PIN_SET)

#define LCD_DC_Clr()  HAL_GPIO_WritePin(SPI1_DC_GPIO_Port,SPI1_DC_Pin,GPIO_PIN_RESET)
#define LCD_DC_Set()  HAL_GPIO_WritePin(SPI1_DC_GPIO_Port,SPI1_DC_Pin,GPIO_PIN_SET)

#define LCD_BLK_Clr() HAL_GPIO_WritePin(SPI1_PWM_GPIO_Port,SPI1_PWM_Pin,GPIO_PIN_RESET)
#define LCD_BLK_Set() HAL_GPIO_WritePin(SPI1_PWM_GPIO_Port,SPI1_PWM_Pin,GPIO_PIN_SET)

void delay_ms(unsigned int ms);
void LCD_Writ_Bus(uint8_t dat);
void LCD_WR_DATA8(uint8_t dat);
void LCD_WR_DATA(uint16_t dat);
void LCD_WR_REG(uint8_t dat);
void LCD_Address_Set(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2);
void LCD_Init(void);

void LCD_Fill(uint16_t xsta, uint16_t ysta, uint16_t xend, uint16_t yend, uint16_t *color_buf);

#endif