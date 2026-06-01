#include "scheduler.h"

#include "adc_app.h"
#include "btn_app.h"
#include "dac_app.h"
#include "oled_app.h"
#include "pt100_app.h"
#include "rs485_app.h"
#include "rtc_app.h"
#include "systick.h"
#include "uart0_app.h"

typedef struct
{
    void (*run)(void);
    uint32_t period_ms;
    uint32_t last_ms;
} task_t;

static task_t tasks[] = {
    {uart0_task, 5, 0},
    {rs485_task, 5, 0},
    {btn_task, 5, 0},
    {pt100_task, 1, 0},
    {rtc_task, 50, 0},
    {adc_task, 100, 0},
    {dac_task, 100, 0},
    {oled_task, 100, 0},
};

static const uint32_t task_count = sizeof(tasks) / sizeof(tasks[0]);

void scheduler_init(void)
{
    uint32_t now_ms = systick_get_ms();

    for (uint32_t i = 0; i < task_count; i++)
    {
        tasks[i].last_ms = now_ms;
    }
}

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
