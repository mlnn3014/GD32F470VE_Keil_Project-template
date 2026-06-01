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

typedef struct {
    void (*run)(void);
    uint32_t period_ms;
    uint32_t last_ms;
} task_t;

static task_t tasks[] = {
    {uart0_task, 5U,   0U},
    {rs485_task, 5U,   0U},
    {btn_task,   5U,   0U},
    {pt100_task, 1U,   0U},
    {rtc_task,   50U,  0U},
    {adc_task,   100U, 0U},
    {dac_task,   100U, 0U},
    {oled_task,  100U, 0U}
};

static const uint32_t task_count = sizeof(tasks) / sizeof(tasks[0]);

void scheduler_init(void)
{
    uint32_t now_ms = systick_get_ms();
    uint32_t i;

    for (i = 0U; i < task_count; i++) {
        tasks[i].last_ms = now_ms;
    }
}

void scheduler_run(void)
{
    uint32_t now_ms = systick_get_ms();
    uint32_t i;

    for (i = 0U; i < task_count; i++) {
        while ((uint32_t)(now_ms - tasks[i].last_ms) >= tasks[i].period_ms) {
            tasks[i].last_ms += tasks[i].period_ms;

            if ((uint32_t)(now_ms - tasks[i].last_ms) >= tasks[i].period_ms) {
                tasks[i].last_ms = now_ms;
            }

            tasks[i].run();
        }
    }
}
