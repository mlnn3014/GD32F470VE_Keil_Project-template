#ifndef OTA_APP_H
#define OTA_APP_H

#ifdef __cplusplus
extern "C" {
#endif

void ota_app_init(void); // 初始化 OTA 接收状态
void ota_task(void);     // OTA 串口接收和写入任务

#ifdef __cplusplus
}
#endif

#endif
