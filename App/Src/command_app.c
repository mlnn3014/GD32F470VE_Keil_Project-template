#include "command_app.h"

#include "rs485_app.h"
#include "uart0_app.h"

// UART0 命令入口, 现在先预留
void uart0_command_parse(const char *line)
{
    (void)line;
}

// RS485 命令入口, 现在先预留
void rs485_command_parse(const char *line)
{
    (void)line;
}
