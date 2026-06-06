/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2025/06/05
* Note:
*/
#include "mcu_cimc_gd32f470vet6.h"

/* OLED command/data buffers */
__IO uint8_t oled_cmd_buf[2] = {0x00, 0x00};  // control byte + command
__IO uint8_t oled_data_buf[2] = {0x40, 0x00}; // control byte + data

/* GD30AD3344 SPI DMA buffers */
uint8_t gd30_send_array[ARRAYSIZE] = {0};
uint8_t gd30_receive_array[ARRAYSIZE] = {0};

/* SPI1 DMA buffers */
uint8_t spi1_send_array[ARRAYSIZE] = {0};
uint8_t spi1_receive_array[ARRAYSIZE] = {0};

/* OTA UART DMA RX buffer */
uint8_t rxbuffer[OTA_UART_RXBUF_SIZE];
/* DEBUG USART0 DMA RX buffer */
uint8_t debug_rxbuffer[DEBUG_UART_RXBUF_SIZE];

/* ADC sample buffer */
uint16_t adc_value[2];

/* DAC  */
uint16_t convertarr[CONVERT_NUM] = {0};

/* RTC */
rtc_parameter_struct rtc_initpara;
rtc_alarm_struct  rtc_alarm;
__IO uint32_t prescaler_a = 0, prescaler_s = 0;
uint32_t RTCSRC_FLAG = 0;

void bsp_led_init(void)
{
    /* enable the led clock */
    rcu_periph_clock_enable(LED_CLK_PORT);
    /* configure led GPIO port */ 
    gpio_mode_set(LED_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, LED1_PIN | LED2_PIN | LED3_PIN | LED4_PIN | LED5_PIN | LED6_PIN);
    gpio_output_options_set(LED_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, LED1_PIN | LED2_PIN | LED3_PIN | LED4_PIN | LED5_PIN | LED6_PIN);

    LED1_OFF;
    LED2_OFF;
    LED3_OFF;
    LED4_OFF;
    LED5_OFF;
    LED6_OFF;
}

void bsp_btn_init(void)
{
    /* enable the led clock */
    rcu_periph_clock_enable(KEYB_CLK_PORT);
    rcu_periph_clock_enable(KEYE_CLK_PORT);
    rcu_periph_clock_enable(KEYA_CLK_PORT);
    
    /* configure led GPIO port */ 
    gpio_mode_set(KEYE_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, KEY1_PIN | KEY2_PIN | KEY3_PIN | KEY4_PIN | KEY5_PIN);
    gpio_mode_set(KEYB_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, KEY6_PIN);
    gpio_mode_set(KEYA_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, KEYW_PIN);
}

void bsp_wkup_key_exti_init(void)
{
    rcu_periph_clock_enable(KEYA_CLK_PORT);
    rcu_periph_clock_enable(RCU_SYSCFG);

    gpio_mode_set(KEYA_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, KEYW_PIN);

    syscfg_exti_line_config(EXTI_SOURCE_GPIOA, EXTI_SOURCE_PIN0);
    exti_init(EXTI_0, EXTI_INTERRUPT, EXTI_TRIG_BOTH);
    exti_interrupt_flag_clear(EXTI_0);
    nvic_irq_enable(EXTI0_IRQn, 1U, 0U);
}

static void bsp_ota_disable_for_deepsleep(void)
{
    ota_uart_reset_state();

    usart_interrupt_disable(OTA_UART_PERIPH, USART_INT_IDLE);
    nvic_irq_disable((IRQn_Type)OTA_UART_IRQN);
    usart_dma_receive_config(OTA_UART_PERIPH, USART_RECEIVE_DMA_DISABLE);
    dma_channel_disable(OTA_UART_DMA, OTA_UART_DMA_CH);
    usart_disable(OTA_UART_PERIPH);

    if(DEBUG_USART != OTA_UART_PERIPH) {
        usart_interrupt_disable(DEBUG_USART, USART_INT_IDLE);
        nvic_irq_disable(USART0_IRQn);
        usart_dma_receive_config(DEBUG_USART, USART_RECEIVE_DMA_DISABLE);
        dma_channel_disable(DEBUG_UART_RX_DMA, DEBUG_UART_RX_DMA_CH);
        usart_disable(DEBUG_USART);
    }
}

static void bsp_oled_disable_for_deepsleep(void)
{
    OLED_Display_Off();

    i2c_dma_config(I2C0, I2C_DMA_OFF);
    dma_channel_disable(DMA0, DMA_CH6);
    i2c_disable(I2C0);

    gpio_mode_set(OLED_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, OLED_DAT_PIN | OLED_CLK_PIN);
}

static void bsp_spi_disable_for_deepsleep(void)
{
    SPI_FLASH_CS_HIGH();

    spi_dma_disable(SPI1, SPI_DMA_RECEIVE);
    spi_dma_disable(SPI1, SPI_DMA_TRANSMIT);
    spi_dma_disable(GD30_SPI, SPI_DMA_RECEIVE);
    spi_dma_disable(GD30_SPI, SPI_DMA_TRANSMIT);

    dma_channel_disable(DMA0, DMA_CH3);
    dma_channel_disable(DMA0, DMA_CH4);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_TX);

    spi_disable(SPI1);
    spi_disable(GD30_SPI);
}

static void bsp_gpio_enter_deepsleep_state(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_GPIOB);
    rcu_periph_clock_enable(RCU_GPIOC);
    rcu_periph_clock_enable(RCU_GPIOD);
    rcu_periph_clock_enable(RCU_GPIOE);

    LED1_OFF;
    LED2_OFF;
    LED3_OFF;
    LED4_OFF;
    LED5_OFF;
    LED6_OFF;

    gpio_mode_set(KEYE_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  KEY1_PIN | KEY2_PIN | KEY3_PIN | KEY4_PIN | KEY5_PIN);
    gpio_mode_set(KEYB_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, KEY6_PIN);

    gpio_mode_set(DEBUG_USART_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  DEBUG_USART_TX_PIN | DEBUG_USART_RX_PIN);
    gpio_mode_set(OTA_UART_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  OTA_UART_TX_PIN | OTA_UART_RX_PIN);
    gpio_mode_set(RS485_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, RS485_CS_PIN);
    gpio_output_options_set(RS485_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, RS485_CS_PIN);
    RS485_CS_SET(0);

    gpio_mode_set(OLED_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  OLED_DAT_PIN | OLED_CLK_PIN);

    gpio_mode_set(SPI_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  SPI_SCK | SPI_MISO | SPI_MOSI);
    gpio_mode_set(SPI_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI_NSS);
    gpio_output_options_set(SPI_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, SPI_NSS);
    GPIO_BOP(SPI_PORT) = SPI_NSS;

    gpio_mode_set(GD30_SPI_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  GD30_SPI_SCK | GD30_SPI_MISO | GD30_SPI_MOSI);
    gpio_mode_set(GD30_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GD30_CS_PIN);
    gpio_output_options_set(GD30_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_2MHZ, GD30_CS_PIN);
    GPIO_BOP(GD30_CS_PORT) = GD30_CS_PIN;

#if ADC_VREF_SOURCE_PC2
    gpio_mode_set(ADC1_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, ADC1_PIN | ADC_CH1_PIN | ADC_VREF_PIN);
#else
    gpio_mode_set(ADC1_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, ADC1_PIN | ADC_CH1_PIN);
#endif
    gpio_mode_set(DAC1_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, DAC1_PIN);

    gpio_mode_set(GPIOC, GPIO_MODE_ANALOG, GPIO_PUPD_NONE,
                  GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | GPIO_PIN_12);
    gpio_mode_set(GPIOD, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, GPIO_PIN_2);

    gpio_mode_set(KEYA_PORT, GPIO_MODE_INPUT, GPIO_PUPD_PULLUP, KEYW_PIN);
}

static void bsp_deepsleep_reinit_after_wakeup(void)
{
    SystemInit();
    SystemCoreClockUpdate();
    systick_config();
    update_perf_counter();
    bsp_led_init();
    bsp_btn_init();
    bsp_usart_init();
    bsp_oled_init();
    OLED_Init();
    bsp_adc_init();
    bsp_dac_init();
    bsp_gd25qxx_init();
    bsp_gd30ad3344_init();
    flash_lfs_init();
    ota_uart_reset_state();
}

static void bsp_rtc_wakeup_timer_start(uint16_t seconds)
{
    if(seconds == 0U) {
        seconds = 1U;
    }

    exti_interrupt_flag_clear(EXTI_22);
    rtc_flag_clear(RTC_FLAG_WT);
    (void)rtc_wakeup_disable();
    (void)rtc_wakeup_clock_set(WAKEUP_CKSPRE);
    (void)rtc_wakeup_timer_set((uint16_t)(seconds - 1U));
    rtc_interrupt_enable(RTC_INT_WAKEUP);
    exti_init(EXTI_22, EXTI_INTERRUPT, EXTI_TRIG_RISING);
    nvic_irq_enable(RTC_WKUP_IRQn, 1U, 0U);
    rtc_wakeup_enable();
}

static void bsp_rtc_wakeup_timer_stop(void)
{
    (void)rtc_wakeup_disable();
    rtc_interrupt_disable(RTC_INT_WAKEUP);
    rtc_flag_clear(RTC_FLAG_WT);
    exti_interrupt_flag_clear(EXTI_22);
    nvic_irq_disable(RTC_WKUP_IRQn);
}

void bsp_enter_deepsleep(void)
{
    rcu_periph_clock_enable(RCU_PMU);

    __disable_irq();

    bsp_ota_disable_for_deepsleep();
    bsp_oled_disable_for_deepsleep();
    bsp_spi_disable_for_deepsleep();

    adc_disable(ADC0);
    adc_dma_mode_disable(ADC0);
    dma_channel_disable(ADC_DMA, ADC_DMA_CHANNEL);
    dac_disable(DAC0, DAC_OUT0);
    timer_disable(TIMER5);

    bsp_gpio_enter_deepsleep_state();
    bsp_wkup_key_exti_init();

    pmu_flag_clear(PMU_FLAG_RESET_WAKEUP);
    pmu_flag_clear(PMU_FLAG_RESET_STANDBY);

    before_cycle_counter_reconfiguration();
    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;

    __enable_irq();

    pmu_to_deepsleepmode(PMU_LDO_LOWPOWER, PMU_LOWDRIVER_ENABLE, WFI_CMD);

    bsp_deepsleep_reinit_after_wakeup();
}

void bsp_enter_deepsleep_rtc_wakeup(uint16_t seconds)
{
    rcu_periph_clock_enable(RCU_PMU);

    __disable_irq();

    bsp_ota_disable_for_deepsleep();
    bsp_oled_disable_for_deepsleep();
    bsp_spi_disable_for_deepsleep();

    adc_disable(ADC0);
    adc_dma_mode_disable(ADC0);
    dma_channel_disable(ADC_DMA, ADC_DMA_CHANNEL);
    dac_disable(DAC0, DAC_OUT0);
    timer_disable(TIMER5);

    bsp_gpio_enter_deepsleep_state();
    bsp_rtc_wakeup_timer_start(seconds);

    pmu_flag_clear(PMU_FLAG_RESET_WAKEUP);
    pmu_flag_clear(PMU_FLAG_RESET_STANDBY);

    before_cycle_counter_reconfiguration();
    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;

    __enable_irq();

    pmu_to_deepsleepmode(PMU_LDO_LOWPOWER, PMU_LOWDRIVER_ENABLE, WFI_CMD);

    bsp_rtc_wakeup_timer_stop();
    bsp_deepsleep_reinit_after_wakeup();
}

/*!
    \brief      configure USART
    \param[in]  none
    \param[out] none
    \retval     none
*/
static void bsp_debug_usart_init(void)
{
    rcu_periph_clock_enable(DEBUG_USART_PORT_RCU);
    rcu_periph_clock_enable(DEBUG_USART_RCU);

    gpio_af_set(DEBUG_USART_PORT, DEBUG_USART_AF, DEBUG_USART_TX_PIN);
    gpio_af_set(DEBUG_USART_PORT, DEBUG_USART_AF, DEBUG_USART_RX_PIN);

    gpio_mode_set(DEBUG_USART_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, DEBUG_USART_TX_PIN);
    gpio_output_options_set(DEBUG_USART_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, DEBUG_USART_TX_PIN);

    gpio_mode_set(DEBUG_USART_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, DEBUG_USART_RX_PIN);
    gpio_output_options_set(DEBUG_USART_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, DEBUG_USART_RX_PIN);

    usart_deinit(DEBUG_USART);
    usart_baudrate_set(DEBUG_USART, DEBUG_USART_BAUDRATE);
    usart_receive_config(DEBUG_USART, USART_RECEIVE_ENABLE);
    usart_transmit_config(DEBUG_USART, USART_TRANSMIT_ENABLE);
    usart_enable(DEBUG_USART);
}

static void bsp_ota_usart_init(void)
{
    dma_single_data_parameter_struct dma_init_struct;

    /*
     * OTA UART RX uses circular DMA. The foreground task polls the DMA write
     * index and forwards only newly received bytes to the OTA frame parser.
     */
    rcu_periph_clock_enable(OTA_UART_DMA_RCU);

    dma_deinit(OTA_UART_DMA, OTA_UART_DMA_CH);
    dma_init_struct.direction = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.memory0_addr = (uint32_t)rxbuffer;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.number = OTA_UART_RXBUF_SIZE;
    dma_init_struct.periph_addr = OTA_UART_RDATA_ADDRESS;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(OTA_UART_DMA, OTA_UART_DMA_CH, &dma_init_struct);

    dma_circulation_enable(OTA_UART_DMA, OTA_UART_DMA_CH);
    dma_channel_subperipheral_select(OTA_UART_DMA, OTA_UART_DMA_CH, OTA_UART_DMA_SUBPERI);
    dma_channel_enable(OTA_UART_DMA, OTA_UART_DMA_CH);

    rcu_periph_clock_enable(OTA_UART_PORT_RCU);
    rcu_periph_clock_enable(OTA_UART_RCU);
    rcu_periph_clock_enable(RS485_CS_PORT_RCU);

    gpio_af_set(OTA_UART_PORT, OTA_UART_AF, OTA_UART_TX_PIN);
    gpio_af_set(OTA_UART_PORT, OTA_UART_AF, OTA_UART_RX_PIN);

    gpio_mode_set(OTA_UART_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, OTA_UART_TX_PIN);
    gpio_output_options_set(OTA_UART_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, OTA_UART_TX_PIN);

    gpio_mode_set(OTA_UART_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, OTA_UART_RX_PIN);
    gpio_output_options_set(OTA_UART_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, OTA_UART_RX_PIN);

    gpio_mode_set(RS485_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_PULLUP, RS485_CS_PIN);
    gpio_output_options_set(RS485_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, RS485_CS_PIN);
    RS485_CS_SET(0);

    usart_deinit(OTA_UART_PERIPH);
    usart_baudrate_set(OTA_UART_PERIPH, OTA_UART_BAUDRATE);
    usart_receive_config(OTA_UART_PERIPH, USART_RECEIVE_ENABLE);
    usart_transmit_config(OTA_UART_PERIPH, USART_TRANSMIT_ENABLE);
    usart_dma_receive_config(OTA_UART_PERIPH, USART_RECEIVE_DMA_ENABLE);
    usart_enable(OTA_UART_PERIPH);

    /*
     * IDLE interrupt is enabled to keep the UART peripheral serviced; frame
     * assembly itself is done by polling the circular DMA buffer in uart_task().
     */
    nvic_irq_enable((IRQn_Type)OTA_UART_IRQN, 0, 0);
    usart_interrupt_enable(OTA_UART_PERIPH, USART_INT_IDLE);
}

static void bsp_debug_usart_dma_rx_init(void)
{
    dma_single_data_parameter_struct dma_init_struct;

    rcu_periph_clock_enable(DEBUG_UART_RX_DMA_RCU);

    dma_deinit(DEBUG_UART_RX_DMA, DEBUG_UART_RX_DMA_CH);
    dma_init_struct.direction = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.memory0_addr = (uint32_t)debug_rxbuffer;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.number = DEBUG_UART_RXBUF_SIZE;
    dma_init_struct.periph_addr = DEBUG_UART_RDATA_ADDRESS;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(DEBUG_UART_RX_DMA, DEBUG_UART_RX_DMA_CH, &dma_init_struct);

    dma_circulation_disable(DEBUG_UART_RX_DMA, DEBUG_UART_RX_DMA_CH);
    dma_channel_subperipheral_select(DEBUG_UART_RX_DMA, DEBUG_UART_RX_DMA_CH, DEBUG_UART_RX_DMA_SUBPERI);
    dma_channel_enable(DEBUG_UART_RX_DMA, DEBUG_UART_RX_DMA_CH);

    usart_dma_receive_config(DEBUG_USART, USART_RECEIVE_DMA_ENABLE);
    nvic_irq_enable(USART0_IRQn, 0, 1);
    usart_interrupt_enable(DEBUG_USART, USART_INT_IDLE);
}

void bsp_usart_init(void)
{
    if(DEBUG_USART != OTA_UART_PERIPH) {
        bsp_debug_usart_init();
        if(DEBUG_USART == USART0) {
            bsp_debug_usart_dma_rx_init();
        }
    }
    bsp_ota_usart_init();
}

void bsp_ota_uart_dma_rearm(void)
{
    dma_channel_disable(OTA_UART_DMA, OTA_UART_DMA_CH);
    dma_flag_clear(OTA_UART_DMA, OTA_UART_DMA_CH, DMA_FLAG_FTF);
    dma_transfer_number_config(OTA_UART_DMA, OTA_UART_DMA_CH, OTA_UART_RXBUF_SIZE);
    dma_channel_enable(OTA_UART_DMA, OTA_UART_DMA_CH);
}

uint32_t bsp_ota_uart_dma_received_len(void)
{
    uint32_t dma_left_cnt = dma_transfer_number_get(OTA_UART_DMA, OTA_UART_DMA_CH);
    return (OTA_UART_RXBUF_SIZE - dma_left_cnt);
}

void bsp_oled_init(void)
{
    dma_single_data_parameter_struct dma_init_struct;
    /* enable GPIOB clock */
    rcu_periph_clock_enable(OLED_CLK_PORT);
    /* enable I2C0 clock */
    rcu_periph_clock_enable(RCU_I2C0);
    /* enable DMA0 clock */
    rcu_periph_clock_enable(RCU_DMA0);
    
    /* connect PB9 to I2C0_SDA */
    gpio_af_set(OLED_PORT, GPIO_AF_4, OLED_DAT_PIN);
    /* connect PB8 to I2C0_SCL */
    gpio_af_set(OLED_PORT, GPIO_AF_4, OLED_CLK_PIN);
    
    gpio_mode_set(OLED_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, OLED_DAT_PIN);
    gpio_output_options_set(OLED_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, OLED_DAT_PIN);
    gpio_mode_set(OLED_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP, OLED_CLK_PIN);
    gpio_output_options_set(OLED_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ, OLED_CLK_PIN);
    
    /* configure I2C0 clock */
    i2c_clock_config(I2C0, 400000, I2C_DTCY_2);
    /* configure I2C0 address */
    i2c_mode_addr_config(I2C0, I2C_I2CMODE_ENABLE, I2C_ADDFORMAT_7BITS, I2C0_OWN_ADDRESS7);
    /* enable I2C0 */
    i2c_enable(I2C0);
    /* enable acknowledge */
    i2c_ack_config(I2C0, I2C_ACK_ENABLE);
    
    /* Configure DMA channel for I2C0 TX */
    dma_deinit(DMA0, DMA_CH6);
    
    dma_single_data_para_struct_init(&dma_init_struct);
    dma_init_struct.direction = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.memory0_addr = (uint32_t)oled_data_buf;  // default source buffer
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.number = 2;  // control byte + payload byte
    dma_init_struct.periph_addr = I2C0_DATA_ADDRESS;  // I2C0 data register
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(DMA0, DMA_CH6, &dma_init_struct);
    
    /*  DMA  */
    dma_circulation_disable(DMA0, DMA_CH6);
    dma_channel_subperipheral_select(DMA0, DMA_CH6, DMA_SUBPERI1);  // I2C0 TX 
}

void bsp_gd25qxx_init(void)
{
    rcu_periph_clock_enable(SPI_CLK_PORT);
    rcu_periph_clock_enable(RCU_SPI1);
    rcu_periph_clock_enable(RCU_DMA0);
    
    /* configure SPI1 GPIO */
    gpio_af_set(SPI_PORT, GPIO_AF_5, SPI_SCK | SPI_MISO | SPI_MOSI);
    gpio_mode_set(SPI_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, SPI_SCK | SPI_MISO | SPI_MOSI);
    gpio_output_options_set(SPI_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SPI_SCK | SPI_MISO | SPI_MOSI);

    /* set SPI1_NSS as GPIO*/
    gpio_mode_set(SPI_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, SPI_NSS);
    gpio_output_options_set(SPI_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, SPI_NSS);
    
    spi_parameter_struct spi_init_struct;

    /* configure SPI1 parameter */
    spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode          = SPI_MASTER;
    spi_init_struct.frame_size           = SPI_FRAMESIZE_8BIT;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_HIGH_PH_2EDGE;
    spi_init_struct.nss                  = SPI_NSS_SOFT;
    spi_init_struct.prescale             = SPI_PSC_8;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;
    spi_init(SPI1, &spi_init_struct);

    /* Initialize SPI Flash device */
    spi_flash_init();
}

void bsp_gd30ad3344_init(void)
{
    rcu_periph_clock_enable(GD30_SPI_PORT_RCU);
    rcu_periph_clock_enable(GD30_CS_PORT_RCU);
    rcu_periph_clock_enable(GD30_SPI_RCU);
    rcu_periph_clock_enable(GD30_DMA_RCU);
    
    gpio_af_set(GD30_SPI_PORT, GPIO_AF_5, GD30_SPI_SCK | GD30_SPI_MISO | GD30_SPI_MOSI);
    gpio_mode_set(GD30_SPI_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE, GD30_SPI_SCK | GD30_SPI_MISO | GD30_SPI_MOSI);
    gpio_output_options_set(GD30_SPI_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GD30_SPI_SCK | GD30_SPI_MISO | GD30_SPI_MOSI);

    gpio_mode_set(GD30_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GD30_CS_PIN);
    gpio_output_options_set(GD30_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GD30_CS_PIN);
    GD30_CS_HIGH();
    
    spi_parameter_struct spi_init_struct;

    /* configure SPI1 parameter */
    spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode          = SPI_MASTER;
    spi_init_struct.frame_size           = SPI_FRAMESIZE_8BIT;
    spi_init_struct.clock_polarity_phase = GD30_SPIMODE;
    spi_init_struct.nss                  = SPI_NSS_SOFT;
    spi_init_struct.prescale             = SPI_PSC_256;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;
    spi_init(GD30_SPI, &spi_init_struct);

    /* Initialize SPI GD30AD3344 device */
    GD30AD3344_Init();
}

void bsp_adc_init(void)
{
    rcu_periph_clock_enable(ADC1_CLK_PORT);

    rcu_periph_clock_enable(RCU_ADC0);
    
    rcu_periph_clock_enable(RCU_DMA1);
    
    adc_clock_config(ADC_ADCCK_PCLK2_DIV8);
    
    /* config the GPIO as analog mode */
#if ADC_VREF_SOURCE_PC2
    gpio_mode_set(ADC1_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, ADC1_PIN | ADC_CH1_PIN | ADC_VREF_PIN);
#else
    gpio_mode_set(ADC1_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, ADC1_PIN | ADC_CH1_PIN);
    adc_channel_16_to_18(ADC_TEMP_VREF_CHANNEL_SWITCH, ENABLE);
#endif
    
    /* ADC_DMA_channel configuration */
    dma_single_data_parameter_struct dma_single_data_parameter;

    /* ADC DMA_channel configuration */
    dma_deinit(ADC_DMA, ADC_DMA_CHANNEL);

    /* initialize DMA single data mode */
    dma_single_data_parameter.periph_addr = (uint32_t)(&ADC_RDATA(ADC0));
    dma_single_data_parameter.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_single_data_parameter.memory0_addr = (uint32_t)(adc_value);
    dma_single_data_parameter.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_single_data_parameter.periph_memory_width = DMA_PERIPH_WIDTH_16BIT;
    dma_single_data_parameter.direction = DMA_PERIPH_TO_MEMORY;
    dma_single_data_parameter.number = 2;
    dma_single_data_parameter.priority = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(ADC_DMA, ADC_DMA_CHANNEL, &dma_single_data_parameter);
    dma_channel_subperipheral_select(ADC_DMA, ADC_DMA_CHANNEL, ADC_DMA_SUBPERI);

    /* enable DMA circulation mode */
    dma_circulation_enable(ADC_DMA, ADC_DMA_CHANNEL);

    /* enable DMA channel */
    dma_channel_enable(ADC_DMA, ADC_DMA_CHANNEL);
    
    /* ADC mode config */
    adc_sync_mode_config(ADC_SYNC_MODE_INDEPENDENT);
    /* ADC contineous function disable */
    adc_special_function_config(ADC0, ADC_CONTINUOUS_MODE, ENABLE);
    /* ADC scan mode disable */
    adc_special_function_config(ADC0, ADC_SCAN_MODE, ENABLE);
    /* ADC data alignment config */
    adc_data_alignment_config(ADC0, ADC_DATAALIGN_RIGHT);

    /* ADC channel length config */
    adc_channel_length_config(ADC0, ADC_ROUTINE_CHANNEL, 2);
    /* ADC routine channel config */
    adc_routine_channel_config(ADC0, 0, ADC_CHANNEL_10, ADC_SAMPLETIME_15);
    adc_routine_channel_config(ADC0, 1, ADC_CH1_CHANNEL, ADC_SAMPLETIME_15);
    /* ADC trigger config */
    adc_external_trigger_source_config(ADC0, ADC_ROUTINE_CHANNEL, ADC_EXTTRIG_ROUTINE_T0_CH0); 
    adc_external_trigger_config(ADC0, ADC_ROUTINE_CHANNEL, EXTERNAL_TRIGGER_DISABLE);

    /* ADC DMA function enable */
    adc_dma_request_after_last_enable(ADC0);
    adc_dma_mode_enable(ADC0);

    /* enable ADC interface */
    adc_enable(ADC0);
    /* wait for ADC stability */
    delay_1ms(1);
    /* ADC calibration and reset calibration */
    adc_calibration_enable(ADC0);

    /* enable ADC software trigger */
    adc_software_trigger_enable(ADC0, ADC_ROUTINE_CHANNEL);
}

void timer5_config(void)
{
    timer_parameter_struct timer_initpara;

    /* TIMER deinitialize */
    timer_deinit(TIMER5);

    /* TIMER configuration */
    timer_struct_para_init(&timer_initpara);
    timer_initpara.prescaler         = 239;
    timer_initpara.alignedmode       = TIMER_COUNTER_EDGE;
    timer_initpara.counterdirection  = TIMER_COUNTER_UP;
    timer_initpara.period            = 99;
    timer_initpara.clockdivision     = TIMER_CKDIV_DIV1;
    timer_initpara.repetitioncounter = 0;

    /* initialize TIMER init parameter struct */
    timer_init(TIMER5, &timer_initpara);

    /* TIMER master mode output trigger source: Update event */
    timer_master_output_trigger_source_select(TIMER5, TIMER_TRI_OUT_SRC_UPDATE);

    /* enable TIMER */
    timer_enable(TIMER5);
}

void bsp_dac_init(void)
{
    /* enable GPIOA clock */
    rcu_periph_clock_enable(DAC1_CLK_PORT);
    /* enable DAC clock */
    rcu_periph_clock_enable(RCU_DAC);
    /* enable TIMER clock */
    rcu_periph_clock_enable(RCU_TIMER5);
    
    /* configure PA4 as DAC output */
    gpio_mode_set(DAC1_PORT, GPIO_MODE_ANALOG, GPIO_PUPD_NONE, DAC1_PIN);
    
    /* initialize DAC */
    dac_deinit(DAC0);
    /* DAC trigger config */
    dac_trigger_source_config(DAC0, DAC_OUT0, DAC_TRIGGER_T5_TRGO);
    /* DAC trigger enable */
    dac_trigger_enable(DAC0, DAC_OUT0);
    /* DAC wave mode config */
    dac_wave_mode_config(DAC0, DAC_OUT0, DAC_WAVE_DISABLE);

    /* DAC enable */
    dac_enable(DAC0, DAC_OUT0);
    
    timer5_config();
}

int bsp_rtc_setup(void)
{
    int ret = 0;
    /* setup RTC time value */
    uint32_t tmp_hh = 0x23, tmp_mm = 0x59, tmp_ss = 0x50;

    rtc_initpara.factor_asyn = prescaler_a;
    rtc_initpara.factor_syn = prescaler_s;
    rtc_initpara.year = 0x25;
    rtc_initpara.day_of_week = RTC_SATURDAY;
    rtc_initpara.month = RTC_APR;
    rtc_initpara.date = 0x30;
    rtc_initpara.display_format = RTC_24HOUR;
    rtc_initpara.am_pm = RTC_AM;

    /* current time input */
    rtc_initpara.hour = tmp_hh;
    rtc_initpara.minute = tmp_mm;
    rtc_initpara.second = tmp_ss;

    /* RTC current time configuration */
    if(ERROR == rtc_init(&rtc_initpara)){
        ret = -1;
    }else{
        RTC_BKP0 = BKP_VALUE;
    }
    return ret;
}

void bsp_rtc_pre_cfg(void)
{
    #if defined (RTC_CLOCK_SOURCE_IRC32K)
          rcu_osci_on(RCU_IRC32K);
          rcu_osci_stab_wait(RCU_IRC32K);
          rcu_rtc_clock_config(RCU_RTCSRC_IRC32K);

          prescaler_s = 0x13F;
          prescaler_a = 0x63;
    #elif defined (RTC_CLOCK_SOURCE_LXTAL)
          rcu_osci_on(RCU_LXTAL);
          rcu_osci_stab_wait(RCU_LXTAL);
          rcu_rtc_clock_config(RCU_RTCSRC_LXTAL);

          prescaler_s = 0xFF;
          prescaler_a = 0x7F;
    #else
    #error RTC clock source should be defined.
    #endif /* RTC_CLOCK_SOURCE_IRC32K */

    rcu_periph_clock_enable(RCU_RTC);
    rtc_register_sync_wait();
}

int bsp_rtc_init(void)
{
    int ret = 0;
    /* enable access to RTC registers in Backup domain */
    rcu_periph_clock_enable(RCU_PMU);
    pmu_backup_write_enable();

    bsp_rtc_pre_cfg();
    /* get RTC clock entry selection */
    RTCSRC_FLAG = GET_BITS(RCU_BDCTL, 8, 9);

    /* Set initial RTC each boot (board may not keep backup power) */
    bsp_rtc_setup();
    
//    if((BKP_VALUE != RTC_BKP0) || (0x00 == RTCSRC_FLAG)){
//        /* backup data register value is not correct or not yet programmed
//        or RTC clock source is not configured (when the first time the program 
//        is executed or data in RCU_BDCTL is lost due to Vbat feeding) */
//        ret = bsp_rtc_setup();
//    }else{
//        /* detect the reset source */
//        if (RESET != rcu_flag_get(RCU_FLAG_PORRST)){
//            ret = 1;
//        }else if (RESET != rcu_flag_get(RCU_FLAG_EPRST)){
//            ret = 2;
//        }
//    }

    rcu_all_reset_flag_clear();
    return ret;
}
