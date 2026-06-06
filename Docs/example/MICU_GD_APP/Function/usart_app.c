/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2026/04/29
* Note:
*/
#include "mcu_cimc_gd32f470vet6.h"
#include "cimc_app.h"
#include "cimc_protocol_defs.h"
#include "ota_uart.h"

__IO uint16_t tx_count = 0;
__IO uint8_t rx_flag = 0;
__IO uint16_t uart_dma_len = 0;
uint8_t uart_dma_buffer[OTA_UART_RXBUF_SIZE] = {0};
__IO uint8_t debug_rx_flag = 0;
__IO uint16_t debug_uart_dma_len = 0;
uint8_t debug_uart_dma_buffer[DEBUG_UART_RXBUF_SIZE] = {0};
extern uint8_t rxbuffer[OTA_UART_RXBUF_SIZE];

/*
 * DMA writes OTA UART bytes into rxbuffer as a circular buffer. This cursor
 * tracks how far the application has already handed bytes to ota_uart.c.
 */
static uint32_t s_ota_dma_old_pos = 0U;
static char s_cimc_ascii_frame[CIMC_FRAME_MAX_ASCII_LEN];
static uint16_t s_cimc_ascii_len = 0U;

static uint8_t is_ascii_hex_char(uint8_t ch)
{
    return (((ch >= '0') && (ch <= '9')) ||
            ((ch >= 'A') && (ch <= 'F')) ||
            ((ch >= 'a') && (ch <= 'f')));
}

static uint8_t cimc_frame_has_tail(void)
{
    if(s_cimc_ascii_len < 4U) {
        return 0U;
    }

    return ((s_cimc_ascii_frame[s_cimc_ascii_len - 4U] == 'B') &&
            (s_cimc_ascii_frame[s_cimc_ascii_len - 3U] == '6') &&
            (s_cimc_ascii_frame[s_cimc_ascii_len - 2U] == 'A') &&
            (s_cimc_ascii_frame[s_cimc_ascii_len - 1U] == '5'));
}

static void cimc_uart_feed_bytes(const uint8_t *data, uint32_t len)
{
    uint32_t i;

    for(i = 0U; i < len; i++) {
        uint8_t ch = data[i];

        if(!is_ascii_hex_char(ch)) {
            s_cimc_ascii_len = 0U;
            continue;
        }

        if(s_cimc_ascii_len == 0U) {
            if(ch != 'A') {
                continue;
            }
            s_cimc_ascii_frame[s_cimc_ascii_len++] = (char)ch;
            continue;
        }

        if(s_cimc_ascii_len >= CIMC_FRAME_MAX_ASCII_LEN) {
            s_cimc_ascii_len = 0U;
            continue;
        }

        s_cimc_ascii_frame[s_cimc_ascii_len++] = (char)ch;

        if(s_cimc_ascii_len == 1U && s_cimc_ascii_frame[0] != 'A') {
            s_cimc_ascii_len = 0U;
        } else if(s_cimc_ascii_len == 2U && s_cimc_ascii_frame[1] != '5') {
            s_cimc_ascii_len = (ch == 'A') ? 1U : 0U;
            if(s_cimc_ascii_len == 1U) {
                s_cimc_ascii_frame[0] = 'A';
            }
        } else if(s_cimc_ascii_len == 3U && s_cimc_ascii_frame[2] != 'B') {
            s_cimc_ascii_len = (ch == 'A') ? 1U : 0U;
            if(s_cimc_ascii_len == 1U) {
                s_cimc_ascii_frame[0] = 'A';
            }
        } else if(s_cimc_ascii_len == 4U && s_cimc_ascii_frame[3] != '6') {
            s_cimc_ascii_len = (ch == 'A') ? 1U : 0U;
            if(s_cimc_ascii_len == 1U) {
                s_cimc_ascii_frame[0] = 'A';
            }
        } else if(cimc_frame_has_tail()) {
            cimc_app_process_ascii_frame(s_cimc_ascii_frame, s_cimc_ascii_len);
            s_cimc_ascii_len = 0U;
        } else if(s_cimc_ascii_len >= CIMC_FRAME_MAX_ASCII_LEN) {
            s_cimc_ascii_len = 0U;
        }
    }
}

void debug_uart_frame_callback(const uint8_t *data, uint16_t len)
{
    (void)data;
    my_printf(DEBUG_USART, "debug rx len=%u\r\n", len);
}

int my_printf(uint32_t usart_periph, const char *format, ...)
{
    char buffer[512];
    va_list arg;
    int len;

    va_start(arg, format);
    len = vsnprintf(buffer, sizeof(buffer), format, arg);
    va_end(arg);

    if(usart_periph == RS232_RS485_USART || usart_periph == DEBUG_USART || usart_periph == OTA_UART_PERIPH) {
        RS485_CS_SET(1);
    }

    for(tx_count = 0; tx_count < len; tx_count++) {
        usart_data_transmit(usart_periph, buffer[tx_count]);
        while(RESET == usart_flag_get(usart_periph, USART_FLAG_TBE));
    }
    while(RESET == usart_flag_get(usart_periph, USART_FLAG_TC));

    if(usart_periph == RS232_RS485_USART || usart_periph == DEBUG_USART || usart_periph == OTA_UART_PERIPH) {
        RS485_CS_SET(0);
    }

    return len;
}

void ota_reset_state(void)
{
    s_ota_dma_old_pos = 0U;
    memset(rxbuffer, 0, OTA_UART_RXBUF_SIZE);
    memset(uart_dma_buffer, 0, OTA_UART_RXBUF_SIZE);
    uart_dma_len = 0U;
    rx_flag = 0U;
    ota_uart_reset_state();
}

static void ota_uart_dma_poll(void)
{
    uint32_t pos;

    /*
     * In circular DMA mode, the current write position is buffer_size - NDTR.
     * When it wraps, feed the tail segment first and then the new head segment.
     */
    pos = OTA_UART_RXBUF_SIZE - dma_transfer_number_get(OTA_UART_DMA, OTA_UART_DMA_CH);
    if(pos == s_ota_dma_old_pos) {
        return;
    }

    if(pos > s_ota_dma_old_pos) {
        cimc_uart_feed_bytes(&rxbuffer[s_ota_dma_old_pos], pos - s_ota_dma_old_pos);
        ota_uart_process_frame(&rxbuffer[s_ota_dma_old_pos], pos - s_ota_dma_old_pos);
    } else {
        cimc_uart_feed_bytes(&rxbuffer[s_ota_dma_old_pos], OTA_UART_RXBUF_SIZE - s_ota_dma_old_pos);
        ota_uart_process_frame(&rxbuffer[s_ota_dma_old_pos], OTA_UART_RXBUF_SIZE - s_ota_dma_old_pos);
        if(pos > 0U) {
            cimc_uart_feed_bytes(rxbuffer, pos);
            ota_uart_process_frame(rxbuffer, pos);
        }
    }

    s_ota_dma_old_pos = pos;
}

void uart_task(void)
{
    if(debug_rx_flag) {
        debug_uart_frame_callback(debug_uart_dma_buffer, debug_uart_dma_len);
        memset(debug_uart_dma_buffer, 0, sizeof(debug_uart_dma_buffer));
        debug_uart_dma_len = 0;
        debug_rx_flag = 0;
    }

    /* Poll raw OTA bytes first, then let the OTA parser consume complete frames. */
    ota_uart_dma_poll();
    ota_uart_task();
}
