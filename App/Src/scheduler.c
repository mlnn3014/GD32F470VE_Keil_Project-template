#include "scheduler.h"

#include "adc_app.h"
#include "btn_app.h"
#include "dac_app.h"
#include "oled_app.h"
#include "ota_app.h"
#include "pt100_app.h"
#include "rs485_app.h"
#include "rtc_app.h"
#include "systick.h"
#include "uart0_app.h"

typedef struct
{
    void (*run)(void);   // 任务函数
    uint32_t period_ms;  // 任务周期
    uint32_t last_ms;    // 上次运行时间
} task_t;

// 简单轮询任务表
static task_t tasks[] = {
    {uart0_task, 1, 0},
    {rs485_task, 1, 0},
    {ota_task, 2, 0},
    {btn_task, 5, 0},
    {pt100_task, 1, 0},
    {rtc_task, 50, 0},
    {adc_task, 100, 0},
    {dac_task, 100, 0},
    {oled_task, 100, 0},
};

static const uint32_t task_count = sizeof(tasks) / sizeof(tasks[0]); // 任务数量

// 初始化每个任务的时间戳
void scheduler_init(void)
{
    uint32_t now_ms = systick_get_ms();

    for (uint32_t i = 0; i < task_count; i++)
    {
        tasks[i].last_ms = now_ms;
    }
}

// 到点就运行任务
void scheduler_run(void)
{
    uint32_t now_ms = systick_get_ms();

    for (uint32_t i = 0; i < task_count; i++)
    {
        if ((uint32_t)(now_ms - tasks[i].last_ms) >= tasks[i].period_ms)
        {
            tasks[i].last_ms = now_ms;
            tasks[i].run();
        }
    }
}
