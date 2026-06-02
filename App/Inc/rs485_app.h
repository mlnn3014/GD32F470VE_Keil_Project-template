#ifndef RS485_APP_H
#define RS485_APP_H

#ifdef __cplusplus
extern "C"
{
#endif

int rs485_printf(const char *format, ...);
void rs485_task(void);

#ifdef __cplusplus
}
#endif

#endif
