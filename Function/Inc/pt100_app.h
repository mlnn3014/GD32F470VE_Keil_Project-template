#ifndef PT100_APP_H
#define PT100_APP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef enum
{
    PT100_WAIT = 0, // 等待有效采样
    PT100_OK,       // 温度数据正常
    PT100_SPI,      // GD30 SPI 通讯异常
    PT100_LOW,      // 测量值低于范围
    PT100_HIGH      // 测量值高于范围
} pt100_status_t;

typedef struct
{
    int16_t raw;           // ADC 原始采样值
    int32_t ain0_uv;       // AIN0 输入电压, uV
    int32_t lead_uv;       // 引线补偿电压, uV
    int32_t pt_uv;         // PT100 实际电压, uV
    int32_t r_mohm;        // PT100 电阻, milli-ohm
    int32_t temp;          // 温度, 0.01C
    pt100_status_t status; // 当前测量状态
    uint8_t ok;            // 1 表示数据可用
    uint8_t ref_on;        // AIN3 reference 是否打开
} pt100_data_t;

extern pt100_data_t pt100; // PT100 app 层最新数据
void pt100_app_init(void); // 初始化 PT100 采样
void pt100_task(void);     // 周期读取 PT100 并上报
const char *pt100_status_text(pt100_status_t status); // 状态转打印字符串

#ifdef __cplusplus
}
#endif

#endif
