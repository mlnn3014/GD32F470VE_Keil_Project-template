#include "gd30_bsp.h"

#include "gd32f4xx.h"
#include "systick.h"

/* PT100板接在SPI0：PA5=SCK，PA6=MISO，PA7=MOSI，PE8=CS。 */
#define GD30_SPI SPI0
#define GD30_SPI_CLK RCU_SPI0

#define GD30_SPI_PORT GPIOA
#define GD30_SPI_PORT_CLK RCU_GPIOA
#define GD30_PIN_SCK GPIO_PIN_5
#define GD30_PIN_MISO GPIO_PIN_6
#define GD30_PIN_MOSI GPIO_PIN_7

#define GD30_CS_PORT GPIOE
#define GD30_CS_PORT_CLK RCU_GPIOE
#define GD30_PIN_CS GPIO_PIN_8

/* 外部参考电压寄存器，来自厂家示例。 */
#define GD30_PROCESS_REGISTER 0x0012
#define GD30_PROCESS_VALUE 0xACCA
#define GD30_EXTREF_REGISTER 0x0014
#define GD30_EXTREF_ENABLE 0x0040
#define GD30_EXTREF_READ_CMD 0x8106
#define GD30_EXTREF_WRITE_CMD 0x8100
#define GD30_SPI_TIMEOUT 100000

/* CS时序按厂家示例留一点余量。 */
#define GD30_CS_SETUP_MS 1
#define GD30_CS_HOLD_US 100
#define GD30_CS_RECOVERY_MS 10
#define GD30_POWER_ON_SETTLE_MS 5

#define GD30_CONFIG_RETRY 5
#define GD30_CONFIG_VERIFY_MASK 0x7FE8

static uint16_t gd30_extref_register;
static uint16_t gd30_config_register;

static void gd30_delay_us(uint32_t us)
{
    uint32_t loops_per_us = SystemCoreClock / 4000000;
    volatile uint32_t cycles;

    if (loops_per_us == 0)
    {
        loops_per_us = 1;
    }

    cycles = us * loops_per_us;
    while (cycles != 0)
    {
        cycles--;
    }
}

static int gd30_wait_flag(uint32_t flag, FlagStatus state)
{
    uint32_t timeout = GD30_SPI_TIMEOUT;

    while (spi_i2s_flag_get(GD30_SPI, flag) != state)
    {
        if (timeout == 0)
        {
            return -1;
        }
        timeout--;
    }

    return 0;
}

static void gd30_clear_rx(void)
{
    while (spi_i2s_flag_get(GD30_SPI, SPI_FLAG_RBNE) == SET)
    {
        (void)spi_i2s_data_receive(GD30_SPI);
    }
}

static int gd30_transfer16_word(uint16_t value, uint16_t *received)
{
    uint8_t hi = 0;
    uint8_t lo = 0;

    if (received == 0)
    {
        return -1;
    }

    if (gd30_wait_flag(SPI_FLAG_TBE, SET) != 0)
    {
        return -1;
    }
    spi_i2s_data_transmit(GD30_SPI, (uint8_t)(value >> 8));

    if (gd30_wait_flag(SPI_FLAG_TBE, SET) != 0)
    {
        return -1;
    }
    spi_i2s_data_transmit(GD30_SPI, (uint8_t)value);

    if (gd30_wait_flag(SPI_FLAG_RBNE, SET) != 0)
    {
        return -1;
    }
    hi = (uint8_t)spi_i2s_data_receive(GD30_SPI);

    if (gd30_wait_flag(SPI_FLAG_RBNE, SET) != 0)
    {
        return -1;
    }
    lo = (uint8_t)spi_i2s_data_receive(GD30_SPI);

    *received = (uint16_t)(((uint16_t)hi << 8) | (uint16_t)lo);
    return 0;
}

static void gd30_select(void)
{
    gpio_bit_reset(GD30_CS_PORT, GD30_PIN_CS);
    delay_1ms(GD30_CS_SETUP_MS);
}

static void gd30_deselect(void)
{
    gd30_delay_us(GD30_CS_HOLD_US);
    gpio_bit_set(GD30_CS_PORT, GD30_PIN_CS);
    delay_1ms(GD30_CS_RECOVERY_MS);
}

void gd30_bus_init(void)
{
    spi_parameter_struct spi_init_struct;

    rcu_periph_clock_enable(GD30_SPI_PORT_CLK);
    rcu_periph_clock_enable(GD30_CS_PORT_CLK);
    rcu_periph_clock_enable(GD30_SPI_CLK);

    gpio_af_set(GD30_SPI_PORT, GPIO_AF_5, GD30_PIN_SCK | GD30_PIN_MISO | GD30_PIN_MOSI);
    gpio_mode_set(GD30_SPI_PORT, GPIO_MODE_AF, GPIO_PUPD_NONE,
                  GD30_PIN_SCK | GD30_PIN_MISO | GD30_PIN_MOSI);
    gpio_output_options_set(GD30_SPI_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ,
                            GD30_PIN_SCK | GD30_PIN_MISO | GD30_PIN_MOSI);

    gpio_mode_set(GD30_CS_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, GD30_PIN_CS);
    gpio_output_options_set(GD30_CS_PORT, GPIO_OTYPE_PP, GPIO_OSPEED_50MHZ, GD30_PIN_CS);
    gd30_deselect();

    spi_i2s_deinit(GD30_SPI);
    spi_init_struct.trans_mode = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode = SPI_MASTER;
    spi_init_struct.frame_size = SPI_FRAMESIZE_8BIT;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_LOW_PH_2EDGE;
    spi_init_struct.nss = SPI_NSS_SOFT;
    spi_init_struct.prescale = SPI_PSC_32;
    spi_init_struct.endian = SPI_ENDIAN_MSB;
    spi_init(GD30_SPI, &spi_init_struct);
    spi_enable(GD30_SPI);
    gd30_clear_rx();

    delay_1ms(GD30_POWER_ON_SETTLE_MS);
}

static int gd30_write_extended_register(uint16_t address, uint16_t value)
{
    const uint16_t cmd[] = {
        GD30_EXTREF_WRITE_CMD,
        address,
        value,
    };

    if (gd30_transfer16_sequence(cmd, 0, 3) != 0)
    {
        return -1;
    }

    delay_1ms(1);
    return 0;
}

static int gd30_read_extended_register(uint16_t address, uint16_t *value)
{
    const uint16_t cmd[] = {
        GD30_EXTREF_READ_CMD,
        address,
        0x0000,
    };
    uint16_t rx[3] = {
        0,
        0,
        0,
    };

    if (value == 0)
    {
        return -1;
    }

    if (gd30_transfer16_sequence(cmd, rx, 3) != 0)
    {
        return -1;
    }

    delay_1ms(1);

    *value = rx[2];
    return 0;
}

uint8_t gd30_bsp_enable_ain3_reference(void)
{
    uint16_t value;

    if (gd30_write_extended_register(GD30_PROCESS_REGISTER, GD30_PROCESS_VALUE) != 0)
    {
        return 0;
    }

    if (gd30_read_extended_register(GD30_EXTREF_REGISTER, &value) != 0)
    {
        return 0;
    }
    gd30_extref_register = value;

    value = (uint16_t)(value | GD30_EXTREF_ENABLE);
    if (gd30_write_extended_register(GD30_EXTREF_REGISTER, value) != 0)
    {
        return 0;
    }

    if (gd30_read_extended_register(GD30_EXTREF_REGISTER, &value) != 0)
    {
        return 0;
    }
    gd30_extref_register = value;

    return ((value & GD30_EXTREF_ENABLE) != 0) ? 1 : 0;
}

uint16_t gd30_bsp_get_extref_register(void)
{
    return gd30_extref_register;
}

static int gd30_read_config_register(uint16_t config, uint16_t *readback)
{
    const uint16_t tx[2] = {
        config,
        0x0000,
    };
    uint16_t rx[2] = {
        0,
        0,
    };

    if (readback == 0)
    {
        return -1;
    }

    if (gd30_transfer16_sequence(tx, rx, 2) != 0)
    {
        return -1;
    }

    *readback = rx[1];
    return 0;
}

uint8_t gd30_bsp_configure(uint16_t config)
{
    uint32_t attempt;

    for (attempt = 0; attempt < GD30_CONFIG_RETRY; attempt++)
    {
        uint16_t readback = 0;

        if (gd30_read_config_register(config, &readback) == 0)
        {
            gd30_config_register = readback;
            if ((readback & GD30_CONFIG_VERIFY_MASK) == (config & GD30_CONFIG_VERIFY_MASK))
            {
                return 1;
            }
        }
        delay_1ms(2);
    }

    return 0;
}

uint16_t gd30_bsp_get_config_register(void)
{
    return gd30_config_register;
}

int gd30_transfer16(uint16_t tx, uint16_t *rx)
{
    uint16_t received;
    int result = 0;

    if (rx == 0)
    {
        return -1;
    }

    gd30_clear_rx();
    gd30_select();

    if (gd30_transfer16_word(tx, &received) != 0)
    {
        result = -1;
    }
    else if (gd30_wait_flag(SPI_FLAG_TRANS, RESET) != 0)
    {
        result = -1;
    }
    else
    {
        *rx = received;
    }

    gd30_deselect();

    return result;
}

int gd30_transfer16_sequence(const uint16_t *tx, uint16_t *rx, uint32_t count)
{
    int result = 0;

    gd30_clear_rx();
    gd30_select();
    for (uint32_t i = 0; i < count; i++)
    {
        uint16_t value;
        uint16_t received;

        value = 0;
        if (tx != 0)
        {
            value = tx[i];
        }

        if (gd30_transfer16_word(value, &received) != 0)
        {
            result = -1;
            break;
        }

        if (rx != 0)
        {
            rx[i] = received;
        }

        delay_1ms(1);
    }

    if ((result == 0) && (gd30_wait_flag(SPI_FLAG_TRANS, RESET) != 0))
    {
        result = -1;
    }

    gd30_deselect();
    return result;
}
