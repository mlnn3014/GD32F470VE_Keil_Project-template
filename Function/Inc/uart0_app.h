#ifndef UART0_APP_H
#define UART0_APP_H

#include <stdint.h>
#include "uart0_bsp.h"

#ifdef __cplusplus
extern "C"
{
#endif

int uart0_printf(const char *format, ...); // UART0 µ÷ÊÔ´òÓ¡

#ifdef __cplusplus
}
#endif

#endif
