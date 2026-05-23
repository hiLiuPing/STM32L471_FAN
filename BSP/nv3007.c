#include "nv3007.h"

/* 屏幕缓存 */
static uint16_t width = NV3007_WIDTH;
static uint16_t height = NV3007_HEIGHT;

/* SPI写命令（单次） */
static void LCD_WriteCmd(uint8_t cmd)
{
    CS_LOW();
    DC_LOW();
    HAL_SPI_Transmit(&NV3007_SPI_PORT, &cmd, 1, HAL_MAX_DELAY);
    CS_HIGH();
}

/* SPI写数据（单次） */
static void LCD_WriteData(uint8_t data)
{
    CS_LOW();
    DC_HIGH();
    HAL_SPI_Transmit(&NV3007_SPI_PORT, &data, 1, HAL_MAX_DELAY);
    CS_HIGH();
}

/* 批量写数据（关键优化点） */
static void LCD_WriteBuf(uint8_t *buf, uint32_t len)
{
    CS_LOW();
    DC_HIGH();
    HAL_SPI_Transmit(&NV3007_SPI_PORT, buf, len, HAL_MAX_DELAY);
    CS_HIGH();
}

/* 设置窗口 */
void NV3007_SetAddressWindow(uint16_t x0,uint16_t y0,uint16_t x1,uint16_t y1)
{
    uint8_t buf[4];

    x0 += TFT_COLUMN_OFFSET;
    x1 += TFT_COLUMN_OFFSET;

    buf[0] = x0 >> 8;
    buf[1] = x0 & 0xFF;
    buf[2] = x1 >> 8;
    buf[3] = x1 & 0xFF;
    LCD_WriteCmd(CMD_CASET);
    LCD_WriteBuf(buf,4);

    buf[0] = y0 >> 8;
    buf[1] = y0 & 0xFF;
    buf[2] = y1 >> 8;
    buf[3] = y1 & 0xFF;
    LCD_WriteCmd(CMD_RASET);
    LCD_WriteBuf(buf,4);

    LCD_WriteCmd(CMD_RAMWR);
}
void NV3007_SetRotation(uint8_t m)
{
    LCD_WriteCmd(CMD_MADCTL);

    switch(m)
    {
        case 0:
            LCD_WriteData(0x00); // RGB
            width = NV3007_WIDTH;
            height = NV3007_HEIGHT;
            break;

        case 1:
            LCD_WriteData(0x60);
            width = NV3007_HEIGHT;
            height = NV3007_WIDTH;
            break;

        case 2:
            LCD_WriteData(0xC0);
            width = NV3007_WIDTH;
            height = NV3007_HEIGHT;
            break;

        case 3:
            LCD_WriteData(0xA0);
            width = NV3007_HEIGHT;
            height = NV3007_WIDTH;
            break;
    }
}
void NV3007_Fill(uint16_t color)
{
    NV3007_FillRect(0,0,width,height,color);
}
/* 🚀 高效批量刷屏函数（带内存软件自动反向） */
void NV3007_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    if ((x >= width) || (y >= height)) return;
    if ((x + w) > width)  w = width - x;
    if ((y + h) > height) h = height - y;

    // 🌟 软件反向：由于硬件INVOFF了，我们直接对要输入的标准色彩取反
    uint16_t inverted_color = ~color; 

    NV3007_SetAddressWindow(x, y, x + w - 1, y + h - 1);

    // 建立一整行的爆发缓冲区（240像素 = 480字节）
    uint8_t row_buffer[480];
    uint16_t max_pixels = (w > 240) ? 240 : w;

    // 将取反后的数据填入发送缓冲区
    for (uint16_t i = 0; i < max_pixels; i++) {
        row_buffer[i * 2]     = inverted_color >> 8;   // 高字节在前
        row_buffer[i * 2 + 1] = inverted_color & 0xFF;  // 低字节在后
    }

    uint32_t total_pixels = w * h;
    uint32_t pixels_rem  = total_pixels;

    CS_LOW();
    DC_HIGH();
    while (pixels_rem > 0) {
        uint32_t chunk = (pixels_rem > max_pixels) ? max_pixels : pixels_rem;
        HAL_SPI_Transmit(&NV3007_SPI_PORT, row_buffer, chunk * 2, HAL_MAX_DELAY);
        pixels_rem -= chunk;
    }
    CS_HIGH();
}


void NV3007_Init(void)
{
    /* =========================
     * 1. Reset
     * ========================= */
    RST_LOW();
    HAL_Delay(100);
    RST_HIGH();
    HAL_Delay(120);

    /* =========================
     * 2. Unlock
     * ========================= */
    LCD_WriteCmd(0xFF);
    LCD_WriteData(0xA5);

    /* =========================
     * 3. 基础电源/时序（保留原厂）
     * ========================= */
    LCD_WriteCmd(0x9A); LCD_WriteData(0x08);
    LCD_WriteCmd(0x9B); LCD_WriteData(0x08);
    LCD_WriteCmd(0x9C); LCD_WriteData(0xB0);
    LCD_WriteCmd(0x9D); LCD_WriteData(0x16);
    LCD_WriteCmd(0x9E); LCD_WriteData(0xC4);

    LCD_WriteCmd(0x8F);
    LCD_WriteData(0x55);
    LCD_WriteData(0x04);

    LCD_WriteCmd(0x84); LCD_WriteData(0x90);
    LCD_WriteCmd(0x83); LCD_WriteData(0x7B);
    LCD_WriteCmd(0x85); LCD_WriteData(0x33);

    /* =========================
     * 4. Power timing
     * ========================= */
    LCD_WriteCmd(0x60); LCD_WriteData(0x00);
    LCD_WriteCmd(0x70); LCD_WriteData(0x00);

    LCD_WriteCmd(0x61); LCD_WriteData(0x02);
    LCD_WriteCmd(0x71); LCD_WriteData(0x02);

    LCD_WriteCmd(0x62); LCD_WriteData(0x04);
    LCD_WriteCmd(0x72); LCD_WriteData(0x04);

    /* =========================
     * 5. GOA scan
     * ========================= */
    LCD_WriteCmd(0xA0);
    LCD_WriteData(0x2B);
    LCD_WriteData(0x24);
    LCD_WriteData(0x00);

    LCD_WriteCmd(0xA1); LCD_WriteData(0x87);
    LCD_WriteCmd(0xA2); LCD_WriteData(0x86);

    LCD_WriteCmd(0xA8); LCD_WriteData(0x36);
    LCD_WriteCmd(0xA9); LCD_WriteData(0x7E);
    LCD_WriteCmd(0xAA); LCD_WriteData(0x7E);

    /* =========================
     * 6. Frame timing
     * ========================= */
    LCD_WriteCmd(0xB2);
    LCD_WriteData(0x2C);
    LCD_WriteData(0x1B);
    LCD_WriteData(0x0B);
    LCD_WriteData(0x20);

    /* =========================
     * 7. Gamma（保留原厂）
     * ========================= */
    LCD_WriteCmd(0xE0);
    LCD_WriteData(0x00);

    LCD_WriteCmd(0xE1);
    LCD_WriteData(0x03);
    LCD_WriteData(0x0F);

    LCD_WriteCmd(0xE2);
    LCD_WriteData(0x04);

    LCD_WriteCmd(0xE3);
    LCD_WriteData(0x01);

    LCD_WriteCmd(0xE4);
    LCD_WriteData(0x0E);

    LCD_WriteCmd(0xE5);
    LCD_WriteData(0x01);

    LCD_WriteCmd(0xE6);
    LCD_WriteData(0x19);

    LCD_WriteCmd(0xE7);
    LCD_WriteData(0x10);

    LCD_WriteCmd(0xE8);
    LCD_WriteData(0x10);

    /* =========================
     * 8. TE control
     * ========================= */
    LCD_WriteCmd(0x35);
    LCD_WriteData(0x00);

    /* =========================
     * 9. Pixel format（关键）
     * ========================= */
    LCD_WriteCmd(0x3A);
    LCD_WriteData(0x05);   // RGB565

    /* =========================
     * 10. ⚠ 关键：不再乱动颜色控制
     * ========================= */
    // ❌ 不使用 0x36
    // ❌ 不使用 0x21

    /* =========================
     * 11. Sleep out / display on
     * ========================= */
    LCD_WriteCmd(0x11);
    HAL_Delay(220);

    LCD_WriteCmd(0x29);
    HAL_Delay(50);

    /* =========================
     * 12. Backlight
     * ========================= */
    BL_ON();

    /* =========================
     * 13. Clear screen
     * ========================= */
    NV3007_Fill(0x0000);
}