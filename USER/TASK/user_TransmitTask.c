#include "user_TransmitTask.h"

#include "FreeRTOS.h"
#include "log.h"
#include "task.h"
#include "user_TasksInit.h"
#include "weather_app.h"

static ProtocolState_t s_protocol_state = STATE_IDLE;
static uint8_t s_protocol_curr_cmd = 0U;
static uint16_t s_protocol_data_len = 0U;
static uint16_t s_protocol_data_idx = 0U;
static uint8_t s_protocol_payload[256];
static uint8_t s_protocol_check_buf[260];
static uint16_t s_protocol_check_ptr = 0U;
static volatile uint32_t s_uart2_error_code = HAL_UART_ERROR_NONE;

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    if ((huart == NULL) || (huart->Instance != USART2))
    {
        return;
    }
    s_uart2_error_code |= huart->ErrorCode;
    if (xTransmitTaskWakeSemaphore != NULL)
    {
        (void)xSemaphoreGiveFromISR(xTransmitTaskWakeSemaphore, &higher_priority_task_woken);
    }
    portYIELD_FROM_ISR(higher_priority_task_woken);
}

void TransmitTask_ResetProtocolState(void)
{
    s_protocol_state = STATE_IDLE;
    s_protocol_curr_cmd = 0U;
    s_protocol_data_len = 0U;
    s_protocol_data_idx = 0U;
    s_protocol_check_ptr = 0U;
}

void TransmitTask(void *argument)
{
    uint8_t ch;

    (void)argument;

    User_Tasks_WaitForHardwareReady();

    for (;;)
    {
        HAL_StatusTypeDef recover_status;
        uint32_t error_code;

        (void)xSemaphoreTake(xTransmitTaskWakeSemaphore, portMAX_DELAY);

        taskENTER_CRITICAL();
        error_code = s_uart2_error_code;
        s_uart2_error_code = HAL_UART_ERROR_NONE;
        taskEXIT_CRITICAL();

        if (error_code != HAL_UART_ERROR_NONE)
        {
            vTaskDelay(pdMS_TO_TICKS(5U));
            recover_status = uart_dma_restart_rx(&uart2_admin);
            TransmitTask_ResetProtocolState();
            if (recover_status != HAL_OK)
            {
                log_printf("[Weather] uart rx recover fail=%u",
                           (unsigned int)recover_status);
            }
            continue;
        }

        while (uart_dma_read(&uart2_admin, &ch, 1U, 0U) > 0)
        {
            switch (s_protocol_state)
            {
            case STATE_IDLE:
                if (ch == FRAME_HEAD)
                {
                    s_protocol_state = STATE_CMD;
                    s_protocol_check_ptr = 0U;
                }
                break;

            case STATE_CMD:
                s_protocol_curr_cmd = ch;
                s_protocol_check_buf[s_protocol_check_ptr++] = ch;
                s_protocol_state = STATE_LEN_H;
                break;

            case STATE_LEN_H:
                s_protocol_data_len = (uint16_t)ch << 8;
                s_protocol_check_buf[s_protocol_check_ptr++] = ch;
                s_protocol_state = STATE_LEN_L;
                break;

            case STATE_LEN_L:
                s_protocol_data_len |= ch;
                s_protocol_check_buf[s_protocol_check_ptr++] = ch;
                if (s_protocol_data_len > 250U)
                {
                    TransmitTask_ResetProtocolState();
                }
                else if (s_protocol_data_len == 0U)
                {
                    s_protocol_state = STATE_CRC;
                }
                else
                {
                    s_protocol_data_idx = 0U;
                    s_protocol_state = STATE_DATA;
                }
                break;

            case STATE_DATA:
                s_protocol_payload[s_protocol_data_idx++] = ch;
                s_protocol_check_buf[s_protocol_check_ptr++] = ch;
                if (s_protocol_data_idx >= s_protocol_data_len)
                {
                    s_protocol_state = STATE_CRC;
                }
                break;

            case STATE_CRC:
                if (ch == stm32_calc_crc8(s_protocol_check_buf, s_protocol_check_ptr))
                {
                    s_protocol_state = STATE_TAIL;
                }
                else
                {
                    log_printf("[Protocol] crc err");
                    TransmitTask_ResetProtocolState();
                }
                break;

            case STATE_TAIL:
                if (ch == FRAME_TAIL)
                {
                    s_protocol_payload[s_protocol_data_len] = '\0';
                    process_protocol_data(s_protocol_curr_cmd, (char *)s_protocol_payload);
                }
                else
                {
                    log_printf("[Protocol] tail err");
                }
                TransmitTask_ResetProtocolState();
                break;

            default:
                TransmitTask_ResetProtocolState();
                break;
            }
        }
    }
}
