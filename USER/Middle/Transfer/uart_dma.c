#include "uart_dma.h"
#include <string.h>

//  uint32_t number = 0;
//  uint32_t numberB = 0;

static void uart_dma_clear_rx_status(UART_HandleTypeDef *huart)
{
    __HAL_UART_DISABLE_IT(huart, UART_IT_IDLE);
    __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_PEF | UART_CLEAR_FEF |
                                  UART_CLEAR_NEF | UART_CLEAR_OREF |
                                  UART_CLEAR_IDLEF);
    __HAL_UART_SEND_REQ(huart, UART_RXDATA_FLUSH_REQUEST);
}

static HAL_StatusTypeDef uart_dma_start_rx(uart_dma_t *ctrl)
{
    HAL_StatusTypeDef status;

    uart_dma_clear_rx_status(ctrl->huart);
    status = HAL_UART_Receive_DMA(ctrl->huart, ctrl->dma_rx_buf, ctrl->dma_rx_size);
    if (status != HAL_OK)
    {
        return status;
    }
    __HAL_UART_CLEAR_IDLEFLAG(ctrl->huart);
    __HAL_UART_ENABLE_IT(ctrl->huart, UART_IT_IDLE);
    return HAL_OK;
}

void uart_dma_init(uart_dma_t* ctrl, UART_HandleTypeDef* huart, uint8_t* dma_buf, uint32_t dma_size, uint8_t* rb_buf, uint32_t rb_size)
{
    ctrl->huart = huart;
    ctrl->dma_rx_buf = dma_buf;
    ctrl->dma_rx_size = dma_size;
    ctrl->lwrb_buf = rb_buf;
    ctrl->lwrb_size = rb_size;
    ctrl->old_pos = 0;
/* (可选) 清空 DMA 数组，方便 debug 时查看数据干净 */
    memset(dma_buf, 0, dma_size);
    /* 初始化环形缓冲区 */
    lwrb_init(&ctrl->uart_rb, ctrl->lwrb_buf, ctrl->lwrb_size);

    (void)uart_dma_start_rx(ctrl);
}

HAL_StatusTypeDef uart_dma_restart_rx(uart_dma_t* ctrl)
{
    if ((ctrl == NULL) || (ctrl->huart == NULL) ||
        (ctrl->huart->hdmarx == NULL) || (ctrl->dma_rx_buf == NULL) ||
        (ctrl->dma_rx_size == 0U))
    {
        return HAL_ERROR;
    }

    __HAL_UART_DISABLE_IT(ctrl->huart, UART_IT_IDLE);
    (void)HAL_UART_DMAStop(ctrl->huart);
    ctrl->old_pos = 0U;
    memset(ctrl->dma_rx_buf, 0, ctrl->dma_rx_size);
    lwrb_reset(&ctrl->uart_rb);
    return uart_dma_start_rx(ctrl);
}

void uart_dma_rx_check(uart_dma_t* ctrl)
{
    uint32_t pos;
    /* 获取当前 DMA 写入位置 */
    pos = ctrl->dma_rx_size - __HAL_DMA_GET_COUNTER(ctrl->huart->hdmarx);

    if (pos != ctrl->old_pos)
    {
        if (pos > ctrl->old_pos)
        {
            lwrb_write(&ctrl->uart_rb, &ctrl->dma_rx_buf[ctrl->old_pos], pos - ctrl->old_pos);
        }
        else
        {
            lwrb_write(&ctrl->uart_rb, &ctrl->dma_rx_buf[ctrl->old_pos], ctrl->dma_rx_size - ctrl->old_pos);
            lwrb_write(&ctrl->uart_rb, &ctrl->dma_rx_buf[0], pos);
        }
        ctrl->old_pos = pos;
    }
}

int uart_dma_read(uart_dma_t* ctrl, uint8_t *data, uint32_t len, uint32_t timeout)
{
    uint32_t tick = HAL_GetTick();
    uint32_t rec = 0;

    while (rec < len)
    {
        rec += lwrb_read(&ctrl->uart_rb, &data[rec], len - rec);

        if ((HAL_GetTick() - tick) > timeout) break;
    }
    return rec;
}

int uart_dma_write(uart_dma_t* ctrl, const uint8_t *data, uint32_t len, uint32_t timeout)
{
    if (HAL_UART_Transmit(ctrl->huart, (uint8_t *)data, len, timeout) == HAL_OK)
    {
        return len;
    }
    return 0;
}
