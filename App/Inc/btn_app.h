#ifndef BTN_APP_H
#define BTN_APP_H

#ifdef __cplusplus
extern "C"
{
#endif

void btn_app_init(void); // 初始化按键应用层
void btn_task(void);     // 扫描按键并处理事件

#ifdef __cplusplus
}
#endif

#endif
