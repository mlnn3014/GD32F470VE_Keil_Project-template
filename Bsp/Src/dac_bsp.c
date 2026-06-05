#include "dac_bsp.h"

#include "gd32f4xx.h"

#define DAC_BSP_PERIPH     DAC0       // 使用 DAC0
#define DAC_BSP_OUT        DAC_OUT0   // DAC 输出通道0
#define DAC_BSP_GPIO_CLOCK RCU_GPIOA  // DAC 引脚时钟
#define DAC_BSP_GPIO_PORT  GPIOA      // DAC 引脚端口
#define DAC_BSP_GPIO_PIN   GPIO_PIN_4 // DAC 输出引脚

static uint16_t dac_value; // 当前写入的 DAC raw 值

// 初始化 DAC 输出引脚和外设
void dac_init(void)
{
    rcu_periph_clock_enable(DAC_BSP_GPIO_CLOCK);
    rcu_periph_clock_enable(RCU_DAC);

    gpio_mode_set(DAC_BSP_GPIO_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, DAC_BSP_GPIO_PIN);

    dac_deinit(DAC_BSP_PERIPH);
    dac_trigger_disable(DAC_BSP_PERIPH, DAC_BSP_OUT);
    dac_dma_disable(DAC_BSP_PERIPH, DAC_BSP_OUT);
    dac_wave_mode_config(DAC_BSP_PERIPH, DAC_BSP_OUT, DAC_WAVE_DISABLE);
    dac_enable(DAC_BSP_PERIPH, DAC_BSP_OUT);

    dac_write(0);
}

// 写 DAC raw 值
void dac_write(uint16_t value)
{
    dac_value = value;
    dac_data_set(DAC_BSP_PERIPH, DAC_BSP_OUT, DAC_ALIGN_12B_R, value);
}

// 返回最近一次写入的 DAC 值
uint16_t dac_read(void)
{
    return dac_value;
}
