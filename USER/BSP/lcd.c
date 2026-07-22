#include "lcd.h"
#include "lcdfont.h"
#include "string.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

extern SPI_HandleTypeDef hspi1;
extern DMA_HandleTypeDef hdma_spi1_tx;

static volatile bool s_lcd_dma_busy = false;
static lcd_dma_done_cb_t s_lcd_dma_done_cb = NULL;
static void *s_lcd_dma_user_data = NULL;
static SemaphoreHandle_t s_lcd_dma_done_sem = NULL;
static StaticSemaphore_t s_lcd_dma_done_sem_buf;
static bool s_lcd_render_ready = false;

#define LCD_DMA_WAIT_TIMEOUT_MS 20U

static void LCD_Render_Setup(void)
{
    if (s_lcd_render_ready)
    {
        return;
    }

    /* One DMA request per pixel instead of per byte. */
    hdma_spi1_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    hdma_spi1_tx.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    (void)HAL_DMA_Init(&hdma_spi1_tx);

    s_lcd_dma_done_sem = xSemaphoreCreateBinaryStatic(&s_lcd_dma_done_sem_buf);
    s_lcd_render_ready = true;
}

static void LCD_DMA_Finish(SPI_HandleTypeDef *hspi)
{
    lcd_dma_done_cb_t done_cb;
    void *user_data;

    if ((hspi == NULL) || (hspi->Instance != SPI1) || !s_lcd_dma_busy)
    {
        return;
    }

    LCD_CS_Set();
    LCD_SPI_SetFrame16(false, false);

    done_cb = s_lcd_dma_done_cb;
    user_data = s_lcd_dma_user_data;
    s_lcd_dma_done_cb = NULL;
    s_lcd_dma_user_data = NULL;
    s_lcd_dma_busy = false;

    if (done_cb != NULL)
    {
        done_cb(user_data);
    }

    /* Wake a task blocked in LCD_Color_Render_DMA_Wait(). The callback above
     * may already have chained the next transfer; waiters re-check the busy
     * flag, so a spurious give is harmless. */
    if (s_lcd_dma_done_sem != NULL)
    {
        if (__get_IPSR() != 0U)
        {
            BaseType_t higher_prio_woken = pdFALSE;

            (void)xSemaphoreGiveFromISR(s_lcd_dma_done_sem, &higher_prio_woken);
            portYIELD_FROM_ISR(higher_prio_woken);
        }
        else
        {
            (void)xSemaphoreGive(s_lcd_dma_done_sem);
        }
    }
}

/* Recover a transfer whose DMA interrupt was lost or whose peripheral state
 * no longer progresses. The callback is completed in task context so the PFB
 * queue cannot remain permanently owned by the failed tile. */
static void LCD_DMA_ForceRecover(void)
{
    lcd_dma_done_cb_t done_cb;
    void *user_data;

    taskENTER_CRITICAL();
    if (!s_lcd_dma_busy)
    {
        taskEXIT_CRITICAL();
        return;
    }

    (void)HAL_SPI_DMAStop(&hspi1);
    __HAL_SPI_DISABLE(&hspi1);
    LCD_CS_Set();
    LCD_SPI_SetFrame16(false, false);

    done_cb = s_lcd_dma_done_cb;
    user_data = s_lcd_dma_user_data;
    s_lcd_dma_done_cb = NULL;
    s_lcd_dma_user_data = NULL;
    s_lcd_dma_busy = false;
    taskEXIT_CRITICAL();

    if (done_cb != NULL)
    {
        done_cb(user_data);
    }
}

/**
 * @brief       ÔÚLCDÆÁÄ»ÉÏ»­Ò»¸öµã
 * @param       x:ÏñËØµãÁÐ×ø±ê
 * @param       y:ÏñËØµãÐÐ×ø±ê
 * @param       color:Ìî³äÑÕÉ«Öµ
 * @retval      ÎÞ
 */
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    LCD_SetCursor(x, y);
    LCD_WR_DATA(color);
}

void LCD_Color_Render(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, const uint16_t *color_p)
{
    uint32_t pixel_count;
    const uint16_t *data;
    const uint32_t max_chunk = 0xFFFFU;

    if ((color_p == NULL) || (xe < xs) || (ye < ys))
    {
        return;
    }

    LCD_Render_Setup();

    pixel_count = ((uint32_t)xe - xs + 1U) * ((uint32_t)ye - ys + 1U);
    data = color_p;

    LCD_Address_Set(xs, ys, xe, ye);

    LCD_DC_Set();
    LCD_CS_Clr();
    LCD_SPI_SetFrame16(true, true);

    while (pixel_count > 0U)
    {
        uint16_t chunk = (pixel_count > max_chunk) ? (uint16_t)max_chunk : (uint16_t)pixel_count;

        if (HAL_SPI_Transmit(&hspi1, (uint8_t *)data, chunk, HAL_MAX_DELAY) != HAL_OK)
        {
            break;
        }

        data += chunk;
        pixel_count -= chunk;
    }

    LCD_SPI_SetFrame16(false, true);
    LCD_CS_Set();
}

bool LCD_Color_Render_DMA(uint16_t xs,
                          uint16_t ys,
                          uint16_t xe,
                          uint16_t ye,
                          const uint16_t *color_p,
                          lcd_dma_done_cb_t done_cb,
                          void *user_data)
{
    uint32_t pixel_count;

    if ((color_p == NULL) || (xe < xs) || (ye < ys) || s_lcd_dma_busy)
    {
        return false;
    }

    pixel_count = ((uint32_t)xe - xs + 1U) * ((uint32_t)ye - ys + 1U);

    /* 16-bit frames: the DMA item count is the pixel count. */
    if ((pixel_count == 0U) || (pixel_count > 0xFFFFU))
    {
        return false;
    }

    LCD_Render_Setup();

    LCD_Address_Set(xs, ys, xe, ye);

    s_lcd_dma_done_cb = done_cb;
    s_lcd_dma_user_data = user_data;
    s_lcd_dma_busy = true;

    LCD_DC_Set();
    LCD_CS_Clr();
    LCD_SPI_SetFrame16(true, true);

    if (HAL_SPI_Transmit_DMA(&hspi1, (uint8_t *)color_p, (uint16_t)pixel_count) != HAL_OK)
    {
        LCD_SPI_SetFrame16(false, false);
        LCD_CS_Set();
        s_lcd_dma_done_cb = NULL;
        s_lcd_dma_user_data = NULL;
        s_lcd_dma_busy = false;
        return false;
    }

    return true;
}

void LCD_Color_Render_DMA_Wait(void)
{
    TickType_t wait_start = xTaskGetTickCount();

    while (s_lcd_dma_busy)
    {
        /* Block on the completion semaphore instead of spinning: during a
         * full-tile SPI transfer (~170us per 428x16 tile) the CPU is handed
         * back to other tasks. Falls back to spinning before the scheduler
         * runs or in interrupt context. */
        if ((s_lcd_dma_done_sem != NULL) &&
            (__get_IPSR() == 0U) &&
            (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING))
        {
            (void)xSemaphoreTake(s_lcd_dma_done_sem, pdMS_TO_TICKS(10U));
        }

        if ((xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) &&
            ((xTaskGetTickCount() - wait_start) >= pdMS_TO_TICKS(LCD_DMA_WAIT_TIMEOUT_MS)))
        {
            LCD_DMA_ForceRecover();
            wait_start = xTaskGetTickCount();
        }
    }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    LCD_DMA_Finish(hspi);
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    LCD_DMA_Finish(hspi);
}

/**
 * @brief       Á½µãÖ®¼ä»­Ïßº¯Êý
 * @param       xs:¾ØÐÎµÄÆðÊ¼ÁÐ×ø±ê
 * @param       ys:¾ØÐÎµÄÆðÊ¼ÐÐ×ø±ê
 * @param       xe:¾ØÐÎµÄ½áÊøÁÐ×ø±ê
 * @param       ye:¾ØÐÎµÄ½áÊøÐÐ×ø±ê
 * @param       color:¾ØÐÎµÄÑÕÉ«Öµ
 * @retval      ÎÞ
 */
void LCD_DrawLine(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color)
{
    uint16_t t;
    int xerr = 0, yerr = 0, delta_x, delta_y, distance;
    int incx, incy, uRow, uCol;
    delta_x = xe - xs; // ¼ÆËã×ø±êÔöÁ¿
    delta_y = ye - ys;
    uRow = xs;
    uCol = ys;
    if (delta_x > 0)
    {
        incx = 1; // ÉèÖÃµ¥²½·½Ïò
    }
    else if (delta_x == 0)
    {
        incx = 0; // ´¹¾ØÐÎ
    }
    else
    {
        incx = -1;
        delta_x = -delta_x;
    }
    if (delta_y > 0)
    {
        incy = 1;
    }
    else if (delta_y == 0)
    {
        incy = 0; // Ë®Æ½Ïß
    }
    else
    {
        incy = -1;
        delta_y = -delta_y;
    }
    if (delta_x > delta_y)
    {
        distance = delta_x; // Ñ¡È¡»ù±¾ÔöÁ¿×ø±êÖá
    }
    else
    {
        distance = delta_y;
    }
    for (t = 0; t <= distance + 1; t++) // »­ÏßÊä³ö
    {
        LCD_DrawPoint(uRow, uCol, color); // »­µã
        xerr += delta_x;
        yerr += delta_y;
        if (xerr > distance)
        {
            xerr -= distance;
            uRow += incx;
        }
        if (yerr > distance)
        {
            yerr -= distance;
            uCol += incy;
        }
    }
}

/**
 * @brief       »æÖÆ¿ÕÐÄ¾ØÐÎº¯Êý
 * @param       xs:¾ØÐÎµÄÆðÊ¼ÁÐ×ø±ê
 * @param       ys:¾ØÐÎµÄÆðÊ¼ÐÐ×ø±ê
 * @param       xe:¾ØÐÎµÄ½áÊøÁÐ×ø±ê
 * @param       ye:¾ØÐÎµÄ½áÊøÐÐ×ø±ê
 * @param       color:¾ØÐÎµÄÑÕÉ«Öµ
 * @retval      ÎÞ
 */
void LCD_DrawRectangle(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color)
{
    LCD_DrawLine(xs, ys, xe, ys, color);
    LCD_DrawLine(xs, ys, xs, ye, color);
    LCD_DrawLine(xs, ye, xe, ye, color);
    LCD_DrawLine(xe, ys, xe, ye, color);
}

/**
 * @brief       »æÖÆÊµÐÄ¾ØÐÎº¯Êý
 * @param       xs:¾ØÐÎµÄÆðÊ¼ÁÐ×ø±ê
 * @param       ys:¾ØÐÎµÄÆðÊ¼ÐÐ×ø±ê
 * @param       xe:¾ØÐÎµÄ½áÊøÁÐ×ø±ê
 * @param       ye:¾ØÐÎµÄ½áÊøÐÐ×ø±ê
 * @param       color:¾ØÐÎµÄÑÕÉ«Öµ
 * @retval      ÎÞ
 */
void LCD_DrawFillRectangle(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color)
{
    LCD_Fill(xs, ys, xe, ye, color);
}

/**
 * @brief       8·Ö·¨»æÖÆÔ²ÐÎ(ÄÚ²¿µ÷ÓÃ)
 * @param       xc:Ô²ÐÄÁÐ×ø±ê
 * @param       yc:Ô²ÐÄÐÐ×ø±ê
 * @param       x:Ïà¶ÔÓÚÔ²ÐÄµÄÁÐ×ø±ê
 * @param       y:Ïà¶ÔÓÚÔ²ÐÄµÄÐÐ×ø±ê
 * @param       color:Ô²ÐÎµÄÑÕÉ«Öµ
 * @retval      ÎÞ
 */
void Draw_Circle(int xc, int yc, int x, int y, uint16_t color)
{
    LCD_DrawPoint(xc + x, yc + y, color);
    LCD_DrawPoint(xc - x, yc + y, color);
    LCD_DrawPoint(xc + x, yc - y, color);
    LCD_DrawPoint(xc - x, yc - y, color);
    LCD_DrawPoint(xc + y, yc + x, color);
    LCD_DrawPoint(xc - y, yc + x, color);
    LCD_DrawPoint(xc + y, yc - x, color);
    LCD_DrawPoint(xc - y, yc - x, color);
}

/**
 * @brief       »æÖÆÔ²ÐÎ
 * @param       xc:Ô²ÐÄÁÐ×ø±ê
 * @param       yc:Ô²ÐÄÐÐ×ø±ê
 * @param       r:Ô²ÐÎ°ë¾¶
 * @param       color:Ô²ÐÎµÄÑÕÉ«Öµ
 * @param       mode:ÊÇ·ñÌî³äÔ²ÐÎ
 * @retval      ÎÞ
 */
void LCD_DrawCircle(uint16_t xc, uint16_t yc, uint16_t r, uint16_t color, uint16_t mode)
{
    int x = 0, y = r, yi, d;
    d = 3 - 2 * r;
    /*»æÖÆÊµÐÄÔ²*/
    if (mode)
    {
        while (x <= y)
        {
            for (yi = x; yi <= y; yi++)
            {
                Draw_Circle(xc, yc, x, yi, color);
            }
            if (d < 0)
            {
                d = d + 4 * x + 6;
            }
            else
            {
                d = d + 4 * (x - y) + 10;
                y--;
            }
            x++;
        }
    }
    /*»æÖÆ¿ÕÐÄÔ²*/
    else
    {
        while (x <= y)
        {
            Draw_Circle(xc, yc, x, y, color);
            if (d < 0)
            {
                d = d + 4 * x + 6;
            }
            else
            {
                d = d + 4 * (x - y) + 10;
                y--;
            }
            x++;
        }
    }
}

/**
 * @brief       »æÖÆ¿ÕÐÄÈý½ÇÐÎ
 * @param       x:Èý½ÇÐÎ×ø±êÁÐÆðÊ¼×ø±ê
 * @param       y:Èý½ÇÐÎ×ø±êÐÐÆðÊ¼×ø±ê
 * @param       xs:Èý½ÇÐÎ¶¥µãÁÐ×ø±ê
 * @param       ys:Èý½ÇÐÎ¶¥µãÐÐ×ø±ê
 * @param       xe:Èý½ÇÐÎÄ©¶ËÁÐ×ø±ê
 * @param       ye:Èý½ÇÐÎÄ©¶ËÐÐ×ø±ê
 * @param       color:Èý½ÇÐÎµÄÑÕÉ«Öµ
 * @retval      ÎÞ
 */
void LCD_DrawTriangel(uint16_t x, uint16_t y, uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color)
{
    LCD_DrawLine(x, y, xs, ys, color);
    LCD_DrawLine(xs, ys, xe, ye, color);
    LCD_DrawLine(xe, ye, x, y, color);
}

/**
 * @brief       ÏÔÊ¾µ¥¸ö×Ö·û
 * @param       x:×Ö·ûÏÔÊ¾Î»ÖÃÁÐÆðÊ¼×ø±ê
 * @param       y:×Ö·ûÏÔÊ¾Î»ÖÃÐÐÆðÊ¼×ø±ê
 * @param       num:ÏÔÊ¾×Ö·ûµÄASCIIÂë
 * @param       fc:×Ö·ûÑÕÉ«
 * @param       bc:×Ö·û±³¾°ÑÕÉ«
 * @param       sizey:×Ö·û´óÐ¡
 * @param       mode:ÊÇ·ñµþ¼ÓÏÔÊ¾
 * @retval      ÎÞ
 */
void LCD_ShowChar(uint16_t x, uint16_t y, uint8_t num, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
    uint8_t temp, sizex, t, m = 0;
    uint16_t i, TypefaceNum; // Ò»¸ö×Ö·ûËùÕ¼×Ö½Ú´óÐ¡
    uint16_t x0 = x;
    sizex = sizey / 2;
    TypefaceNum = (sizex / 8 + ((sizex % 8) ? 1 : 0)) * sizey;
    num = num - ' ';                                     // µÃµ½Æ«ÒÆºóµÄÖµ
    LCD_Address_Set(x, y, x + sizex - 1, y + sizey - 1); // ÉèÖÃÏÔÊ¾´°¿Ú
    for (i = 0; i < TypefaceNum; i++)
    {
        if (sizey == 12)
            temp = ascii_1206[num][i]; // µ÷ÓÃ6x12×ÖÌå
        else if (sizey == 16)
            temp = ascii_1608[num][i]; // µ÷ÓÃ8x16×ÖÌå
        else if (sizey == 24)
            temp = ascii_2412[num][i]; // µ÷ÓÃ12x24×ÖÌå
        else if (sizey == 32)
            temp = ascii_3216[num][i]; // µ÷ÓÃ16x32×ÖÌå
        else
            return;
        for (t = 0; t < 8; t++)
        {
            if (!mode) // ·Çµþ¼ÓÄ£Ê½
            {
                if (temp & (0x01 << t))
                    LCD_WR_DATA(fc);
                else
                    LCD_WR_DATA(bc);
                m++;
                if (m % sizex == 0)
                {
                    m = 0;
                    break;
                }
            }
            else // µþ¼ÓÄ£Ê½
            {
                if (temp & (0x01 << t))
                    LCD_DrawPoint(x, y, fc); // »­Ò»¸öµã
                x++;
                if ((x - x0) == sizex)
                {
                    x = x0;
                    y++;
                    break;
                }
            }
        }
    }
}

/**
 * @brief       ÏÔÊ¾×Ö·û´®
 * @param       x:×Ö·û´®ÏÔÊ¾Î»ÖÃÁÐÆðÊ¼×ø±ê
 * @param       y:×Ö·û´®ÏÔÊ¾Î»ÖÃÐÐÆðÊ¼×ø±ê
 * @param       *s:ÏÔÊ¾µÄ×Ö·û´®ÄÚÈÝ
 * @param       fc:×Ö·ûÑÕÉ«
 * @param       bc:×Ö·û±³¾°ÑÕÉ«
 * @param       sizey:×Ö·û´óÐ¡
 * @param       mode:ÊÇ·ñµþ¼ÓÏÔÊ¾
 * @retval      ÎÞ
 */
void LCD_ShowString(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint16_t sizey, uint8_t mode)
{
    while ((*s <= '~') && (*s >= ' ')) // ÅÐ¶ÏÊÇ²»ÊÇ·Ç·¨×Ö·û
    {
        if (x > (LCD_W - 1) || y > (LCD_H - 1))
            return;
        LCD_ShowChar(x, y, *s, fc, bc, sizey, mode);
        x += sizey / 2;
        s++;
    }
}

/**
 * @brief       ÃÝÔËËã(ÄÚ²¿µ÷ÓÃ)
 * @param       m:µ×Êý
 * @param       n:Ö¸Êý
 * @retval      result:mµÄn´ÎÃÝ
 */
uint32_t mypow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;
    while (n--)
    {
        result *= m;
    }
    return result;
}

/**
 * @brief       ÏÔÊ¾Êý×Ö
 * @param       x:Êý×ÖÏÔÊ¾Î»ÖÃÁÐÆðÊ¼×ø±ê
 * @param       y:Êý×ÖÏÔÊ¾Î»ÖÃÐÐÆðÊ¼×ø±ê
 * @param       num:ÏÔÊ¾µÄÊý×Ö(0~4294967295)
 * @param       len:ÏÔÊ¾Êý×ÖµÄÎ»Êý
 * @param       fc:×Ö·ûÑÕÉ«
 * @param       bc:×Ö·û±³¾°ÑÕÉ«
 * @param       sizey:×Ö·û´óÐ¡
 * @param       mode:ÊÇ·ñµþ¼ÓÏÔÊ¾
 * @retval      ÎÞ
 */
void LCD_ShowNum(uint16_t x, uint16_t y, uint32_t num, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
    uint8_t t, temp, enshow = 0;
    uint8_t sizex = sizey / 2;
    for (t = 0; t < len; t++)
    {
        temp = (num / mypow(10, len - t - 1)) % 10;
        if (enshow == 0 && t < (len - 1))
        {
            if (temp == 0)
            {
                LCD_ShowChar(x + t * sizex, y, ' ', fc, bc, sizey, mode);
                continue;
            }
            else
            {
                enshow = 1;
            }
        }
        LCD_ShowChar(x + t * sizex, y, temp + '0', fc, bc, sizey, mode);
    }
}

/**
 * @brief       ÏÔÊ¾¸¡µãÊý
 * @param       x:Êý×ÖÏÔÊ¾Î»ÖÃÁÐÆðÊ¼×ø±ê
 * @param       y:Êý×ÖÏÔÊ¾Î»ÖÃÐÐÆðÊ¼×ø±ê
 * @param       num:ÏÔÊ¾µÄ¸¡µãÊý
 * @param       pre:ÏÔÊ¾¸¡µãÊý¾«¶È
 * @param       len:ÏÔÊ¾¸¡µãÊýµÄÎ»Êý(²»°üº¬Ð¡Êýµã)
 * @param       fc:×Ö·ûÑÕÉ«
 * @param       bc:×Ö·û±³¾°ÑÕÉ«
 * @param       sizey:×Ö·û´óÐ¡
 * @param       mode:ÊÇ·ñµþ¼ÓÏÔÊ¾
 * @retval      ÎÞ
 */
void LCD_ShowFloatNum(uint16_t x, uint16_t y, float num, uint8_t pre, uint8_t len, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
    uint32_t i, temp, num1;
    uint8_t sizex = sizey / 2;
    num1 = num * mypow(10, pre);
    for (i = 0; i < len; i++)
    {
        temp = (num1 / mypow(10, len - i - 1)) % 10;
        if (i == (len - pre))
        {
            LCD_ShowChar(x + (len - pre) * sizex, y, '.', fc, bc, sizey, mode);
            i++;
            len += 1;
        }
        LCD_ShowChar(x + i * sizex, y, temp + '0', fc, bc, sizey, mode);
    }
}

/**
 * @brief       ÏÔÊ¾12x12ºº×Ö
 * @param       x:ºº×ÖÏÔÊ¾Î»ÖÃÁÐÆðÊ¼×ø±ê
 * @param       y:ºº×ÖÏÔÊ¾Î»ÖÃÐÐÆðÊ¼×ø±ê
 * @param       *s:ÏÔÊ¾ÖÐÎÄ×Ö·ûÆðÊ¼µØÖ·
 * @param       fc:×Ö·ûÑÕÉ«
 * @param       bc:×Ö·û±³¾°ÑÕÉ«
 * @param       sizey:×Ö·û´óÐ¡
 * @param       mode:ÊÇ·ñµþ¼ÓÏÔÊ¾
 * @retval      ÎÞ
 */
void LCD_ShowChinese12x12(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
    uint8_t i, j, m = 0;
    uint16_t k, HZnum;    // ºº×ÖÊýÄ¿
    uint16_t TypefaceNum; // Ò»¸ö×Ö·ûËùÕ¼×Ö½Ú´óÐ¡
    uint16_t x0 = x;
    TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
    HZnum = sizeof(tfont12) / sizeof(typFONT_GB12); // Í³¼Æºº×ÖÊýÄ¿
    for (k = 0; k < HZnum; k++)
    {
        if ((tfont12[k].Index[0] == *(s)) && (tfont12[k].Index[1] == *(s + 1)))
        {
            LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
            for (i = 0; i < TypefaceNum; i++)
            {
                for (j = 0; j < 8; j++)
                {
                    if (!mode) // ·Çµþ¼Ó·½Ê½
                    {
                        if (tfont12[k].Msk[i] & (0x01 << j))
                        {
                            LCD_WR_DATA(fc);
                        }
                        else
                        {
                            LCD_WR_DATA(bc);
                        }
                        m++;
                        if (m % sizey == 0)
                        {
                            m = 0;
                            break;
                        }
                    }
                    else // µþ¼Ó·½Ê½
                    {
                        if (tfont12[k].Msk[i] & (0x01 << j))
                        {
                            LCD_DrawPoint(x, y, fc); // »­Ò»¸öµã
                        }
                        x++;
                        if ((x - x0) == sizey)
                        {
                            x = x0;
                            y++;
                            break;
                        }
                    }
                }
            }
        }
        continue; // ²éÕÒµ½¶ÔÓ¦µãÕó×Ö¿âÁ¢¼´ÍË³ö£¬·ÀÖ¹¶à¸öºº×ÖÖØ¸´È¡Ä£´øÀ´Ó°Ïì
    }
}

/**
 * @brief       ÏÔÊ¾16x16ºº×Ö
 * @param       x:ºº×ÖÏÔÊ¾Î»ÖÃÁÐÆðÊ¼×ø±ê
 * @param       y:ºº×ÖÏÔÊ¾Î»ÖÃÐÐÆðÊ¼×ø±ê
 * @param       *s:ÏÔÊ¾ÖÐÎÄ×Ö·ûÆðÊ¼µØÖ·
 * @param       fc:×Ö·ûÑÕÉ«
 * @param       bc:×Ö·û±³¾°ÑÕÉ«
 * @param       sizey:×Ö·û´óÐ¡
 * @param       mode:ÊÇ·ñµþ¼ÓÏÔÊ¾
 * @retval      ÎÞ
 */
void LCD_ShowChinese16x16(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
    uint8_t i, j, m = 0;
    uint16_t k, HZnum;    // ºº×ÖÊýÄ¿
    uint16_t TypefaceNum; // Ò»¸ö×Ö·ûËùÕ¼×Ö½Ú´óÐ¡
    uint16_t x0 = x;
    TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
    HZnum = sizeof(tfont16) / sizeof(typFONT_GB16); // Í³¼Æºº×ÖÊýÄ¿
    for (k = 0; k < HZnum; k++)
    {
        if ((tfont16[k].Index[0] == *(s)) && (tfont16[k].Index[1] == *(s + 1)))
        {
            LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
            for (i = 0; i < TypefaceNum; i++)
            {
                for (j = 0; j < 8; j++)
                {
                    if (!mode) // ·Çµþ¼Ó·½Ê½
                    {
                        if (tfont16[k].Msk[i] & (0x01 << j))
                        {
                            LCD_WR_DATA(fc);
                        }
                        else
                        {
                            LCD_WR_DATA(bc);
                        }
                        m++;
                        if (m % sizey == 0)
                        {
                            m = 0;
                            break;
                        }
                    }
                    else // µþ¼Ó·½Ê½
                    {
                        if (tfont16[k].Msk[i] & (0x01 << j))
                        {
                            LCD_DrawPoint(x, y, fc); // »­Ò»¸öµã
                        }
                        x++;
                        if ((x - x0) == sizey)
                        {
                            x = x0;
                            y++;
                            break;
                        }
                    }
                }
            }
        }
        continue; // ²éÕÒµ½¶ÔÓ¦µãÕó×Ö¿âÁ¢¼´ÍË³ö£¬·ÀÖ¹¶à¸öºº×ÖÖØ¸´È¡Ä£´øÀ´Ó°Ïì
    }
}

/**
 * @brief       ÏÔÊ¾24x24ºº×Ö
 * @param       x:ºº×ÖÏÔÊ¾Î»ÖÃÁÐÆðÊ¼×ø±ê
 * @param       y:ºº×ÖÏÔÊ¾Î»ÖÃÐÐÆðÊ¼×ø±ê
 * @param       *s:ÏÔÊ¾ÖÐÎÄ×Ö·ûÆðÊ¼µØÖ·
 * @param       fc:×Ö·ûÑÕÉ«
 * @param       bc:×Ö·û±³¾°ÑÕÉ«
 * @param       sizey:×Ö·û´óÐ¡
 * @param       mode:ÊÇ·ñµþ¼ÓÏÔÊ¾
 * @retval      ÎÞ
 */
void LCD_ShowChinese24x24(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
    uint8_t i, j, m = 0;
    uint16_t k, HZnum;    // ºº×ÖÊýÄ¿
    uint16_t TypefaceNum; // Ò»¸ö×Ö·ûËùÕ¼×Ö½Ú´óÐ¡
    uint16_t x0 = x;
    TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
    HZnum = sizeof(tfont24) / sizeof(typFONT_GB24); // Í³¼Æºº×ÖÊýÄ¿
    for (k = 0; k < HZnum; k++)
    {
        if ((tfont24[k].Index[0] == *(s)) && (tfont24[k].Index[1] == *(s + 1)))
        {
            LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
            for (i = 0; i < TypefaceNum; i++)
            {
                for (j = 0; j < 8; j++)
                {
                    if (!mode) // ·Çµþ¼Ó·½Ê½
                    {
                        if (tfont24[k].Msk[i] & (0x01 << j))
                        {
                            LCD_WR_DATA(fc);
                        }
                        else
                        {
                            LCD_WR_DATA(bc);
                        }
                        m++;
                        if (m % sizey == 0)
                        {
                            m = 0;
                            break;
                        }
                    }
                    else // µþ¼Ó·½Ê½
                    {
                        if (tfont24[k].Msk[i] & (0x01 << j))
                        {
                            LCD_DrawPoint(x, y, fc); // »­Ò»¸öµã
                        }
                        x++;
                        if ((x - x0) == sizey)
                        {
                            x = x0;
                            y++;
                            break;
                        }
                    }
                }
            }
        }
        continue; // ²éÕÒµ½¶ÔÓ¦µãÕó×Ö¿âÁ¢¼´ÍË³ö£¬·ÀÖ¹¶à¸öºº×ÖÖØ¸´È¡Ä£´øÀ´Ó°Ïì
    }
}

/**
 * @brief       ÏÔÊ¾32x32ºº×Ö
 * @param       x:ºº×ÖÏÔÊ¾Î»ÖÃÁÐÆðÊ¼×ø±ê
 * @param       y:ºº×ÖÏÔÊ¾Î»ÖÃÐÐÆðÊ¼×ø±ê
 * @param       *s:ÏÔÊ¾ÖÐÎÄ×Ö·ûÆðÊ¼µØÖ·
 * @param       fc:×Ö·ûÑÕÉ«
 * @param       bc:×Ö·û±³¾°ÑÕÉ«
 * @param       sizey:×Ö·û´óÐ¡
 * @param       mode:ÊÇ·ñµþ¼ÓÏÔÊ¾
 * @retval      ÎÞ
 */
void LCD_ShowChinese32x32(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
    uint8_t i, j, m = 0;
    uint16_t k, HZnum;    // ºº×ÖÊýÄ¿
    uint16_t TypefaceNum; // Ò»¸ö×Ö·ûËùÕ¼×Ö½Ú´óÐ¡
    uint16_t x0 = x;
    TypefaceNum = (sizey / 8 + ((sizey % 8) ? 1 : 0)) * sizey;
    HZnum = sizeof(tfont32) / sizeof(typFONT_GB32); // Í³¼Æºº×ÖÊýÄ¿
    for (k = 0; k < HZnum; k++)
    {
        if ((tfont32[k].Index[0] == *(s)) && (tfont32[k].Index[1] == *(s + 1)))
        {
            LCD_Address_Set(x, y, x + sizey - 1, y + sizey - 1);
            for (i = 0; i < TypefaceNum; i++)
            {
                for (j = 0; j < 8; j++)
                {
                    if (!mode) // ·Çµþ¼Ó·½Ê½
                    {
                        if (tfont32[k].Msk[i] & (0x01 << j))
                        {
                            LCD_WR_DATA(fc);
                        }
                        else
                        {
                            LCD_WR_DATA(bc);
                        }
                        m++;
                        if (m % sizey == 0)
                        {
                            m = 0;
                            break;
                        }
                    }
                    else // µþ¼Ó·½Ê½
                    {
                        if (tfont32[k].Msk[i] & (0x01 << j))
                        {
                            LCD_DrawPoint(x, y, fc); // »­Ò»¸öµã
                        }
                        x++;
                        if ((x - x0) == sizey)
                        {
                            x = x0;
                            y++;
                            break;
                        }
                    }
                }
            }
        }
        continue; // ²éÕÒµ½¶ÔÓ¦µãÕó×Ö¿âÁ¢¼´ÍË³ö£¬·ÀÖ¹¶à¸öºº×ÖÖØ¸´È¡Ä£´øÀ´Ó°Ïì
    }
}

/**
 * @brief       ÏÔÊ¾ºº×Ö´®
 * @param       x:ºº×ÖÏÔÊ¾Î»ÖÃÁÐÆðÊ¼×ø±ê
 * @param       y:ºº×ÖÏÔÊ¾Î»ÖÃÐÐÆðÊ¼×ø±ê
 * @param       *s:ÏÔÊ¾ÖÐÎÄ×Ö·û
 * @param       fc:×Ö·ûÑÕÉ«
 * @param       bc:×Ö·û±³¾°ÑÕÉ«
 * @param       sizey:×Ö·û´óÐ¡
 * @param       mode:ÊÇ·ñµþ¼ÓÏÔÊ¾
 * @retval      ÎÞ
 */
void LCD_ShowChinese(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
    while (*s != 0)
    {
        if (sizey == 12)
            LCD_ShowChinese12x12(x, y, s, fc, bc, sizey, mode);
        else if (sizey == 16)
            LCD_ShowChinese16x16(x, y, s, fc, bc, sizey, mode);
        else if (sizey == 24)
            LCD_ShowChinese24x24(x, y, s, fc, bc, sizey, mode);
        else if (sizey == 32)
            LCD_ShowChinese32x32(x, y, s, fc, bc, sizey, mode);
        else
            return;
        s += 2;
        x += sizey;
    }
}

/**
 * @brief       ÖÐÓ¢×Ö·û»ìÏÔ
 * @param       x:ÏÔÊ¾Î»ÖÃÁÐÆðÊ¼×ø±ê
 * @param       y:ÏÔÊ¾Î»ÖÃÐÐÆðÊ¼×ø±ê
 * @param       *s:ÏÔÊ¾×Ö·ûÆðÊ¼µØÖ·
 * @param       fc:×Ö·ûÑÕÉ«
 * @param       bc:×Ö·û±³¾°ÑÕÉ«
 * @param       sizey:×Ö·û´óÐ¡
 * @param       mode:ÊÇ·ñµþ¼ÓÏÔÊ¾
 * @retval      ÎÞ
 */
void LCD_ShowStr(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
    uint16_t x0 = x;
    uint8_t bHz = 0; // ×Ö·û»òÕßÖÐÎÄ
    while (*s != 0)  // Êý¾ÝÎ´½áÊø
    {
        if (!bHz) // Ó¢ÎÄ
        {
            if (x > (LCD_W - sizey / 2) || y > (LCD_H - sizey))
            {
                return;
            }
            if (*s > 0x80)
            {
                bHz = 1; // ÖÐÎÄ
            }
            else // ×Ö·û
            {
                if (*s == 0x0D) // »»ÐÐ·ûºÅ
                {
                    y += sizey;
                    x = x0;
                    s++;
                }
                else
                {
                    LCD_ShowChar(x, y, *s, fc, bc, sizey, mode);
                    x += sizey / 2; // ×Ö·û,ÎªÈ«×ÖµÄÒ»°ë
                }
                s++;
            }
        }
        else // ÖÐÎÄ
        {
            if (x > (LCD_W - sizey) || y > (LCD_H - sizey))
            {
                return;
            }
            bHz = 0;
            if (sizey == 12)
                LCD_ShowChinese12x12(x, y, s, fc, bc, sizey, mode);
            else if (sizey == 16)
                LCD_ShowChinese16x16(x, y, s, fc, bc, sizey, mode);
            else if (sizey == 24)
                LCD_ShowChinese24x24(x, y, s, fc, bc, sizey, mode);
            else
                LCD_ShowChinese32x32(x, y, s, fc, bc, sizey, mode);
            s += 2;
            x += sizey;
        }
    }
}

/**
 * @brief       ×Ö·û¾ÓÖÐÏÔÊ¾
 * @param       x:´ËÊäÈë²ÎÊýÎÞÐ§
 * @param       y:ÏÔÊ¾Î»ÖÃÐÐÆðÊ¼×ø±ê
 * @param       *s:ÏÔÊ¾×Ö·ûÆðÊ¼µØÖ·
 * @param       fc:×Ö·ûÑÕÉ«
 * @param       bc:×Ö·û±³¾°ÑÕÉ«
 * @param       sizey:×Ö·û´óÐ¡
 * @param       mode:ÊÇ·ñµþ¼ÓÏÔÊ¾
 * @retval      ÎÞ
 */
void LCD_StrCenter(uint16_t x, uint16_t y,const char *s, uint16_t fc, uint16_t bc, uint8_t sizey, uint8_t mode)
{
    uint16_t len = strlen((const char *)s);
    uint16_t x1 = (LCD_W - len * (sizey/2)) / 2;
    LCD_ShowStr(x1, y, s, fc, bc, sizey, mode);
}

/**
 * @brief       Í¼Æ¬ÏÔÊ¾º¯Êý
 * @param       x:Í¼Æ¬ÏÔÊ¾Î»ÖÃÁÐÆðÊ¼×ø±ê
 * @param       y:Í¼Æ¬ÏÔÊ¾Î»ÖÃÐÐÆðÊ¼×ø±ê
 * @param       width:Í¼Æ¬¿í¶È
 * @param       height:Í¼Æ¬¸ß¶È
 * @param       pic:Í¼Æ¬È¡Ä£Êý×é
 * @retval      ÎÞ
 */
// void LCD_ShowPicture(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t pic[])
// {
//     uint8_t t = 1;
//     uint32_t num = width * height * 2, num1;
//     LCD_Address_Set(x, y, x + width - 1, y + height - 1);
//     LCD_CS_Clr();
//     while (t)
//     {
//         if (num > 65534)
//         {
//             num -= 65534;
//             num1 = 65534;
//         }
//         else
//         {
//             t = 0;
//             num1 = num;
//         }
//         MYDMA_Config(DMA1_Channel3);
//         MYDMA_Enable((uint32_t)pic, (uint32_t)&SPI1->DR, num1);
//         while (1)
//         {
//             if (__HAL_DMA_GET_FLAG(&DMA_InitStructure, DMA_FLAG_TC3)) // µÈ´ýÍ¨µÀ4´«ÊäÍê³É
//             {
//                 __HAL_DMA_CLEAR_FLAG(&DMA_InitStructure, DMA_FLAG_TC3); // Çå³ýÍ¨µÀ3´«ÊäÍê³É±êÖ¾
//                 HAL_SPI_DMAStop(&SPI_InitStructure);
//                 break;
//             }
//         }
//         pic += 65534;
//     }
//     while(__HAL_SPI_GET_FLAG(&SPI_InitStructure,SPI_FLAG_BSY)==SET);
//     LCD_CS_Set();
// }
