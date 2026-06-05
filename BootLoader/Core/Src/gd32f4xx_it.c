#include "gd32f4xx_it.h"

#include "systick.h"

// NMI 异常, 留在现场
void NMI_Handler(void)
{
    while (1)
    {
    }
}

// HardFault 异常, 留在现场
void HardFault_Handler(void)
{
    while (1)
    {
    }
}

// MPU 异常, 留在现场
void MemManage_Handler(void)
{
    while (1)
    {
    }
}

// 总线异常, 留在现场
void BusFault_Handler(void)
{
    while (1)
    {
    }
}

// 用法异常, 留在现场
void UsageFault_Handler(void)
{
    while (1)
    {
    }
}

// SVC 异常, BootLoader 不处理
void SVC_Handler(void)
{
    while (1)
    {
    }
}

// DebugMon 异常, BootLoader 不处理
void DebugMon_Handler(void)
{
    while (1)
    {
    }
}

// PendSV 异常, BootLoader 不处理
void PendSV_Handler(void)
{
    while (1)
    {
    }
}

// SysTick 中断, 只维护 delay 计数
void SysTick_Handler(void)
{
    delay_decrement();
}
