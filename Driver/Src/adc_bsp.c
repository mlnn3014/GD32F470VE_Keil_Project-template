#include "adc_bsp.h"

#include "gd32f4xx.h"
#include "systick.h"

#define ADC_BSP_PERIPH      ADC0                              // 使用 ADC0
#define ADC_BSP_CH0         ADC_CHANNEL_10                    // CH0: PC0 电位器
#define ADC_BSP_CH1         ADC_CHANNEL_11                    // CH1: PC1, PA4 DAC 回读
#define ADC_BSP_DATA_REG    ((uint32_t)&ADC_RDATA(ADC_BSP_PERIPH)) // ADC 数据寄存器

#define ADC_BSP_GPIO_CLOCK  RCU_GPIOC  // ADC 引脚时钟
#define ADC_BSP_GPIO_PORT   GPIOC      // ADC 引脚端口
#define ADC_BSP_GPIO_PIN    (GPIO_PIN_0 | GPIO_PIN_1) // PC0/PC1 ADC 输入

#define ADC_BSP_DMA_PERIPH  DMA1       // ADC DMA 控制器
#define ADC_BSP_DMA_CLOCK   RCU_DMA1   // ADC DMA 时钟
#define ADC_BSP_DMA_CH      DMA_CH0    // ADC DMA 通道
#define ADC_BSP_DMA_SUBPERI DMA_SUBPERI0 // ADC DMA 子外设

static volatile uint16_t adc_sample[2]; // DMA 写入 CH0/CH1 最新值

// 配置 ADC 连续采样, DMA 自动搬到 adc_sample
void adc_init(void)
{
    dma_single_data_parameter_struct dma_init_struct;

    rcu_periph_clock_enable(ADC_BSP_GPIO_CLOCK);
    rcu_periph_clock_enable(RCU_ADC0);
    rcu_periph_clock_enable(ADC_BSP_DMA_CLOCK);

    adc_clock_config(ADC_ADCCK_PCLK2_DIV8);

    gpio_mode_set(ADC_BSP_GPIO_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, ADC_BSP_GPIO_PIN);

    dma_deinit(ADC_BSP_DMA_PERIPH, ADC_BSP_DMA_CH);
    dma_single_data_para_struct_init(&dma_init_struct);
    dma_init_struct.direction = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.memory0_addr = (uint32_t)adc_sample;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_addr = ADC_BSP_DATA_REG;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_16BIT;
    dma_init_struct.number = 2;
    dma_init_struct.priority = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(ADC_BSP_DMA_PERIPH, ADC_BSP_DMA_CH, &dma_init_struct);
    dma_circulation_enable(ADC_BSP_DMA_PERIPH, ADC_BSP_DMA_CH);
    dma_channel_subperipheral_select(ADC_BSP_DMA_PERIPH, ADC_BSP_DMA_CH, ADC_BSP_DMA_SUBPERI);
    dma_channel_enable(ADC_BSP_DMA_PERIPH, ADC_BSP_DMA_CH);

    adc_sync_mode_config(ADC_SYNC_MODE_INDEPENDENT);
    adc_special_function_config(ADC_BSP_PERIPH, ADC_CONTINUOUS_MODE, ENABLE);
    adc_special_function_config(ADC_BSP_PERIPH, ADC_SCAN_MODE, ENABLE);
    adc_data_alignment_config(ADC_BSP_PERIPH, ADC_DATAALIGN_RIGHT);
    adc_channel_length_config(ADC_BSP_PERIPH, ADC_ROUTINE_CHANNEL, 2);
    adc_routine_channel_config(ADC_BSP_PERIPH, 0, ADC_BSP_CH0, ADC_SAMPLETIME_84);
    adc_routine_channel_config(ADC_BSP_PERIPH, 1, ADC_BSP_CH1, ADC_SAMPLETIME_84);
    adc_external_trigger_config(ADC_BSP_PERIPH, ADC_ROUTINE_CHANNEL, EXTERNAL_TRIGGER_DISABLE);
    adc_dma_request_after_last_enable(ADC_BSP_PERIPH);
    adc_dma_mode_enable(ADC_BSP_PERIPH);

    adc_enable(ADC_BSP_PERIPH);
    delay_1ms(1);
    adc_calibration_enable(ADC_BSP_PERIPH);
    adc_software_trigger_enable(ADC_BSP_PERIPH, ADC_ROUTINE_CHANNEL);
}

// 读取最近一次采样
void adc_read(adc_bsp_data_t *out)
{
    if (out == 0)
        return;

    out->ch0_raw = adc_sample[0];
    out->ch1_raw = adc_sample[1];
}
