/*!
    \file    spi_port.c
    \brief   SPI functions port on platform for the gd30ad3344
    
    \version 2024-10-08, V1.0.0, firmware for GD30AD3344
*/

#include "spi_port.h"

/*!
    \brief      initialize SPI0
    \param[in]  none
    \param[out] none
    \retval     none
*/
void ad3344_spi_init()
{ 
    /*!< SPI pins configuration *************************************************/  
    /* SPI0 GPIO config: CS/PA4, SCK/PA5, MISO/PA6, MOSI/PA7 */
    rcu_periph_clock_enable(RCU_GPIOA);
    gpio_init(GPIOA, GPIO_MODE_OUT_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_4);

    rcu_periph_clock_enable(RCU_SPI0);
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7);

    spi_parameter_struct spi_init_struct;
    spi_i2s_deinit(SPI0);
    spi_struct_para_init(&spi_init_struct);

    /* SPI0 parameter config */
    spi_init_struct.trans_mode           = SPI_TRANSMODE_FULLDUPLEX;
    spi_init_struct.device_mode          = SPI_MASTER;
    spi_init_struct.frame_size           = SPI_FRAMESIZE_16BIT;
    spi_init_struct.clock_polarity_phase = SPI_CK_PL_LOW_PH_2EDGE;
    spi_init_struct.nss                  = SPI_NSS_SOFT;
    spi_init_struct.prescale             = SPI_PSC_32;
    spi_init_struct.endian               = SPI_ENDIAN_MSB;
    spi_init(SPI0, &spi_init_struct);

    spi_enable(SPI0);
    
    SPI_SET_CS();
}

/*!
    \brief      SPI transmit and receive 16 bit data
    \param[in]  tx_byte: the data need to be transmit
    \param[out] none
    \retval     the data received from slave
*/
uint16_t ad3344_spi_txrx16bit(uint16_t tx_byte)
{
    /*!< Loop while DR register in not empty */
    while(RESET == spi_i2s_flag_get(SPI0, SPI_FLAG_TBE));
    
    /*!< Send byte through the SPI0 peripheral */
    spi_i2s_data_transmit(SPI0, tx_byte);
    
    /*!< Wait to receive a byte */
    while(RESET == spi_i2s_flag_get(SPI0, SPI_FLAG_RBNE));
    
    /*!< Return the byte read from the SPI bus */
    return spi_i2s_data_receive(SPI0);
}

