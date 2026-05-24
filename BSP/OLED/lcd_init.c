#include "lcd_init.h"
// #include "dma.h"
#include "stdio.h"

extern SPI_HandleTypeDef hspi1;
// extern DMA_HandleTypeDef DMA_InitStructure;
void LCD_WR_Bus(uint8_t dat)
{
    LCD_CS_Clr();
    HAL_SPI_Transmit(&hspi1,&dat,1,0xFF);
    LCD_CS_Set();
}

/**
 * @brief       ÏòÒº¾§Ð´¼Ä´æÆ÷ÃüÁî
 * @param       reg: ÒªÐ´µÄÃüÁî
 * @retval      ÎÞ
 */
void LCD_WR_REG(uint8_t reg)
{
    LCD_DC_Clr();
    LCD_WR_Bus(reg);
    LCD_DC_Set();
}

/**
 * @brief       ÏòÒº¾§Ð´Ò»¸ö×Ö½ÚÊý¾Ý
 * @param       dat: ÒªÐ´µÄÊý¾Ý
 * @retval      ÎÞ
 */
void LCD_WR_DATA8(uint8_t dat)
{
    LCD_DC_Set();
    LCD_WR_Bus(dat);
    LCD_DC_Set();
}

/**
 * @brief       ÏòÒº¾§Ð´Ò»¸ö°ë×ÖÊý¾Ý
 * @param       dat: ÒªÐ´µÄÊý¾Ý
 * @retval      ÎÞ
 */
void LCD_WR_DATA(uint16_t dat)
{
    LCD_DC_Set();
    LCD_WR_Bus(dat >> 8);
    LCD_WR_Bus(dat & 0xFF);
    LCD_DC_Set();
}
/**
 * @brief       IOÄ£ÄâSPI·¢ËÍÒ»¸ö×Ö½ÚÊý¾Ý
 * @param       dat: ÐèÒª·¢ËÍµÄ×Ö½ÚÊý¾Ý
 * @retval      ÎÞ
 */


/**
 * @brief       ���ù��λ��
 * @param       x:�����е�ַ
 * @param       y:�����е�ַ
 * @retval      ��
 */
void LCD_SetCursor(uint16_t x, uint16_t y)
{
    LCD_Address_Set(x, y, x, y);
}

/**
 * @brief       ������ʾ����
 * @param       xs:��������ʼ��ַ
 * @param       ys:��������ʼ��ַ
 * @param       xe:�����н�����ַ
 * @param       ye:�����н�����ַ
 * @retval      ��
 */
void LCD_Address_Set(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye)
{
    if(USE_HORIZONTAL==0)
    {
        LCD_WR_REG(0x2a); // �е�ַ����
        LCD_WR_DATA(xs+12);
        LCD_WR_DATA(xe+12);
        LCD_WR_REG(0x2b); // �е�ַ����
        LCD_WR_DATA(ys);
        LCD_WR_DATA(ye);
        LCD_WR_REG(0x2c); // ������д
    }
    else if(USE_HORIZONTAL==1)
    {
        LCD_WR_REG(0x2a); // �е�ַ����
        LCD_WR_DATA(xs+14);
        LCD_WR_DATA(xe+14);
        LCD_WR_REG(0x2b); // �е�ַ����
        LCD_WR_DATA(ys);
        LCD_WR_DATA(ye);
        LCD_WR_REG(0x2c); // ������д
    }
    else if(USE_HORIZONTAL==2)
    {
        LCD_WR_REG(0x2a); // �е�ַ����
        LCD_WR_DATA(xs);
        LCD_WR_DATA(xe);
        LCD_WR_REG(0x2b); // �е�ַ����
        LCD_WR_DATA(ys+14);
        LCD_WR_DATA(ye+14);
        LCD_WR_REG(0x2c); // ������д
    }
    else
    {
        LCD_WR_REG(0x2a); // �е�ַ����
        LCD_WR_DATA(xs);
        LCD_WR_DATA(xe);
        LCD_WR_REG(0x2b); // �е�ַ����
        LCD_WR_DATA(ys+12);
        LCD_WR_DATA(ye+12);
        LCD_WR_REG(0x2c); // ������д
    }
}

/**
 * @brief       ָ����ɫ�������
 * @param       xs:�����������ʼ��ַ
 * @param       ys:�����������ʼ��ַ
 * @param       xe:��������н�����ַ
 * @param       ye:��������н�����ַ
 * @param       color:�����ɫֵ
 * @retval      ��
 */

void  MY_LCD_Fill(uint16_t sx, uint16_t sy, uint16_t ex, uint16_t ey, uint16_t *color)
{
    
    LCD_Fill(sx, sy, ex, ey, *color); 
    
}


void LCD_Fill(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color)
{
    uint32_t i;
    uint32_t num = (uint32_t)(xe - xs) * (ye - ys);
    
    LCD_Address_Set(xs, ys, xe - 1, ye - 1);
    
    // 设置 DC 为数据模式
    LCD_DC_Set();
    LCD_CS_Clr();
    
    // 循环发送，直接调用 HAL_SPI_Transmit
    // 注意：如果是 16 位颜色，这里手动拆分为两个 8 位发送，效率更高
    uint8_t color_high = color >> 8;
    uint8_t color_low = color & 0xFF;
    
    for(i = 0; i < num; i++)
    {
        HAL_SPI_Transmit(&hspi1, &color_high, 1, 0xFF);
        HAL_SPI_Transmit(&hspi1, &color_low, 1, 0xFF);
    }
    
    LCD_CS_Set();
}

/**
 * @brief       ��ʼ��LCD
 * @param       ��
 * @retval      ��
 */
void LCD_Init(void)
{

    LCD_RES_Set();
    HAL_Delay(50);
    LCD_RES_Clr();
    HAL_Delay(50);
    LCD_RES_Set();
    HAL_Delay(120);
    LCD_BLK_Set();
    LCD_WR_REG(0xff);
    LCD_WR_DATA8(0xa5);
    LCD_WR_REG(0x9a);
    LCD_WR_DATA8(0x08);
    LCD_WR_REG(0x9b);
    LCD_WR_DATA8(0x08);
    LCD_WR_REG(0x9c);
    LCD_WR_DATA8(0xb0);
    LCD_WR_REG(0x9d);
    LCD_WR_DATA8(0x16);
    LCD_WR_REG(0x9e);
    LCD_WR_DATA8(0xc4);
    LCD_WR_REG(0x8f);
    LCD_WR_DATA8(0x55);
    LCD_WR_DATA8(0x04);
    LCD_WR_REG(0x84);
    LCD_WR_DATA8(0x90);
    LCD_WR_REG(0x83);
    LCD_WR_DATA8(0x7b);
    LCD_WR_REG(0x85);
    LCD_WR_DATA8(0x33);
    LCD_WR_REG(0x60);
    LCD_WR_DATA8(0x00);
    LCD_WR_REG(0x70);
    LCD_WR_DATA8(0x00);
    LCD_WR_REG(0x61);
    LCD_WR_DATA8(0x02);
    LCD_WR_REG(0x71);
    LCD_WR_DATA8(0x02);
    LCD_WR_REG(0x62);
    LCD_WR_DATA8(0x04);
    LCD_WR_REG(0x72);
    LCD_WR_DATA8(0x04);
    LCD_WR_REG(0x6c);
    LCD_WR_DATA8(0x29);
    LCD_WR_REG(0x7c);
    LCD_WR_DATA8(0x29);
    LCD_WR_REG(0x6d);
    LCD_WR_DATA8(0x31);
    LCD_WR_REG(0x7d);
    LCD_WR_DATA8(0x31);
    LCD_WR_REG(0x6e);
    LCD_WR_DATA8(0x0f);
    LCD_WR_REG(0x7e);
    LCD_WR_DATA8(0x0f);
    LCD_WR_REG(0x66);
    LCD_WR_DATA8(0x21);
    LCD_WR_REG(0x76);
    LCD_WR_DATA8(0x21);
    LCD_WR_REG(0x68);
    LCD_WR_DATA8(0x3A);
    LCD_WR_REG(0x78);
    LCD_WR_DATA8(0x3A);
    LCD_WR_REG(0x63);
    LCD_WR_DATA8(0x07);
    LCD_WR_REG(0x73);
    LCD_WR_DATA8(0x07);
    LCD_WR_REG(0x64);
    LCD_WR_DATA8(0x05);
    LCD_WR_REG(0x74);
    LCD_WR_DATA8(0x05);
    LCD_WR_REG(0x65);
    LCD_WR_DATA8(0x02);
    LCD_WR_REG(0x75);
    LCD_WR_DATA8(0x02);
    LCD_WR_REG(0x67);
    LCD_WR_DATA8(0x23);
    LCD_WR_REG(0x77);
    LCD_WR_DATA8(0x23);
    LCD_WR_REG(0x69);
    LCD_WR_DATA8(0x08);
    LCD_WR_REG(0x79);
    LCD_WR_DATA8(0x08);
    LCD_WR_REG(0x6a);
    LCD_WR_DATA8(0x13);
    LCD_WR_REG(0x7a);
    LCD_WR_DATA8(0x13);
    LCD_WR_REG(0x6b);
    LCD_WR_DATA8(0x13);
    LCD_WR_REG(0x7b);
    LCD_WR_DATA8(0x13);
    LCD_WR_REG(0x6f);
    LCD_WR_DATA8(0x00);
    LCD_WR_REG(0x7f);
    LCD_WR_DATA8(0x00);
    LCD_WR_REG(0x50);
    LCD_WR_DATA8(0x00);
    LCD_WR_REG(0x52);
    LCD_WR_DATA8(0xd6);
    LCD_WR_REG(0x53);
    LCD_WR_DATA8(0x08);
    LCD_WR_REG(0x54);
    LCD_WR_DATA8(0x08);
    LCD_WR_REG(0x55);
    LCD_WR_DATA8(0x1e);
    LCD_WR_REG(0x56);
    LCD_WR_DATA8(0x1c);
    //goa map_sel
    LCD_WR_REG(0xa0);
    LCD_WR_DATA8(0x2b);
    LCD_WR_DATA8(0x24);
    LCD_WR_DATA8(0x00);
    LCD_WR_REG(0xa1);
    LCD_WR_DATA8(0x87);
    LCD_WR_REG(0xa2);
    LCD_WR_DATA8(0x86);
    LCD_WR_REG(0xa5);
    LCD_WR_DATA8(0x00);
    LCD_WR_REG(0xa6);
    LCD_WR_DATA8(0x00);
    LCD_WR_REG(0xa7);
    LCD_WR_DATA8(0x00);
    LCD_WR_REG(0xa8);
    LCD_WR_DATA8(0x36);
    LCD_WR_REG(0xa9);
    LCD_WR_DATA8(0x7e);
    LCD_WR_REG(0xaa);
    LCD_WR_DATA8(0x7e);
    LCD_WR_REG(0xB9);
    LCD_WR_DATA8(0x85);
    LCD_WR_REG(0xBA);
    LCD_WR_DATA8(0x84);
    LCD_WR_REG(0xBB);
    LCD_WR_DATA8(0x83);
    LCD_WR_REG(0xBC);
    LCD_WR_DATA8(0x82);
    LCD_WR_REG(0xBD);
    LCD_WR_DATA8(0x81);
    LCD_WR_REG(0xBE);
    LCD_WR_DATA8(0x80);
    LCD_WR_REG(0xBF);
    LCD_WR_DATA8(0x01);
    LCD_WR_REG(0xC0);
    LCD_WR_DATA8(0x02);
    LCD_WR_REG(0xc1);
    LCD_WR_DATA8(0x00);
    LCD_WR_REG(0xc2);
    LCD_WR_DATA8(0x00);
    LCD_WR_REG(0xc3);
    LCD_WR_DATA8(0x00);
    LCD_WR_REG(0xc4);
    LCD_WR_DATA8(0x33);
    LCD_WR_REG(0xc5);
    LCD_WR_DATA8(0x7e);
    LCD_WR_REG(0xc6);
    LCD_WR_DATA8(0x7e);
    LCD_WR_REG(0xC8);
    LCD_WR_DATA8(0x33);
    LCD_WR_DATA8(0x33);
    LCD_WR_REG(0xC9);
    LCD_WR_DATA8(0x68);
    LCD_WR_REG(0xCA);
    LCD_WR_DATA8(0x69);
    LCD_WR_REG(0xCB);
    LCD_WR_DATA8(0x6a);
    LCD_WR_REG(0xCC);
    LCD_WR_DATA8(0x6b);
    LCD_WR_REG(0xCD);
    LCD_WR_DATA8(0x33);
    LCD_WR_DATA8(0x33); 
    LCD_WR_REG(0xCE);
    LCD_WR_DATA8(0x6c);
    LCD_WR_REG(0xCF);
    LCD_WR_DATA8(0x6d);
    LCD_WR_REG(0xD0);
    LCD_WR_DATA8(0x6e);
    LCD_WR_REG(0xD1);
    LCD_WR_DATA8(0x6f);
    LCD_WR_REG(0xAB);
    LCD_WR_DATA8(0x03);
    LCD_WR_DATA8(0x67);
    LCD_WR_REG(0xAC);
    LCD_WR_DATA8(0x03);
    LCD_WR_DATA8(0x6b);
    LCD_WR_REG(0xAD);
    LCD_WR_DATA8(0x03);
    LCD_WR_DATA8(0x68);
    LCD_WR_REG(0xAE);
    LCD_WR_DATA8(0x03);
    LCD_WR_DATA8(0x6c);
    LCD_WR_REG(0xb3);
    LCD_WR_DATA8(0x00);
    LCD_WR_REG(0xb4);
    LCD_WR_DATA8(0x00);
    LCD_WR_REG(0xb5);
    LCD_WR_DATA8(0x00);
    LCD_WR_REG(0xB6);
    LCD_WR_DATA8(0x32);
    LCD_WR_REG(0xB7);
    LCD_WR_DATA8(0x7e);
    LCD_WR_REG(0xB8);
    LCD_WR_DATA8(0x7e);
    LCD_WR_REG(0xe0);
    LCD_WR_DATA8(0x00);
    LCD_WR_REG(0xe1);
    LCD_WR_DATA8(0x03);
    LCD_WR_DATA8(0x0f);
    LCD_WR_REG(0xe2);
    LCD_WR_DATA8(0x04);
    LCD_WR_REG(0xe3);
    LCD_WR_DATA8(0x01);
    LCD_WR_REG(0xe4);
    LCD_WR_DATA8(0x0e);
    LCD_WR_REG(0xe5);
    LCD_WR_DATA8(0x01);
    LCD_WR_REG(0xe6);
    LCD_WR_DATA8(0x19);
    LCD_WR_REG(0xe7);
    LCD_WR_DATA8(0x10);
    LCD_WR_REG(0xe8);
    LCD_WR_DATA8(0x10);
    LCD_WR_REG(0xea);
    LCD_WR_DATA8(0x12);
    LCD_WR_REG(0xeb);
    LCD_WR_DATA8(0xd0);
    LCD_WR_REG(0xec);
    LCD_WR_DATA8(0x04);
    LCD_WR_REG(0xed);
    LCD_WR_DATA8(0x07);
    LCD_WR_REG(0xee);
    LCD_WR_DATA8(0x07);
    LCD_WR_REG(0xef);
    LCD_WR_DATA8(0x09);
    LCD_WR_REG(0xf0);
    LCD_WR_DATA8(0xd0);
    LCD_WR_REG(0xf1);
    LCD_WR_DATA8(0x0e);

    //LCD_WR_REG(0xF9);
    //LCD_WR_DATA8(0x17); 
    //LCD_WR_REG(0xf2);
    //LCD_WR_DATA8(0x2e);
    //LCD_WR_DATA8(0x1b);
    //LCD_WR_DATA8(0x0b);
    //LCD_WR_DATA8(0x20);
    //LCD_WR_REG(0xF9);
    LCD_WR_DATA8(0x17); 
    LCD_WR_REG(0xf2);
    LCD_WR_DATA8(0x2c);
    LCD_WR_DATA8(0x1b);
    LCD_WR_DATA8(0x0b);
    LCD_WR_DATA8(0x20);
    ////1 dot
    LCD_WR_REG(0xe9);
    LCD_WR_DATA8(0x29);
    LCD_WR_REG(0xec);
    LCD_WR_DATA8(0x04);
    //TE
    LCD_WR_REG(0x35);
    LCD_WR_DATA8(0x00); 
    LCD_WR_REG(0x44);
    LCD_WR_DATA8(0x00);
    LCD_WR_DATA8(0x10); 
    LCD_WR_REG(0x46);
    LCD_WR_DATA8(0x10); 
    LCD_WR_REG(0xff);
    LCD_WR_DATA8(0x00); 
    LCD_WR_REG(0x3a);
    LCD_WR_DATA8(0x05); 
    LCD_WR_REG(0x36); /* Memory Access Control */
    if (USE_HORIZONTAL == 0)
    {
        LCD_WR_DATA8(0x00);
    }
    else if (USE_HORIZONTAL == 1)
    {
        LCD_WR_DATA8(0xC0);
    }
    else if (USE_HORIZONTAL == 2)
    {
        LCD_WR_DATA8(0x60);
    }
    else
    {
        LCD_WR_DATA8(0xA0);
    }
    LCD_WR_REG(0x11); 
    HAL_Delay(220); 
    LCD_WR_REG(0x29); 
    HAL_Delay(200); 

}

