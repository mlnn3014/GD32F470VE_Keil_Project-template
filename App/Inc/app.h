#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C"
{
#endif

void app_init(void);     // 初始化整个应用
void app_loop(void);     // 主循环里调度任务
void app_tick_1ms(void); // 1ms tick里跑的轻量处理

#ifdef __cplusplus
}
#endif

#endif
