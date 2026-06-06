#ifndef RS485_APP_H
#define RS485_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

void rs485_app_init(void);                  // 初始化 RS485 app
int rs485_printf(const char *format, ...);  // RS485 格式化发送
void rs485_task(void);                      // 处理 RS485 接收命令

#ifdef __cplusplus
}
#endif

#endif
