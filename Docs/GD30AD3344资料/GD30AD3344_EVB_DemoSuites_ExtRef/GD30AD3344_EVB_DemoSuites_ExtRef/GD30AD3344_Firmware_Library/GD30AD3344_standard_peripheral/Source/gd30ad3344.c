/*!
    \file    gd30ad3344.c
    \brief   gd30ad3344 driver
    
    \version 2024-10-08, V1.0.0, firmware for GD30AD3344
*/

#include "gd30ad3344.h"
#include "spi_port.h"
#include "main.h"

uint16_t ADC_Config[2]={0}; 
uint16_t AD3344_CONFIG;

/*!
    \brief      delay us
    \param[in]  t: delay time
    \param[out] none
    \retval     none
*/
void delay_us(uint32_t t)
{
    uint16_t i;
    while (t--){
         i = 10;
         while(i--);
   }
}

/*!
    \brief      exti-line enable (PA6)
    \param[in]  none
    \param[out] none
    \retval     none
*/
void ad3344_Exti_enable(void)
{
    rcu_periph_clock_enable(RCU_GPIOA);
    rcu_periph_clock_enable(RCU_AF);
    
    gpio_init(GPIOA, GPIO_MODE_IN_FLOATING, GPIO_OSPEED_50MHZ, GPIO_PIN_6);
    /* connect key wakeup EXTI line to key GPIO pin */
    gpio_exti_source_select(GPIO_PORT_SOURCE_GPIOA, GPIO_PIN_SOURCE_6);
    /* configure key wakeup EXTI line */
    exti_init(EXTI_6, EXTI_INTERRUPT, EXTI_TRIG_FALLING);
    exti_interrupt_flag_clear(EXTI_6);
    
    nvic_irq_enable(EXTI5_9_IRQn, 2U, 0U);
}

/*!
    \brief      exti-line disable
    \param[in]  none
    \param[out] none
    \retval     none
*/
void ad3344_Exti_disable(void)
{
    nvic_irq_disable(EXTI5_9_IRQn);
    exti_interrupt_flag_clear(EXTI_6);
    exti_interrupt_disable(EXTI_6);
    
    rcu_periph_clock_enable(RCU_SPI0);
    gpio_init(GPIOA, GPIO_MODE_AF_PP, GPIO_OSPEED_50MHZ, GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7);
}

/*!
    \brief      GD30AD3344 transmit data
    \param[in]  config_d: register value
    \param[out] none
    \retval     the read value of register
*/
uint16_t AD3344_Send_Data(uint16_t config_d)
{
    uint16_t Data;

    Data = ad3344_spi_txrx16bit(config_d);
    
    return (Data);
}

/*!
    \brief      GD30AD3344 Config Register(32bit trans)
    \param[in]  config_d: the data need to be tramit
    \param[in]  *config: Register readback value
    \param[out] none
    \retval     the read value of register
*/
uint16_t ad3344_read_data32(uint16_t config_d, uint16_t *config)
{
    uint16_t data;
    
    data = AD3344_Send_Data(config_d);
    *config = AD3344_Send_Data(0);
    
    return (data);
}

/*!
    \brief      GD30AD3344 Config Register(16bit trans)
    \param[in]  config_d: the data need to be tramit
    \param[out] none
    \retval     the read value of register
*/
uint16_t ad3344_read_data16(uint16_t config_d)
{
    uint16_t data;
    
    SPI_CLR_CS();
    delay_us(1000);
    
    data = AD3344_Send_Data(config_d);
    
    SPI_SET_CS();
    delay_us(10000);
    
    SPI_CLR_CS();
    
    return (data);
}

/*!
    \brief      GD30AD3344 Read Register
    \param[in]  addr
      \arg      0x01: Config Register
    \param[out] none
    \retval     the read value of register
*/
uint16_t ad3344_read_regs(uint8_t addr)
{
    uint8_t reg_addr = addr;
    uint16_t reg_rtu = 0;

    SPI_CLR_CS();
    delay_us(1000);
    
    AD3344_Send_Data(reg_addr);
    
    reg_rtu = AD3344_Send_Data(0x00);
    
    SPI_SET_CS();
    delay_us(10000);
    
    SPI_CLR_CS();


    return reg_rtu;
}

void ad3344_process(void)
{
    uint16_t addr,val;
    uint16_t tx_data;
    
    addr = 0x10 + 0x02;
    val = 0xACCA;
    
    SPI_CLR_CS();
    delay_us(1000);
    
    tx_data = 0x8100;
    ad3344_spi_txrx16bit(tx_data);
    
    tx_data = addr;
    ad3344_spi_txrx16bit(tx_data);
    
    tx_data = val;
    ad3344_spi_txrx16bit(tx_data);
    delay_us(1000);
    
    SPI_SET_CS();
    delay_us(1000);
}

void ad3344_ExtRef(void)
{
    uint16_t addr,val,rdval;
    uint16_t tx_data;
    
    addr = 0x10 + 0x4;
    
    SPI_CLR_CS();
    delay_us(1000);
    
    tx_data = 0x8106;
    ad3344_spi_txrx16bit(tx_data);
    delay_us(1000);
    
    tx_data = addr;
    ad3344_spi_txrx16bit(tx_data);
    delay_us(1000);
    
    rdval = ad3344_spi_txrx16bit(0x00);
    delay_us(1000);
    
    SPI_SET_CS();
    delay_us(1000);
    
    val = rdval | 0x40;
    
    SPI_CLR_CS();
    delay_us(1000);
    
    tx_data = 0x8100;
    ad3344_spi_txrx16bit(tx_data);
    
    tx_data = addr;
    ad3344_spi_txrx16bit(tx_data);
    
    tx_data = val;
    ad3344_spi_txrx16bit(tx_data);
    delay_us(1000);
    
    SPI_SET_CS();
    delay_us(1000);
}

/*!
    \brief      GD30AD3344 Init
    \param[in]  none
    \param[out] none
    \retval     none
*/
void ad3344_init(uint16_t config_d)
{
    SPI_CLR_CS();
    delay_us(1000);
    
    #ifdef BIT32_TRANS_CYCLE
    ad3344_read_data32(config_d, ADC_Config);
    #else
    ad3344_spi_txrx16bit(config_d);
    #endif
    
    delay_us(100);
    
    SPI_SET_CS();
    delay_us(1000);

    SPI_CLR_CS();
    delay_us(1000);
}

/*!
    \brief      GD30AD3344 stop conversion
    \param[in]  none
    \param[out] none
    \retval     none
*/
void ad3344_stop_conver()
{
    AD3344_CONFIG |= AD3344_REG_CONFIG_MODE_SINGLE;
    
    #ifdef BIT32_TRANS_CYCLE
    ad3344_read_data32(AD3344_CONFIG, ADC_Config);
    #else
    ad3344_spi_txrx16bit(AD3344_CONFIG);
    #endif
}

/*!
    \brief      GD30AD3344 reset
    \param[in]  none
    \param[out] none
    \retval     the result of the conversion
*/
void ad3344_reset()
{
    #ifdef BIT32_TRANS_CYCLE
    ad3344_read_data32(AD3344_CONFIG_DEFAULT, ADC_Config);
    #else
    ad3344_spi_txrx16bit(AD3344_CONFIG_DEFAULT);
    #endif
}
