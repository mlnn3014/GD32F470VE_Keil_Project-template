#include "flash_bsp.h"

#define FLASH_SPI              SPI1
#define FLASH_GPIO_PORT        GPIOB
#define FLASH_GPIO_CLK         RCU_GPIOB
#define FLASH_SPI_CLK          RCU_SPI1
#define FLASH_PIN_CS           GPIO_PIN_12
#define FLASH_PIN_SCK          GPIO_PIN_13
#define FLASH_PIN_MISO         GPIO_PIN_14
#define FLASH_PIN_MOSI         GPIO_PIN_15
#define FLASH_DUMMY_BYTE       0xA5U
#define FLASH_SPI_TIMEOUT      100000U

static uint8_t flash_bus_error;

static uint8_t flash_wait_flag(uint32_t flag, FlagStatus state)
{
    uint32_t timeout = FLASH_SPI_TIMEOUT;

    while (spi_i2s_flag_get(FLASH_SPI, flag) != state) {
        if (--timeout == 0U) {
            flash_bus_error = 1U;
            return 0U;
        }
    }

    return 1U;
}

void flash_bus_init(void)
{
    spi_parameter_struct spi_init_struct;

    rcu_periph_clock_enable(FLASH_GPIO_CLK);
    rcu_periph_clock_enable(FLASH_SPI_CLK);

    gpio_af_set(FLASH_GPIO_PORT, GPIO_AF_5, FLASH_PIN_SCK | FLASH_PIN_MISO | FLASH_PIN_MOSI);
    gpio_mode_set(FLASH_GPIO_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE,
                  FLASH_PIN_SCK | FLASH_PIN_MISO | FLASH_PIN_MOSI);
    gpio_output_options_set(FLASH_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            FLASH_PIN_SCK | FLASH_PIN_MISO | FLASH_PIN_MOSI);

    gpio_mode_set(FLASH_GPIO_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, FLASH_PIN_CS);
    gpio_output_options_set(FLASH_GPIO_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, FLASH_PIN_CS);
    flash_bus_deselect();

    spi_i2s_deinit(FLASH_SPI);
    spi_init_struct.trans_mode = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode = SPI_MASTER;
    spi_init_struct.frame_size = SPI_FRAMESIZE_8BIT;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_HIGH_PH_2EDGE;
    spi_init_struct.nss = SPI_NSS_SOFT;
    spi_init_struct.prescale = SPI_PSC_4;
    spi_init_struct.endian = SPI_ENDIAN_MSB;
    spi_init(FLASH_SPI, &spi_init_struct);
    spi_enable(FLASH_SPI);
    flash_bus_clear_error();
}

void flash_bus_select(void)
{
    gpio_bit_reset(FLASH_GPIO_PORT, FLASH_PIN_CS);
}

void flash_bus_deselect(void)
{
    gpio_bit_set(FLASH_GPIO_PORT, FLASH_PIN_CS);
}

uint8_t flash_bus_transfer(uint8_t data)
{
    if (flash_wait_flag(SPI_FLAG_TBE, SET) == 0U) {
        return 0U;
    }
    spi_i2s_data_transmit(FLASH_SPI, data);
    if (flash_wait_flag(SPI_FLAG_RBNE, SET) == 0U) {
        return 0U;
    }
    return (uint8_t)spi_i2s_data_receive(FLASH_SPI);
}

void flash_bus_read(uint8_t *data, uint32_t len)
{
    while (len > 0U) {
        *data = flash_bus_transfer(FLASH_DUMMY_BYTE);
        data++;
        len--;
    }
}

void flash_bus_write(const uint8_t *data, uint32_t len)
{
    while (len > 0U) {
        (void)flash_bus_transfer(*data);
        if (flash_bus_ok() == 0U) {
            return;
        }
        data++;
        len--;
    }
    (void)flash_wait_flag(SPI_FLAG_TRANS, RESET);
}

uint8_t flash_bus_ok(void)
{
    return (flash_bus_error == 0U) ? 1U : 0U;
}

void flash_bus_clear_error(void)
{
    flash_bus_error = 0U;
}
