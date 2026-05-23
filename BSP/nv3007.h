#ifndef __NV3007_H
#define __NV3007_H

#include "main.h"
#include <string.h>

/* SPI */
#define NV3007_SPI_PORT hspi1
extern SPI_HandleTypeDef NV3007_SPI_PORT;

/* 屏幕分辨率 */
#define NV3007_WIDTH     240
#define NV3007_HEIGHT    320

/* 偏移 */
#define TFT_COLUMN_OFFSET 12

/* 方向 */
#define NV3007_ROTATION 0

/* RGB565 */
#define RGB565(r,g,b) (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b) >> 3))

#define WHITE 0xFFFF
#define BLACK 0x0000
#define RED   0xF800
#define GREEN 0x07E0
#define BLUE  0x001F

/* 指令 */
#define CMD_SWRESET 0x01
#define CMD_SLPOUT  0x11
#define CMD_COLMOD  0x3A
#define CMD_MADCTL  0x36
#define CMD_CASET   0x2A
#define CMD_RASET   0x2B
#define CMD_RAMWR   0x2C
#define CMD_DISPON  0x29

/* GPIO */
#define CS_LOW()  HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_RESET)
#define CS_HIGH() HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET)

#define DC_LOW()  HAL_GPIO_WritePin(SPI1_DC_GPIO_Port, SPI1_DC_Pin, GPIO_PIN_RESET)
#define DC_HIGH() HAL_GPIO_WritePin(SPI1_DC_GPIO_Port, SPI1_DC_Pin, GPIO_PIN_SET)

#define RST_LOW() HAL_GPIO_WritePin(SPI1_RST_GPIO_Port, SPI1_RST_Pin, GPIO_PIN_RESET)
#define RST_HIGH()HAL_GPIO_WritePin(SPI1_RST_GPIO_Port, SPI1_RST_Pin, GPIO_PIN_SET)

#define BL_ON()   HAL_GPIO_WritePin(SPI1_PWM_GPIO_Port, SPI1_PWM_Pin, GPIO_PIN_SET)
#define BL_OFF()  HAL_GPIO_WritePin(SPI1_PWM_GPIO_Port, SPI1_PWM_Pin, GPIO_PIN_RESET)

/* API */
void NV3007_Init(void);
void NV3007_SetRotation(uint8_t m);
void NV3007_Fill(uint16_t color);
void NV3007_FillRect(uint16_t x,uint16_t y,uint16_t w,uint16_t h,uint16_t color);

#endif