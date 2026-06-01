#ifndef RS485_APP_H
#define RS485_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*rs485_line_handler_t)(const char *line);

/* 非阻塞格式化输出，返回实际进入 TX 队列的字节数。 */
int rs485_printf(const char *format, ...);
void rs485_app_init(void);
/* 注册按行处理回调，传入 0 恢复默认回显。 */
void rs485_on_line(rs485_line_handler_t handler);
/* 周期调用，读取缓存并派发 CR/LF 结尾的完整行。 */
void rs485_task(void);

#ifdef __cplusplus
}
#endif

#endif /* RS485_APP_H */
