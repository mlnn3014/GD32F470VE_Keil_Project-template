#ifndef RS485_APP_H
#define RS485_APP_H

#ifdef __cplusplus
extern "C"
{
#endif

int rs485_printf(const char *format, ...); // RS485 格式化发送
void rs485_task(void);                     // 处理 RS485 接收命令

#ifdef __cplusplus
}
#endif

#endif
