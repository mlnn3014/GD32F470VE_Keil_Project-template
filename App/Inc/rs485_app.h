#ifndef RS485_APP_H
#define RS485_APP_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*rs485_line_handler_t)(const char *line);

int rs485_printf(const char *format, ...);
void rs485_app_init(void);
void rs485_on_line(rs485_line_handler_t handler);
void rs485_task(void);

#ifdef __cplusplus
}
#endif

#endif /* RS485_APP_H */
