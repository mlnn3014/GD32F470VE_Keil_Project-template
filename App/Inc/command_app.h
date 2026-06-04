#ifndef COMMAND_APP_H
#define COMMAND_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void uart0_command_parse(const char *line);
void rs485_command_parse(const char *line);
void cmd_rx(const uint8_t *buf, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif
