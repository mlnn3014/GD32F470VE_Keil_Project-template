#ifndef __USART_APP_H__
#define __USART_APP_H__

#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

int my_printf(uint32_t usart_periph, const char *format, ...);
void uart_task(void);
void ota_reset_state(void);
void debug_uart_frame_callback(const uint8_t *data, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
