#ifndef COMMAND_APP_H
#define COMMAND_APP_H

#ifdef __cplusplus
extern "C"
{
#endif

void uart0_command_parse(const char *line);
void rs485_command_parse(const char *line);

#ifdef __cplusplus
}
#endif

#endif
