#ifndef COMMAND_APP_H
#define COMMAND_APP_H

#ifdef __cplusplus
extern "C"
{
#endif

void uart0_command_parse(const char *line); // 解析 UART0 命令行
void rs485_command_parse(const char *line); // 解析 RS485 命令行

void command_app_task(void);                // 协议周期任务

#ifdef __cplusplus
}
#endif

#endif
