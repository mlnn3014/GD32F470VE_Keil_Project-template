/*!
    \file    gd30ad3344.c
    \brief   gd30ad3344 driver

    \version 2024-10-08, V1.0.0, firmware for GD30AD3344
*/

#include "gd30ad3344.h"

/* GD30AD3344 SPI DMA buffers */
extern uint8_t gd30_send_array[ARRAYSIZE];
extern uint8_t gd30_receive_array[ARRAYSIZE];

static void spi_gd30ad3344_config_spi(uint32_t frame_size);
static uint16_t spi_gd30ad3344_send_word_triple_dma(uint16_t first_word, uint16_t second_word, uint16_t third_word,
                                                    uint16_t *first_rx, uint16_t *second_rx);

/**
 * @brief Transfer one byte through SPI by DMA.
 * @param byte Byte to send.
 * @return Byte received from SPI.
 */
uint8_t spi_gd30ad3344_send_byte_dma(uint8_t byte)
{
    gd30_send_array[0] = byte;

    dma_single_data_parameter_struct dma_init_struct;

    dma_deinit(GD30_DMA, GD30_DMA_CHANNEL_TX);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(GD30_SPI);
    dma_init_struct.memory0_addr        = (uint32_t)gd30_send_array;
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_init_struct.number              = 1;
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
    dma_single_data_mode_init(GD30_DMA, GD30_DMA_CHANNEL_TX, &dma_init_struct);
    dma_channel_subperipheral_select(GD30_DMA, GD30_DMA_CHANNEL_TX, GD30_DMA_SUBPERI);

    dma_deinit(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(GD30_SPI);
    dma_init_struct.memory0_addr        = (uint32_t)gd30_receive_array;
    dma_init_struct.direction           = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(GD30_DMA, GD30_DMA_CHANNEL_RX, &dma_init_struct);
    dma_channel_subperipheral_select(GD30_DMA, GD30_DMA_CHANNEL_RX, GD30_DMA_SUBPERI);

    dma_channel_enable(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_channel_enable(GD30_DMA, GD30_DMA_CHANNEL_TX);

    spi_dma_enable(GD30_SPI, SPI_DMA_RECEIVE);
    spi_dma_enable(GD30_SPI, SPI_DMA_TRANSMIT);

    while(RESET == dma_flag_get(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF));

    spi_dma_disable(GD30_SPI, SPI_DMA_RECEIVE);
    spi_dma_disable(GD30_SPI, SPI_DMA_TRANSMIT);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_TX);

    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF);
    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_TX, DMA_FLAG_FTF);

    return gd30_receive_array[0];
}

/**
 * @brief Transfer one halfword through SPI by DMA.
 * @param half_word Halfword to send.
 * @return Halfword received from SPI.
 */
uint16_t spi_gd30ad3344_send_halfword_dma(uint16_t half_word)
{
    GD30_CS_LOW();
    uint16_t rx_data;

    gd30_send_array[0] = (uint8_t)(half_word >> 8);
    gd30_send_array[1] = (uint8_t)half_word;

    dma_single_data_parameter_struct dma_init_struct;

    dma_deinit(GD30_DMA, GD30_DMA_CHANNEL_TX);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(GD30_SPI);
    dma_init_struct.memory0_addr        = (uint32_t)gd30_send_array;
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_init_struct.number              = 2;
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
    dma_single_data_mode_init(GD30_DMA, GD30_DMA_CHANNEL_TX, &dma_init_struct);
    dma_channel_subperipheral_select(GD30_DMA, GD30_DMA_CHANNEL_TX, GD30_DMA_SUBPERI);

    dma_deinit(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(GD30_SPI);
    dma_init_struct.memory0_addr        = (uint32_t)gd30_receive_array;
    dma_init_struct.direction           = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(GD30_DMA, GD30_DMA_CHANNEL_RX, &dma_init_struct);
    dma_channel_subperipheral_select(GD30_DMA, GD30_DMA_CHANNEL_RX, GD30_DMA_SUBPERI);

    dma_channel_enable(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_channel_enable(GD30_DMA, GD30_DMA_CHANNEL_TX);

    spi_dma_enable(GD30_SPI, SPI_DMA_RECEIVE);
    spi_dma_enable(GD30_SPI, SPI_DMA_TRANSMIT);

    while(RESET == dma_flag_get(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF));

    spi_dma_disable(GD30_SPI, SPI_DMA_RECEIVE);
    spi_dma_disable(GD30_SPI, SPI_DMA_TRANSMIT);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_TX);

    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF);
    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_TX, DMA_FLAG_FTF);

    rx_data = (uint16_t)(gd30_receive_array[0] << 8);
    rx_data |= gd30_receive_array[1];
    GD30_CS_HIGH();
    return rx_data;
}

/**
 * @brief Transfer two halfwords in one CS window and return the second halfword.
 * @param first_word First 16-bit word to send.
 * @param second_word Second 16-bit word to send.
 * @return The second received halfword.
 */
uint16_t spi_gd30ad3344_send_word_pair_dma(uint16_t first_word, uint16_t second_word)
{
    uint16_t rx_data;

    GD30_CS_LOW();

    gd30_send_array[0] = (uint8_t)(first_word >> 8);
    gd30_send_array[1] = (uint8_t)first_word;
    gd30_send_array[2] = (uint8_t)(second_word >> 8);
    gd30_send_array[3] = (uint8_t)second_word;

    dma_single_data_parameter_struct dma_init_struct;

    dma_deinit(GD30_DMA, GD30_DMA_CHANNEL_TX);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(GD30_SPI);
    dma_init_struct.memory0_addr        = (uint32_t)gd30_send_array;
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_init_struct.number              = 4;
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
    dma_single_data_mode_init(GD30_DMA, GD30_DMA_CHANNEL_TX, &dma_init_struct);
    dma_channel_subperipheral_select(GD30_DMA, GD30_DMA_CHANNEL_TX, GD30_DMA_SUBPERI);

    dma_deinit(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(GD30_SPI);
    dma_init_struct.memory0_addr        = (uint32_t)gd30_receive_array;
    dma_init_struct.direction           = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(GD30_DMA, GD30_DMA_CHANNEL_RX, &dma_init_struct);
    dma_channel_subperipheral_select(GD30_DMA, GD30_DMA_CHANNEL_RX, GD30_DMA_SUBPERI);

    dma_channel_enable(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_channel_enable(GD30_DMA, GD30_DMA_CHANNEL_TX);

    spi_dma_enable(GD30_SPI, SPI_DMA_RECEIVE);
    spi_dma_enable(GD30_SPI, SPI_DMA_TRANSMIT);

    while(RESET == dma_flag_get(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF));

    spi_dma_disable(GD30_SPI, SPI_DMA_RECEIVE);
    spi_dma_disable(GD30_SPI, SPI_DMA_TRANSMIT);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_TX);

    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF);
    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_TX, DMA_FLAG_FTF);

    rx_data = (uint16_t)(gd30_receive_array[2] << 8);
    rx_data |= gd30_receive_array[3];

    GD30_CS_HIGH();

    return rx_data;
}

static uint16_t spi_gd30ad3344_send_word_triple_dma(uint16_t first_word, uint16_t second_word, uint16_t third_word,
                                                    uint16_t *first_rx, uint16_t *second_rx)
{
    uint16_t third_rx;

    GD30_CS_LOW();

    gd30_send_array[0] = (uint8_t)(first_word >> 8);
    gd30_send_array[1] = (uint8_t)first_word;
    gd30_send_array[2] = (uint8_t)(second_word >> 8);
    gd30_send_array[3] = (uint8_t)second_word;
    gd30_send_array[4] = (uint8_t)(third_word >> 8);
    gd30_send_array[5] = (uint8_t)third_word;

    dma_single_data_parameter_struct dma_init_struct;

    dma_deinit(GD30_DMA, GD30_DMA_CHANNEL_TX);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(GD30_SPI);
    dma_init_struct.memory0_addr        = (uint32_t)gd30_send_array;
    dma_init_struct.direction           = DMA_MEMORY_TO_PERIPH;
    dma_init_struct.periph_memory_width = DMA_PERIPH_WIDTH_8BIT;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_init_struct.number              = 6;
    dma_init_struct.periph_inc          = DMA_PERIPH_INCREASE_DISABLE;
    dma_init_struct.memory_inc          = DMA_MEMORY_INCREASE_ENABLE;
    dma_init_struct.circular_mode       = DMA_CIRCULAR_MODE_DISABLE;
    dma_single_data_mode_init(GD30_DMA, GD30_DMA_CHANNEL_TX, &dma_init_struct);
    dma_channel_subperipheral_select(GD30_DMA, GD30_DMA_CHANNEL_TX, GD30_DMA_SUBPERI);

    dma_deinit(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_init_struct.periph_addr         = (uint32_t)&SPI_DATA(GD30_SPI);
    dma_init_struct.memory0_addr        = (uint32_t)gd30_receive_array;
    dma_init_struct.direction           = DMA_PERIPH_TO_MEMORY;
    dma_init_struct.priority            = DMA_PRIORITY_HIGH;
    dma_single_data_mode_init(GD30_DMA, GD30_DMA_CHANNEL_RX, &dma_init_struct);
    dma_channel_subperipheral_select(GD30_DMA, GD30_DMA_CHANNEL_RX, GD30_DMA_SUBPERI);

    dma_channel_enable(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_channel_enable(GD30_DMA, GD30_DMA_CHANNEL_TX);

    spi_dma_enable(GD30_SPI, SPI_DMA_RECEIVE);
    spi_dma_enable(GD30_SPI, SPI_DMA_TRANSMIT);

    while(RESET == dma_flag_get(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF));

    spi_dma_disable(GD30_SPI, SPI_DMA_RECEIVE);
    spi_dma_disable(GD30_SPI, SPI_DMA_TRANSMIT);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_TX);

    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF);
    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_TX, DMA_FLAG_FTF);

    if (first_rx != NULL) {
        *first_rx = (uint16_t)(gd30_receive_array[0] << 8);
        *first_rx |= gd30_receive_array[1];
    }
    if (second_rx != NULL) {
        *second_rx = (uint16_t)(gd30_receive_array[2] << 8);
        *second_rx |= gd30_receive_array[3];
    }
    third_rx = (uint16_t)(gd30_receive_array[4] << 8);
    third_rx |= gd30_receive_array[5];

    GD30_CS_HIGH();

    return third_rx;
}

static void spi_gd30ad3344_config_spi(uint32_t frame_size)
{
    spi_parameter_struct spi_init_struct;

    spi_dma_disable(GD30_SPI, SPI_DMA_RECEIVE);
    spi_dma_disable(GD30_SPI, SPI_DMA_TRANSMIT);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_RX);
    dma_channel_disable(GD30_DMA, GD30_DMA_CHANNEL_TX);

    spi_i2s_deinit(GD30_SPI);
    spi_struct_para_init(&spi_init_struct);

    spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode          = SPI_MASTER;
    spi_init_struct.frame_size           = frame_size;
    spi_init_struct.clock_polarity_phase = GD30_SPIMODE;
    spi_init_struct.nss                  = SPI_NSS_SOFT;
    spi_init_struct.prescale             = SPI_PSC_256;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;
    spi_init(GD30_SPI, &spi_init_struct);
    spi_enable(GD30_SPI);
    (void)spi_i2s_data_receive(GD30_SPI);
}

/**
 * @brief Wait until the GD30AD3344 DMA transfer is complete.
 */
void spi_gd30ad3344_wait_for_dma_end(void)
{
    while(RESET == dma_flag_get(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF));

    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_RX, DMA_FLAG_FTF);
    dma_flag_clear(GD30_DMA, GD30_DMA_CHANNEL_TX, DMA_FLAG_FTF);
}

GD30AD3344 GD30AD3344_InitStruct;
uint16_t GD30AD3344_RawCode = 0U;

/**
 * @brief Initialize the GD30AD3344 configuration register.
 */
void GD30AD3344_Init(void)
{
    GD30AD3344_InitStruct.SS         = GD30AD3344_OS_DISABLE;
    GD30AD3344_InitStruct.MUX        = GD30AD3344_MUX_AIN0_GND;
                                                // AIN0~AIN1 AIN0~AIN3 AIN1~AIN3 AIN2~AIN3 AIN0~GND AIN1~GND AIN2~GND AIN3~GND
    GD30AD3344_InitStruct.PGA        = GD30AD3344_PGA_2V048;
                                                // +/-6.144V +/-4.096V +/-2.048V +/-1.024V +/-0.512V +/-0.256V +/-0.256V +/-0.256V
    GD30AD3344_InitStruct.MODE       = GD30AD3344_MODE_CONTINUOUS;
    GD30AD3344_InitStruct.DR         = GD30AD3344_DR_25SPS;
                                                // 6.25SPS 12.5SPS 25SPS 50SPS 100SPS 250SPS 500SPS 1000SPS
    GD30AD3344_InitStruct.RESERVED_1 = GD30AD3344_RESERVED_0;
    GD30AD3344_InitStruct.PULL_UP_EN = GD30AD3344_PULL_UP_ENABLE;
    GD30AD3344_InitStruct.NOP        = GD30AD3344_NOP_VALID_UPDATE;
    GD30AD3344_InitStruct.RESERVED   = GD30AD3344_RESERVED_1;
    
    spi_enable(GD30_SPI);
    spi_gd30ad3344_send_halfword_dma(GD30AD3344_InitStruct_Value);
    my_printf(DEBUG_USART, "0x%4X\r\n", GD30AD3344_InitStruct_Value);
}

/**
 * @brief Read the current GD30AD3344 configuration register value.
 * @return Raw configuration register value.
 */
uint16_t GD30AD3344_Read_ConfigRegister(void)
{
    GD30AD3344_InitStruct.NOP = GD30AD3344_NOP_VALID_NO_UPDATE;
    return spi_gd30ad3344_send_word_pair_dma(GD30AD3344_InitStruct_Value, GD30AD3344_InitStruct_Value);
}

/**
 * @brief Enable the GD30AD3344 external reference register bit.
 * @return Register value read back after the write.
 */
uint16_t GD30AD3344_ConfigExternalReference(void)
{
    uint16_t reg_addr = GD30AD3344_EXTREF_REGISTER_ADDR;
    uint16_t proc_cmd;
    uint16_t proc_addr;
    uint16_t proc_data;
    uint16_t read_cmd;
    uint16_t read_addr;
    uint16_t read_value;
    uint16_t write_value;
    uint16_t write_cmd;
    uint16_t write_addr;
    uint16_t write_data;
    uint16_t verify_cmd;
    uint16_t verify_addr;
    uint16_t verify_value;

    spi_gd30ad3344_config_spi(SPI_FRAMESIZE_8BIT);

    proc_data = spi_gd30ad3344_send_word_triple_dma(GD30AD3344_EXTREG_WRITE_CMD,
                                                    GD30AD3344_EXTPROC_REGISTER_ADDR,
                                                    GD30AD3344_EXTPROC_UNLOCK_VALUE,
                                                    &proc_cmd,
                                                    &proc_addr);
    delay_1ms(1);

    read_value = spi_gd30ad3344_send_word_triple_dma(GD30AD3344_EXTREG_READ_CMD,
                                                     reg_addr,
                                                     0x0000U,
                                                     &read_cmd,
                                                     &read_addr);
    delay_1ms(1);

    write_value = read_value | GD30AD3344_EXTREF_ENABLE_BIT;

    write_data = spi_gd30ad3344_send_word_triple_dma(GD30AD3344_EXTREG_WRITE_CMD,
                                                     reg_addr,
                                                     write_value,
                                                     &write_cmd,
                                                     &write_addr);
    delay_1ms(1);

    verify_value = spi_gd30ad3344_send_word_triple_dma(GD30AD3344_EXTREG_READ_CMD,
                                                       reg_addr,
                                                       0x0000U,
                                                       &verify_cmd,
                                                       &verify_addr);

//    my_printf(DEBUG_USART, "GD30 proc rx:%04X %04X %04X\r\n", proc_cmd, proc_addr, proc_data);
//    my_printf(DEBUG_USART, "GD30 rd1  rx:%04X %04X %04X\r\n", read_cmd, read_addr, read_value);
//    my_printf(DEBUG_USART, "GD30 wr   rx:%04X %04X %04X\r\n", write_cmd, write_addr, write_data);
//    my_printf(DEBUG_USART, "GD30 rd2  rx:%04X %04X %04X\r\n", verify_cmd, verify_addr, verify_value);
//    my_printf(DEBUG_USART, "GD30 ext ref reg:0x%04X old:0x%04X write:0x%04X read:0x%04X\r\n",
//              reg_addr, read_value, write_value, verify_value);

    return verify_value;
}

float ADS118_PGA_SET(GD30AD3344_PGA_TypeDef PGA)
{
    switch(PGA) {
    case GD30AD3344_PGA_6V144:
        return 6.144f;
    case GD30AD3344_PGA_4V096:
        return 4.096f;
    case GD30AD3344_PGA_2V048:
        return 2.048f;
    case GD30AD3344_PGA_1V024:
        return 1.024f;
    case GD30AD3344_PGA_0V512:
        return 0.512f;
    case GD30AD3344_PGA_0V256:
        return 0.256f;
    case GD30AD3344_PGA_0V064:
        return 0.064f;
    default:
        return 2.048f;
    }
}

/**
 * @brief Read one ADC channel and convert the signed raw code to volts.
 * @param CH ADC input channel.
 * @param Ref PGA full-scale range.
 * @return Converted voltage in volts.
 */
float GD30AD3344_AD_Read(GD30AD3344_Channel_TypeDef CH, GD30AD3344_PGA_TypeDef Ref)
{
    uint16_t raw_data;
    float result = 0.0;

    GD30AD3344_InitStruct.MUX = CH;
    GD30AD3344_InitStruct.PGA = Ref;

    raw_data = spi_gd30ad3344_send_halfword_dma(GD30AD3344_InitStruct_Value);
    GD30AD3344_RawCode = raw_data;
    
    result = (float)((int16_t)raw_data) * 2.055f / 32767.0f;
    // result = (float)((int16_t)raw_data) * ADS118_PGA_SET(Ref) / 32767.0f;

    return (float)result;
}
