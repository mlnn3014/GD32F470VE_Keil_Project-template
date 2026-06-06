#include "driver_rs485.h"
#include "mcu_cimc_gd32f470vet6.h"

static uint32_t s_rs485_baudrate = 19200U;

void driver_rs485_init(uint32_t baudrate)
{
    s_rs485_baudrate = baudrate;
    bsp_usart_init();
}

void driver_rs485_set_baudrate(uint32_t baudrate)
{
    s_rs485_baudrate = baudrate;

    usart_disable(RS232_RS485_USART);
    usart_baudrate_set(RS232_RS485_USART, baudrate);
    usart_enable(RS232_RS485_USART);
}

void driver_rs485_send_bytes(const uint8_t *data, size_t len)
{
    size_t i;

    if((data == NULL) || (len == 0U)) {
        return;
    }

    RS485_CS_SET(1);
    for(i = 0U; i < len; i++) {
        usart_data_transmit(RS232_RS485_USART, data[i]);
        while(RESET == usart_flag_get(RS232_RS485_USART, USART_FLAG_TBE)) {
        }
    }
    while(RESET == usart_flag_get(RS232_RS485_USART, USART_FLAG_TC)) {
    }
    RS485_CS_SET(0);
}

uint32_t driver_rs485_get_baudrate(void)
{
    return s_rs485_baudrate;
}
