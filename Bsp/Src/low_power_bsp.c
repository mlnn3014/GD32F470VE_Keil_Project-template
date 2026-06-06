#include "low_power_bsp.h"

#include "gd32f4xx.h"

#define LP_EXTI_INTEN   REG32(EXTI_BASE + 0x00U)     // EXTI interrupt enable
#define LP_EXTI_RTEN    REG32(EXTI_BASE + 0x08U)     // EXTI rising enable
#define LP_EXTI_FTEN    REG32(EXTI_BASE + 0x0CU)     // EXTI falling enable
#define LP_EXTI_PD      REG32(EXTI_BASE + 0x14U)     // EXTI pending
#define LP_SYSCFG_EXTISS0 REG32(SYSCFG_BASE + 0x08U) // EXTI source select

#define LP_EXTI0        BIT(0) // PA0 wakeup line
#define LP_EXTI22       BIT(22) // RTC wakeup line

#define LP_USART0_RX_DMA DMA_CH2 // UART0 RX DMA
#define LP_USART0_TX_DMA DMA_CH7 // UART0 TX DMA
#define LP_USART1_RX_DMA DMA_CH5 // RS485 RX DMA
#define LP_USART2_RX_DMA DMA_CH1 // OTA RX DMA
#define LP_OLED_DMA      DMA_CH6 // OLED DMA
#define LP_ADC_DMA       DMA_CH0 // ADC DMA

// 关闭各路 USART 和 DMA
static void low_power_usart_off(void)
{
    usart_interrupt_disable(USART0, USART_INT_IDLE);
    usart_dma_receive_config(USART0, USART_RECEIVE_DMA_DISABLE);
    usart_dma_transmit_config(USART0, USART_TRANSMIT_DMA_DISABLE);
    dma_channel_disable(DMA1, LP_USART0_RX_DMA);
    dma_channel_disable(DMA1, LP_USART0_TX_DMA);
    usart_disable(USART0);
    nvic_irq_disable(USART0_IRQn);
    nvic_irq_disable(DMA1_Channel7_IRQn);

    usart_interrupt_disable(USART1, USART_INT_IDLE);
    usart_interrupt_disable(USART1, USART_INT_TBE);
    usart_interrupt_disable(USART1, USART_INT_TC);
    usart_dma_receive_config(USART1, USART_RECEIVE_DMA_DISABLE);
    dma_channel_disable(DMA0, LP_USART1_RX_DMA);
    usart_disable(USART1);
    nvic_irq_disable(USART1_IRQn);

    usart_interrupt_disable(USART2, USART_INT_IDLE);
    usart_dma_receive_config(USART2, USART_RECEIVE_DMA_DISABLE);
    dma_channel_disable(DMA0, LP_USART2_RX_DMA);
    usart_disable(USART2);
    nvic_irq_disable(USART2_IRQn);
}

// 关闭 OLED I2C 和 DMA
static void low_power_oled_off(void)
{
    i2c_dma_config(I2C0, I2C_DMA_OFF);
    dma_channel_disable(DMA0, LP_OLED_DMA);
    i2c_disable(I2C0);

    gpio_mode_set(GPIOB, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_8 | GPIO_PIN_9);
}

// 关闭 SPI, 并把相关脚放到低功耗状态
static void low_power_spi_off(void)
{
    gpio_bit_set(GPIOB, GPIO_PIN_12);
    gpio_bit_set(GPIOE, GPIO_PIN_8);

    spi_dma_disable(SPI0, SPI_DMA_RECEIVE);
    spi_dma_disable(SPI0, SPI_DMA_TRANSMIT);
    spi_dma_disable(SPI1, SPI_DMA_RECEIVE);
    spi_dma_disable(SPI1, SPI_DMA_TRANSMIT);
    spi_disable(SPI0);
    spi_disable(SPI1);

    gpio_mode_set(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7);
    gpio_mode_set(GPIOB, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15);

    gpio_mode_set(GPIOB, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_12);
    gpio_output_options_set(GPIOB, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO_PIN_12);
    gpio_bit_set(GPIOB, GPIO_PIN_12);

    gpio_mode_set(GPIOE, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GPIO_PIN_8);
    gpio_output_options_set(GPIOE, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GPIO_PIN_8);
    gpio_bit_set(GPIOE, GPIO_PIN_8);
}

// 关闭 ADC/DAC
static void low_power_adc_dac_off(void)
{
    adc_dma_mode_disable(ADC0);
    adc_disable(ADC0);
    dma_channel_disable(DMA1, LP_ADC_DMA);

    dac_disable(DAC0, DAC_OUT0);

    gpio_mode_set(GPIOC, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_0);
    gpio_mode_set(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_4);
}

// 统一设置进低功耗前的 GPIO 状态
static void low_power_gpio_state(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOE);

    gpio_bit_reset(GPIOD, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
                          GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13);

    gpio_mode_set(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 |
                  GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 |
                  GPIO_PIN_9 | GPIO_PIN_10);
    gpio_mode_set(GPIOB, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  GPIO_PIN_0 | GPIO_PIN_8 | GPIO_PIN_9 |
                  GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_13 |
                  GPIO_PIN_14 | GPIO_PIN_15);
    gpio_mode_set(GPIOC, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_0);
    gpio_mode_set(GPIOE, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  GPIO_PIN_7 | GPIO_PIN_9 | GPIO_PIN_11 |
                  GPIO_PIN_13 | GPIO_PIN_15);

    gpio_mode_set(GPIOD, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
                  GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
                  GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13);
    gpio_output_options_set(GPIOD, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ,
                            GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
                            GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13);
    gpio_bit_reset(GPIOD, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 |
                          GPIO_PIN_11 | GPIO_PIN_12 | GPIO_PIN_13);

    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO_PIN_0);
}

// 配置 PA0 作为唤醒中断
static void low_power_wakeup_exti_init(void)
{
    rcu_periph_clock_enable(RCU_SYSCFG);
    rcu_periph_clock_enable(RCU_GPIOA);

    gpio_mode_set(GPIOA, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, GPIO_PIN_0);

    LP_SYSCFG_EXTISS0 &= ~0x0FUL;
    LP_EXTI_INTEN |= LP_EXTI0;
    LP_EXTI_RTEN |= LP_EXTI0;
    LP_EXTI_FTEN |= LP_EXTI0;
    LP_EXTI_PD = LP_EXTI0;

    nvic_irq_enable(EXTI0_IRQn, 1U, 0U);
}

// 配置 RTC 10s 唤醒
static void low_power_rtc_wakeup_10s_init(void)
{
    rtc_wakeup_disable();
    rtc_flag_clear(RTC_FLAG_WT);
    LP_EXTI_PD = LP_EXTI22;
    LP_EXTI_INTEN |= LP_EXTI22;
    LP_EXTI_RTEN |= LP_EXTI22;
    LP_EXTI_FTEN &= ~LP_EXTI22;

    rtc_wakeup_clock_set(WAKEUP_CKSPRE);
    rtc_wakeup_timer_set(9);
    rtc_interrupt_enable(RTC_INT_WAKEUP);
    rtc_wakeup_enable();
    nvic_irq_enable(RTC_WKUP_IRQn, 1U, 0U);
}

// 进入低功耗前关闭外设
static void low_power_prepare(void)
{
    low_power_usart_off();
    low_power_oled_off();
    low_power_spi_off();
    low_power_adc_dac_off();
    low_power_gpio_state();

    pmu_flag_clear(PMU_FLAG_RESET_WAKEUP);
    pmu_flag_clear(PMU_FLAG_RESET_STANDBY);

    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
}

// 进入 deep-sleep, 唤醒后直接复位
void low_power_enter_deepsleep(void)
{
    rcu_periph_clock_enable(RCU_PMU);

    __disable_irq();

    low_power_prepare();
    low_power_wakeup_exti_init();

    __enable_irq();

    pmu_to_deepsleepmode(PMU_LDO_LOWPOWER, PMU_LOWDRIVER_ENABLE, WFI_CMD);

    NVIC_SystemReset();
}

// 进入 deep-sleep, RTC 10s 唤醒后复位
void low_power_enter_deepsleep_rtc_10s(void)
{
    rcu_periph_clock_enable(RCU_PMU);

    __disable_irq();

    low_power_prepare();
    low_power_rtc_wakeup_10s_init();

    __enable_irq();

    pmu_to_deepsleepmode(PMU_LDO_LOWPOWER, PMU_LOWDRIVER_ENABLE, WFI_CMD);

    NVIC_SystemReset();
}

// PA0 唤醒中断
void EXTI0_IRQHandler(void)
{
    LP_EXTI_PD = LP_EXTI0;
    NVIC_SystemReset();
}

// RTC 10s 唤醒中断
void RTC_WKUP_IRQHandler(void)
{
    rtc_flag_clear(RTC_FLAG_WT);
    LP_EXTI_PD = LP_EXTI22;
    NVIC_SystemReset();
}

// 清除 RTC 唤醒状态
void low_power_rtc_wakeup_clear(void)
{
    rtc_wakeup_disable();
    rtc_interrupt_disable(RTC_INT_WAKEUP);
    rtc_flag_clear(RTC_FLAG_WT);
    LP_EXTI_PD = LP_EXTI22;
}
