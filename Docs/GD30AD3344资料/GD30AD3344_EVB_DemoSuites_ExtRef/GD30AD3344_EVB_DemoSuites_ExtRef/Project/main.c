/*!
    \file    main.c
    \brief   GD30AD3344 test demo

    \version 2024-10-08, V1.0.0, demo for GD30AD3344
*/

#include "gd32f30x.h"
#include "systick.h"
#include <stdio.h>
#include "gd30ad3344.h"
#include "uart_print.h"
#include "main.h"

int16_t adc_16bit;
uint16_t register_data;
float ADC_Value;
float pga;
uint8_t i;
uint32_t sample_count;
uint32_t number_of_sample = 1;

#define POSITIVE_FS 0x7FFF
#define NEGATIVE_FS 0x8000
#define ADC_DATA    32768

/*!
    \brief      GD30AD3344 Config Register
    \param[in]  TnputMUX
                only one parameter can be selected which is shown as below:
      \arg      AD3344_SINGLE_END
      \arg      AD3344_DUAL_END
    \param[in]  Channel
                only one parameter can be selected which is shown as below:
      \arg      0: AD3344_REG_CONFIG_MUX_DIFF_0_1(AD3344_DUAL_END) or AD3344_REG_CONFIG_MUX_SINGLE_0(AD3344_SINGLE_END)
      \arg      1: AD3344_REG_CONFIG_MUX_DIFF_0_3(AD3344_DUAL_END) or AD3344_REG_CONFIG_MUX_SINGLE_1(AD3344_SINGLE_END)
      \arg      2: AD3344_REG_CONFIG_MUX_DIFF_1_3(AD3344_DUAL_END) or AD3344_REG_CONFIG_MUX_SINGLE_2(AD3344_SINGLE_END)
      \arg      3: AD3344_REG_CONFIG_MUX_DIFF_2_3(AD3344_DUAL_END) or AD3344_REG_CONFIG_MUX_SINGLE_3(AD3344_SINGLE_END)
    \param[out] none
    \retval     none
*/
void AD3344_reg_Config(uint8_t InputMUX, uint8_t Channel)
{
    if(InputMUX == AD3344_DUAL_END)
    {
        switch (Channel)
        {
        case (0):
          AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_DIFF_0_1;
          break;
        case (1):
          AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_DIFF_0_3;
          break;
        case (2):
          AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_DIFF_1_3;
          break;
        case (3):
          AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_DIFF_2_3;
          break;
        }
    }else if(InputMUX == AD3344_SINGLE_END)
    {
        switch (Channel)
        {
        case (0):
          AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_SINGLE_0;
          break;
        case (1):
          AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_SINGLE_1;
          break;
        case (2):
          AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_SINGLE_2;
          break;
        case (3):
          AD3344_CONFIG |= AD3344_REG_CONFIG_MUX_SINGLE_3;
          break;
        }
    }
    
    AD3344_CONFIG |= AD3344_REG_CONFIG_DR_1000SPS;
    AD3344_CONFIG |= AD3344_REG_CONFIG_PULL_UP_EN;
    AD3344_CONFIG |= AD3344_REG_CONFIG_NOP_VALID;
    
    AD3344_CONFIG |= AD3344_REG_CONFIG_PGA_4_096V;
    
    if((AD3344_CONFIG&AD3344_REG_CONFIG_PGA_MASK) == AD3344_REG_CONFIG_PGA_6_144V){
        pga = 6.144;
    }else if((AD3344_CONFIG&AD3344_REG_CONFIG_PGA_MASK) == AD3344_REG_CONFIG_PGA_4_096V){
        pga = 4.096;
    }else if((AD3344_CONFIG&AD3344_REG_CONFIG_PGA_MASK) == AD3344_REG_CONFIG_PGA_2_048V){
        pga = 2.048;
    }else if((AD3344_CONFIG&AD3344_REG_CONFIG_PGA_MASK) == AD3344_REG_CONFIG_PGA_1_024V){
        pga = 1.024;
    }else if((AD3344_CONFIG&AD3344_REG_CONFIG_PGA_MASK) == AD3344_REG_CONFIG_PGA_0_512V){
        pga = 0.512;
    }else if((AD3344_CONFIG&AD3344_REG_CONFIG_PGA_MASK) == AD3344_REG_CONFIG_PGA_0_256V){
        pga = 0.256;
    }else{
        pga = 0.064;
    }
    
    #ifdef CONTINUOUS_CONVERSION
        AD3344_CONFIG |= AD3344_REG_CONFIG_MODE_CONTIN;
    #else
        AD3344_CONFIG |= AD3344_REG_CONFIG_MODE_SINGLE;
        /* Set 'start single-conversion' bit */
        AD3344_CONFIG |= AD3344_REG_CONFIG_OS_SINGLE;
    #endif
}

/*!
    \brief      main function
    \param[in]  none
    \param[out] none
    \retval     none
*/
int main(void)
{
    uart_print_init();
    
    /* GD30AD3344 config */
    ad3344_spi_init();
    
    /* enable AIN3 as extern ref */
    ad3344_process();
    ad3344_ExtRef();
    
    AD3344_reg_Config(AD3344_SINGLE_END, 0);
    ad3344_init(AD3344_CONFIG);

    printf("start conversion!\n\r");
    
    ad3344_Exti_enable();
    
    while(1) {
        
    }
}

/*!
    \brief      adc data conversion to voltage
    \param[in]  adc data
    \param[out] none
    \retval     voltage
*/
float adcdata_to_volt(uint16_t bin)
{
    int  _val;
    float adcValue;
    
    if(bin == NEGATIVE_FS){
        adcValue = -(pga*bin/ADC_DATA);
    }else{
        _val     =  bin&NEGATIVE_FS ?  (-((~bin+1)&POSITIVE_FS)):bin;
        adcValue = pga*_val/ADC_DATA;
    }
    
    return adcValue;
}

/*!
    \brief      this function handles EXTI5_9 exception
    \param[in]  none
    \param[out] none
    \retval     none
*/
void EXTI5_9_IRQHandler()
{
    if(RESET != exti_interrupt_flag_get(EXTI_6))
    {
        /* Clear the EXTI line 6 pending bit */
        exti_interrupt_flag_clear(EXTI_6);
        ad3344_Exti_disable();

        /* read ADC data */
        if(number_of_sample > Adc_Sample_Count){
            ad3344_stop_conver();
            ad3344_Exti_disable();
            usart_interrupt_disable(USART0, USART_INT_RBNE);
            
            printf("sampling over!\n\r");
        }else{
            adc_16bit = ad3344_read_data16(AD3344_CONFIG);
            ADC_Value = adcdata_to_volt(adc_16bit);
            
            printf("%d:  0x%04x  ADC_VALUE=%.4f\n",number_of_sample, adc_16bit, ADC_Value);
            number_of_sample++;
            ad3344_Exti_enable();
        }

        adc_16bit = 0;
        register_data = 0;
        ADC_Value = 0;
    }
}

#ifdef __ICCARM__
int putc(char c)
{
    usart_data_transmit(USART0,c);
    while(RESET == usart_flag_get(USART0, USART_FLAG_TBE));
    return c;
}
#else 
int fputc(int ch, FILE *f)
{
    usart_data_transmit(USART0,ch);
    while(RESET == usart_flag_get(USART0, USART_FLAG_TBE));
    return ch;

}
#endif
