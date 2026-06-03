#include "low_power_bsp.h"

#include "gd32f4xx.h"

#define LP_EXTI_INTEN   REG32(EXTI_BASE + 0x00U)
#define LP_EXTI_RTEN    REG32(EXTI_BASE + 0x08U)
#define LP_EXTI_FTEN    REG32(EXTI_BASE + 0x0CU)
#define LP_EXTI_PD      REG32(EXTI_BASE + 0x14U)
#define LP_SYSCFG_EXTISS0 REG32(SYSCFG_BASE + 0x08U)

#define LP_EXTI0        BIT(0)

#define LP_USART0_RX_DMA DMA_CH2
#define LP_USART0_TX_DMA DMA_CH7
#define LP_USART1_RX_DMA DMA_CH5
#define LP_USART2_RX_DMA DMA_CH1
#define LP_OLED_DMA      DMA_CH6
#define LP_ADC_DMA       DMA_CH0

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

static void low_power_oled_off(void)
{
    i2c_dma_config(I2C0, I2C_DMA_OFF);
    dma_channel_disable(DMA0, LP_OLED_DMA);
    i2c_disable(I2C0);

    gpio_mode_set(GPIOB, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_8 | GPIO_PIN_9);
}

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

static void low_power_adc_dac_off(void)
{
    adc_dma_mode_disable(ADC0);
    adc_disable(ADC0);
    dma_channel_disable(DMA1, LP_ADC_DMA);

    dac_disable(DAC0, DAC_OUT0);

    gpio_mode_set(GPIOC, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_0);
    gpio_mode_set(GPIOA, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_4);
}

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

void low_power_enter_deepsleep(void)
{
    rcu_periph_clock_enable(RCU_PMU);

    __disable_irq();

    low_power_usart_off();
    low_power_oled_off();
    low_power_spi_off();
    low_power_adc_dac_off();
    low_power_gpio_state();
    low_power_wakeup_exti_init();

    pmu_flag_clear(PMU_FLAG_RESET_WAKEUP);
    pmu_flag_clear(PMU_FLAG_RESET_STANDBY);

    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;

    __enable_irq();

    pmu_to_deepsleepmode(PMU_LDO_LOWPOWER, PMU_LOWDRIVER_ENABLE, WFI_CMD);

    NVIC_SystemReset();
}

void EXTI0_IRQHandler(void)
{
    LP_EXTI_PD = LP_EXTI0;
    NVIC_SystemReset();
}
