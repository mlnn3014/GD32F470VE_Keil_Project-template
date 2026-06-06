#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "stdint.h"

#ifdef __cplusplus
extern "C"
{
#endif

void scheduler_init(void); // 初始化任务表
void scheduler_run(void);  // 扫描并运行到期任务

#ifdef __cplusplus
}
#endif

#endif
