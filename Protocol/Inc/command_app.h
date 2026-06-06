#ifndef COMMAND_APP_H
#define COMMAND_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void rs485_command_parse(const char *line); // RS485 协议入口
void command_app_task(void);                // 协议里要周期跑的东西
void command_app_send_heartbeat(void);      // 主动报到一次
uint8_t command_app_sync_boot_param(void);  // 同步 BootLoader 通信参数

#ifdef __cplusplus
}
#endif

#endif