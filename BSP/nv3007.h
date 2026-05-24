#ifndef __NV3007_H
#define __NV3007_H

#include "main.h"

/* SPI 接口句柄定义 */
#define NV3007_SPI_PORT hspi1
extern SPI_HandleTypeDef NV3007_SPI_PORT;

/* 屏幕物理分辨率 */
#define NV3007_WIDTH     240
#define NV3007_HEIGHT    320

/* 逻辑偏移（源自原厂 main.C 参数） */
#define TFT_COLUMN_OFFSET 12

/* 屏幕显示方向控制：0-竖屏 1-横屏 2-反向竖屏 3-反向横屏 */
#define NV3007_ROTATION 0

/* 常用标准 RGB565 颜色宏定义 */
#define WHITE   0xFFFF
#define BLACK   0x0000
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define YELLOW  0xFFE0
#define MAGENTA 0xF81F
#define CYAN    0x07FF

/* NV3007 核心标准指令 */
#define CMD_SWRESET 0x01  // 软件复位
#define CMD_SLPOUT  0x11  // 退出休眠
#define CMD_COLMOD  0x3A  // 像素格式定义
#define CMD_MADCTL  0x36  // 存储器访问控制
#define CMD_CASET   0x2A  // 列地址设置
#define CMD_RASET   0x2B  // 行地址设置
#define CMD_RAMWR   0x2C  // 显存写入
#define CMD_DISPON  0x29  // 开启显示
#define CMD_INVOFF  0x20  // 关闭反相显示
#define CMD_INVON   0x21  // 开启反相显示

/* 引脚控制宏定义（精准对接您的硬件定义） */
#define CS_LOW()  HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_RESET)
#define CS_HIGH() HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET)

#define DC_LOW()  HAL_GPIO_WritePin(SPI1_DC_GPIO_Port, SPI1_DC_Pin, GPIO_PIN_RESET)
#define DC_HIGH() HAL_GPIO_WritePin(SPI1_DC_GPIO_Port, SPI1_DC_Pin, GPIO_PIN_SET)

#define RST_LOW()  HAL_GPIO_WritePin(SPI1_RST_GPIO_Port, SPI1_RST_Pin, GPIO_PIN_RESET)
#define RST_HIGH() HAL_GPIO_WritePin(SPI1_RST_GPIO_Port, SPI1_RST_Pin, GPIO_PIN_SET)

#define BL_OFF()   HAL_GPIO_WritePin(SPI1_PWM_GPIO_Port, SPI1_PWM_Pin, GPIO_PIN_RESET)
#define BL_ON()  HAL_GPIO_WritePin(SPI1_PWM_GPIO_Port, SPI1_PWM_Pin, GPIO_PIN_SET)





/* 外部函数接口 */
void NV3007_Init(void);
// void NV3007_SetRotation(uint8_t m);
void NV3007_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void NV3007_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void NV3007_Fill(uint16_t color);
void NV3007_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

#endif /* __NV3007_H */