#include "oled_bsp.h"

#include "gd32f4xx.h"
#include "gd32f4xx_dma.h"
#include "gd32f4xx_gpio.h"
#include "gd32f4xx_i2c.h"

#include <string.h>

#define OLED_BSP_I2C_PERIPH       I2C0
#define OLED_BSP_I2C_CLOCK        RCU_I2C0
#define OLED_BSP_I2C_OWN_ADDRESS  0x72U
#define OLED_BSP_I2C_WRITE_ADDR   0x78U
#define OLED_BSP_I2C_DATA_ADDRESS ((uint32_t)&I2C_DATA(OLED_BSP_I2C_PERIPH))

#define OLED_BSP_GPIO_CLOCK       RCU_GPIOB
#define OLED_BSP_GPIO_PORT        GPIOB
#define OLED_BSP_SCL_PIN          GPIO_PIN_8
#define OLED_BSP_SDA_PIN          GPIO_PIN_9

#define OLED_BSP_DMA_PERIPH       DMA0
#define OLED_BSP_DMA_CLOCK        RCU_DMA0
#define OLED_BSP_DMA_CH           DMA_CH6
#define OLED_BSP_DMA_SUBPERI      DMA_SUBPERI1

#define OLED_BSP_OK               0U
#define OLED_BSP_ERR              1U
#define OLED_BSP_TIMEOUT          3U
#define OLED_BSP_WAIT_TIMEOUT     100000U
#define OLED_BSP_TX_BUF_SIZE      513U

static uint8_t oled_tx_buf[OLED_BSP_TX_BUF_SIZE];

static uint8_t oled_wait_i2c_flag_set(uint32_t i2c_periph, i2c_flag_enum flag, uint32_t timeout)
{
    while (timeout-- != 0U) {
        if (i2c_flag_get(i2c_periph, flag) == SET) {
            return 1U;
        }
    }

    return 0U;
}

static uint8_t oled_wait_i2c_stop_clear(uint32_t i2c_periph, uint32_t timeout)
{
    while (timeout-- != 0U) {
        if ((I2C_CTL0(i2c_periph) & I2C_CTL0_STOP) == 0U) {
            return 1U;
        }
    }

    return 0U;
}

static uint8_t oled_wait_dma_ftf(uint32_t timeout)
{
    while (timeout-- != 0U) {
        if (dma_flag_get(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH, DMA_FLAG_FTF) == SET) {
            return 1U;
        }
    }

    return 0U;
}

static uint8_t oled_wait_i2c_tx_complete(uint32_t timeout)
{
    while (timeout-- != 0U) {
        if (i2c_flag_get(OLED_BSP_I2C_PERIPH, I2C_FLAG_AERR) == SET) {
            i2c_flag_clear(OLED_BSP_I2C_PERIPH, I2C_FLAG_AERR);
            return OLED_BSP_ERR;
        }

        if (i2c_flag_get(OLED_BSP_I2C_PERIPH, I2C_FLAG_BTC) == SET) {
            return OLED_BSP_OK;
        }
    }

    return OLED_BSP_TIMEOUT;
}

static uint8_t oled_wait_addsend_or_nack(uint32_t timeout)
{
    while (timeout-- != 0U) {
        if (i2c_flag_get(OLED_BSP_I2C_PERIPH, I2C_FLAG_ADDSEND) == SET) {
            return 1U;
        }

        if (i2c_flag_get(OLED_BSP_I2C_PERIPH, I2C_FLAG_AERR) == SET) {
            i2c_flag_clear(OLED_BSP_I2C_PERIPH, I2C_FLAG_AERR);
            return 0U;
        }
    }

    return 0U;
}

static void oled_dma_int_disable_all(void)
{
    dma_interrupt_disable(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH, DMA_INT_FTF);
    dma_interrupt_disable(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH, DMA_INT_TAE);
    dma_interrupt_disable(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH, DMA_INT_SDE);
    dma_interrupt_disable(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH, DMA_INT_FEE);
}

static void oled_dma_int_clear_all(void)
{
    dma_interrupt_flag_clear(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH, DMA_INT_FLAG_FTF);
    dma_interrupt_flag_clear(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH, DMA_INT_FLAG_TAE);
    dma_interrupt_flag_clear(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH, DMA_INT_FLAG_SDE);
    dma_interrupt_flag_clear(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH, DMA_INT_FLAG_FEE);
}

static void oled_stop_i2c_dma(void)
{
    oled_dma_int_disable_all();
    dma_channel_disable(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH);
    i2c_dma_config(OLED_BSP_I2C_PERIPH, I2C_DMA_OFF);
    i2c_dma_last_transfer_config(OLED_BSP_I2C_PERIPH, I2C_DMALST_OFF);
}

static void oled_i2c_bus_reset(void)
{
    uint8_t i;

    oled_stop_i2c_dma();

    gpio_mode_set(OLED_BSP_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE,
                  OLED_BSP_SCL_PIN | OLED_BSP_SDA_PIN);
    gpio_output_options_set(OLED_BSP_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            OLED_BSP_SCL_PIN | OLED_BSP_SDA_PIN);

    gpio_bit_set(OLED_BSP_GPIO_PORT, OLED_BSP_SCL_PIN | OLED_BSP_SDA_PIN);
    for (i = 0U; i < 9U; i++) {
        gpio_bit_reset(OLED_BSP_GPIO_PORT, OLED_BSP_SCL_PIN);
        gpio_bit_set(OLED_BSP_GPIO_PORT, OLED_BSP_SCL_PIN);
    }

    gpio_bit_set(OLED_BSP_GPIO_PORT, OLED_BSP_SCL_PIN);
    gpio_bit_reset(OLED_BSP_GPIO_PORT, OLED_BSP_SDA_PIN);
    gpio_bit_set(OLED_BSP_GPIO_PORT, OLED_BSP_SDA_PIN);

    gpio_af_set(OLED_BSP_GPIO_PORT, GPIO_AF_4, OLED_BSP_SDA_PIN | OLED_BSP_SCL_PIN);
    gpio_mode_set(OLED_BSP_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
                  OLED_BSP_SDA_PIN | OLED_BSP_SCL_PIN);
    gpio_output_options_set(OLED_BSP_GPIO_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ,
                            OLED_BSP_SDA_PIN | OLED_BSP_SCL_PIN);

    i2c_deinit(OLED_BSP_I2C_PERIPH);
    i2c_clock_config(OLED_BSP_I2C_PERIPH, 400000U, I2C_DTCY_2);
    i2c_mode_addr_config(OLED_BSP_I2C_PERIPH, I2C_I2CMODE_ENABLE,
                         I2C_ADDFORMAT_7BITS, OLED_BSP_I2C_OWN_ADDRESS);
    i2c_enable(OLED_BSP_I2C_PERIPH);
    i2c_ack_config(OLED_BSP_I2C_PERIPH, I2C_ACK_ENABLE);
}

static uint8_t oled_prepare_i2c(uint8_t addr)
{
    uint32_t timeout = 10000U;

    if (addr != OLED_BSP_I2C_WRITE_ADDR) {
        return OLED_BSP_ERR;
    }

    if (i2c_flag_get(OLED_BSP_I2C_PERIPH, I2C_FLAG_I2CBSY) == SET) {
        oled_i2c_bus_reset();
    }

    while ((i2c_flag_get(OLED_BSP_I2C_PERIPH, I2C_FLAG_I2CBSY) == SET) && (--timeout != 0U)) {
    }

    if (timeout == 0U) {
        oled_i2c_bus_reset();
        timeout = 10000U;
        while ((i2c_flag_get(OLED_BSP_I2C_PERIPH, I2C_FLAG_I2CBSY) == SET) && (--timeout != 0U)) {
        }
        if (timeout == 0U) {
            return OLED_BSP_TIMEOUT;
        }
    }

    i2c_start_on_bus(OLED_BSP_I2C_PERIPH);
    if (oled_wait_i2c_flag_set(OLED_BSP_I2C_PERIPH, I2C_FLAG_SBSEND, OLED_BSP_WAIT_TIMEOUT) == 0U) {
        oled_i2c_bus_reset();
        return OLED_BSP_TIMEOUT;
    }

    i2c_master_addressing(OLED_BSP_I2C_PERIPH, addr, I2C_TRANSMITTER);
    if (oled_wait_addsend_or_nack(OLED_BSP_WAIT_TIMEOUT) == 0U) {
        i2c_stop_on_bus(OLED_BSP_I2C_PERIPH);
        (void)oled_wait_i2c_stop_clear(OLED_BSP_I2C_PERIPH, OLED_BSP_WAIT_TIMEOUT / 10U);
        oled_i2c_bus_reset();
        return OLED_BSP_ERR;
    }

    i2c_flag_clear(OLED_BSP_I2C_PERIPH, I2C_FLAG_ADDSEND);
    if (oled_wait_i2c_flag_set(OLED_BSP_I2C_PERIPH, I2C_FLAG_TBE, OLED_BSP_WAIT_TIMEOUT) == 0U) {
        oled_i2c_bus_reset();
        return OLED_BSP_TIMEOUT;
    }

    return OLED_BSP_OK;
}

static uint8_t oled_tx_finish_blocking(void)
{
    uint8_t res;

    oled_stop_i2c_dma();
    res = oled_wait_i2c_tx_complete(OLED_BSP_WAIT_TIMEOUT);
    i2c_stop_on_bus(OLED_BSP_I2C_PERIPH);

    if (oled_wait_i2c_stop_clear(OLED_BSP_I2C_PERIPH, OLED_BSP_WAIT_TIMEOUT) == 0U) {
        oled_i2c_bus_reset();
        return OLED_BSP_TIMEOUT;
    }

    if (res != OLED_BSP_OK) {
        oled_i2c_bus_reset();
    }

    return res;
}

static uint8_t oled_dma_start(uint8_t control, const uint8_t *buf, uint16_t len)
{
    uint8_t res;

    if ((buf == NULL) || (len == 0U) || (len > (OLED_BSP_TX_BUF_SIZE - 1U))) {
        return OLED_BSP_ERR;
    }

    res = oled_prepare_i2c(OLED_BSP_I2C_WRITE_ADDR);
    if (res != OLED_BSP_OK) {
        return res;
    }

    oled_tx_buf[0] = control;
    (void)memcpy(&oled_tx_buf[1], buf, len);

    dma_channel_disable(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH);
    dma_flag_clear(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH, DMA_FLAG_FTF);
    dma_flag_clear(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH, DMA_FLAG_TAE);
    dma_flag_clear(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH, DMA_FLAG_SDE);
    dma_flag_clear(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH, DMA_FLAG_FEE);
    oled_dma_int_clear_all();
    dma_memory_address_config(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH, DMA_MEMORY_0, (uint32_t)oled_tx_buf);
    dma_transfer_number_config(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH, (uint32_t)len + 1U);
    i2c_dma_last_transfer_config(OLED_BSP_I2C_PERIPH, I2C_DMALST_ON);
    i2c_dma_config(OLED_BSP_I2C_PERIPH, I2C_DMA_ON);

    oled_dma_int_disable_all();
    dma_channel_enable(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH);

    return OLED_BSP_OK;
}

uint8_t oled_bus_init(void)
{
    dma_single_data_parameter_struct dma_init_struct;

    rcu_periph_clock_enable(OLED_BSP_GPIO_CLOCK);
    rcu_periph_clock_enable(OLED_BSP_I2C_CLOCK);
    rcu_periph_clock_enable(OLED_BSP_DMA_CLOCK);

    gpio_af_set(OLED_BSP_GPIO_PORT, GPIO_AF_4, OLED_BSP_SDA_PIN | OLED_BSP_SCL_PIN);
    gpio_mode_set(OLED_BSP_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_PULLUP,
                  OLED_BSP_SDA_PIN | OLED_BSP_SCL_PIN);
    gpio_output_options_set(OLED_BSP_GPIO_PORT, GPIO_OTYPE_OD, GPIO_OSPEED_50MHZ,
                            OLED_BSP_SDA_PIN | OLED_BSP_SCL_PIN);

    i2c_deinit(OLED_BSP_I2C_PERIPH);
    i2c_clock_config(OLED_BSP_I2C_PERIPH, 400000U, I2C_DTCY_2);
    i2c_mode_addr_config(OLED_BSP_I2C_PERIPH, I2C_I2CMODE_ENABLE,
                         I2C_ADDFORMAT_7BITS, OLED_BSP_I2C_OWN_ADDRESS);
    i2c_enable(OLED_BSP_I2C_PERIPH);
    i2c_ack_config(OLED_BSP_I2C_PERIPH, I2C_ACK_ENABLE);

    dma_deinit(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH);
    dma_single_data_para_struct_init(&dma_init_struct);
    dma_init_struct.direction = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.memory0_addr = (uint32_t)oled_tx_buf;
    dma_init_struct.memory_inc = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.periph_addr = OLED_BSP_I2C_DATA_ADDRESS;
    dma_init_struct.periph_inc = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.number = 1U;
    dma_init_struct.priority = DMA_PRIORITY_ULTRA_HIGH;
    dma_single_data_mode_init(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH, &dma_init_struct);
    dma_circulation_disable(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH);
    dma_channel_subperipheral_select(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH, OLED_BSP_DMA_SUBPERI);

    return OLED_BSP_OK;
}

uint8_t oled_bus_deinit(void)
{
    oled_stop_i2c_dma();
    i2c_deinit(OLED_BSP_I2C_PERIPH);

    return OLED_BSP_OK;
}

uint8_t oled_bus_write(uint8_t control, const uint8_t *buf, uint16_t len)
{
    uint8_t res;

    res = oled_dma_start(control, buf, len);
    if (res != OLED_BSP_OK) {
        return res;
    }

    if (oled_wait_dma_ftf(OLED_BSP_WAIT_TIMEOUT) == 0U) {
        oled_stop_i2c_dma();
        i2c_stop_on_bus(OLED_BSP_I2C_PERIPH);
        (void)oled_wait_i2c_stop_clear(OLED_BSP_I2C_PERIPH, OLED_BSP_WAIT_TIMEOUT / 10U);
        oled_i2c_bus_reset();
        return OLED_BSP_TIMEOUT;
    }

    dma_flag_clear(OLED_BSP_DMA_PERIPH, OLED_BSP_DMA_CH, DMA_FLAG_FTF);

    return oled_tx_finish_blocking();
}
