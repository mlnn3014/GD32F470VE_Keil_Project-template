/*!
    \file    gd30ad3344.h
    \brief   definitions for the gd30ad3344
    
    \version 2024-6-27, V1.0.0, firmware for GD30AD3344
*/

#ifndef __GD30AD3344__H
#define __GD30AD3344__H

#ifdef __cplusplus
extern "C" {
#endif

#include "gd32f30x.h"
#include "spi_port.h"
#include "spi_def.h"

extern uint16_t AD3344_CONFIG;

/*=========================================================================
 * Auxiliary Definition
 */
    #define AD3344_DUAL_END                                 (0)
    #define AD3344_SINGLE_END                               (1)
/*========================================================================= */

/*=========================================================================
    Reset Value for Module GD30AD3344
    -----------------------------------------------------------------------*/
#define AD3344_CONVERSION_RESET                             ((uint32_t)0x0)
#define AD3344_CONFIG_RESET                                 ((uint32_t)0x58b)
/*=========================================================================*/

/******************  Bit Definition for Register CONVERSION  *********************/
#define AD3344_CONVERSION_CNVDATA_Msk               ((uint32_t)0xffff)      /* bit mask, */
#define AD3344_CONVERSION_CNVDATA_Pos               ((uint32_t)0)           /*bit position, */

/******************  Bit Definition for Register CONFIG  *********************/
#define AD3344_CONFIG_NOP_Msk                       ((uint32_t)0x6)         /* bit mask, GD30AD3344的NOP域*/
#define AD3344_CONFIG_NOP_Pos                       ((uint32_t)1)           /*bit position, GD30AD3344的NOP域*/
#define AD3344_CONFIG_PULL_UP_EN_Msk                ((uint32_t)0x8)         /* bit mask, DOUT上拉使能*/
#define AD3344_CONFIG_PULL_UP_EN_Pos                ((uint32_t)3)           /*bit position, DOUT上拉使能*/
#define AD3344_CONFIG_DR_Msk                        ((uint32_t)0xe0)        /* bit mask, Data rate*/
#define AD3344_CONFIG_DR_Pos                        ((uint32_t)5)           /*bit position, Data rate*/
#define AD3344_CONFIG_MODE_Msk                      ((uint32_t)0x100)       /* bit mask, Device operating mode*/
#define AD3344_CONFIG_MODE_Pos                      ((uint32_t)8)           /*bit position, Device operating mode*/
#define AD3344_CONFIG_PGA_Msk                       ((uint32_t)0xe00)       /* bit mask, Programmable gain amplifier configuration*/
#define AD3344_CONFIG_PGA_Pos                       ((uint32_t)9)           /*bit position, Programmable gain amplifier configuration*/
#define AD3344_CONFIG_MUX_Msk                       ((uint32_t)0x7000)      /* bit mask, input multiplexer configuration*/
#define AD3344_CONFIG_MUX_Pos                       ((uint32_t)12)          /*bit position, input multiplexer configuration*/
#define AD3344_CONFIG_OS_Msk                        ((uint32_t)0x8000)      /* bit mask, This bit is used to start a single conversion..OS can only be written when in
                                                                               power-down state and has no effect when a conversion is ongoing.Always reads back 0 (default).*/
#define AD3344_CONFIG_OS_Pos                        ((uint32_t)15)          /*bit position, This bit is used to start a single conversion..OS can only be written when in
                                                                               power-down state and has no effect when a conversion is ongoing.Always reads back 0 (default).*/

/* Register 0x00 (CONVERSION) definition
 * ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * |  Bit 15  |  Bit 14  |  Bit 13  |  Bit 12  |  Bit 11  |  Bit 10  |   Bit 9  |   Bit 8  |   Bit 7  |   Bit 6  |   Bit 5  |   Bit 4  |   Bit 3  |   Bit 2  |   Bit 1  |   Bit 0  |
 * ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * |                                                                                    CONV[15:0]                                                                                   |
 * ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 */

    /* CONVERSION register address */
    #define AD3344_CONVERSION_ADDRESS                                              ((uint8_t) 0x00)

    /* CONVERSION default (reset) value */
    #define AD3344_CONVERSION_DEFAULT                                              ((uint16_t) 0x0000)

    /* CONVERSION register field masks */
    #define AD3344_CONVERSION_CONV_MASK                                            ((uint16_t) 0xFFFF)


/* Register 0x01 (CONFIG) definition
 * ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * |  Bit 15  |  Bit 14  |  Bit 13  |  Bit 12  |  Bit 11  |  Bit 10  |   Bit 9  |   Bit 8  |   Bit 7  |   Bit 6  |   Bit 5  |   Bit 4  |   Bit 3  |   Bit 2  |   Bit 1  |   Bit 0  |
 * ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 * |    SS    |            MUX[2:0]            |            PGA[2:0]            |   MODE   |             DR[2:0]            | RESERVED |PULL_UP_EN|       NOP[1:0]      | RESERVED |
 * ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
 */

    #define AD3344_CONFIG_ADDRESS                                                  ((uint8_t) 0x01)

    /* CONFIG default (reset) value */
    #define AD3344_CONFIG_DEFAULT                                                  ((uint16_t) 0x058b)

    #define AD3344_REG_CONFIG_OS_MASK                       (0x8000)
    #define AD3344_REG_CONFIG_OS_SINGLE                     (0x8000)        // Write: Set to start a single-conversion
    #define AD3344_REG_CONFIG_OS_BUSY                       (0x0000)        // Read: Bit = 0 when conversion is in progress
    #define AD3344_REG_CONFIG_OS_NOTBUSY                    (0x8000)        // Read: Bit = 1 when device is not performing a conversion

    #define AD3344_REG_CONFIG_MUX_MASK                      (0x7000)
    #define AD3344_REG_CONFIG_MUX_DIFF_0_1                  (0x0000)        // Differential P = AIN0, N = AIN1 (default)
    #define AD3344_REG_CONFIG_MUX_DIFF_0_3                  (0x1000)        // Differential P = AIN0, N = AIN3
    #define AD3344_REG_CONFIG_MUX_DIFF_1_3                  (0x2000)        // Differential P = AIN1, N = AIN3
    #define AD3344_REG_CONFIG_MUX_DIFF_2_3                  (0x3000)        // Differential P = AIN2, N = AIN3
    #define AD3344_REG_CONFIG_MUX_SINGLE_0                  (0x4000)        // Single-ended AIN0
    #define AD3344_REG_CONFIG_MUX_SINGLE_1                  (0x5000)        // Single-ended AIN1
    #define AD3344_REG_CONFIG_MUX_SINGLE_2                  (0x6000)        // Single-ended AIN2
    #define AD3344_REG_CONFIG_MUX_SINGLE_3                  (0x7000)        // Single-ended AIN3

    #define AD3344_REG_CONFIG_PGA_MASK                      (0x0E00)
    #define AD3344_REG_CONFIG_PGA_6_144V                    (0x0000)        // +/-6.144V range = Gain 2/3
    #define AD3344_REG_CONFIG_PGA_4_096V                    (0x0200)        // +/-4.096V range = Gain 1
    #define AD3344_REG_CONFIG_PGA_2_048V                    (0x0400)        // +/-2.048V range = Gain 2 (default)
    #define AD3344_REG_CONFIG_PGA_1_024V                    (0x0600)        // +/-1.024V range = Gain 4
    #define AD3344_REG_CONFIG_PGA_0_512V                    (0x0800)        // +/-0.512V range = Gain 8
    #define AD3344_REG_CONFIG_PGA_0_256V                    (0x0A00)        // +/-0.256V range = Gain 16
    #define AD3344_REG_CONFIG_PGA_0_064V                    (0x0C00)        // +/-0.064V range = Gain 32

    #define AD3344_REG_CONFIG_MODE_MASK                     (0x0100)
    #define AD3344_REG_CONFIG_MODE_CONTIN                   (0x0000)        // Continuous conversion mode
    #define AD3344_REG_CONFIG_MODE_SINGLE                   (0x0100)        // Power-down single-shot mode (default)

    #define AD3344_REG_CONFIG_DR_MASK                       (0x00E0)
    #define AD3344_REG_CONFIG_DR_6_25SPS                    (0x0000)        // 6.25 samples per second
    #define AD3344_REG_CONFIG_DR_12_5SPS                    (0x0020)        // 12.5 samples per second
    #define AD3344_REG_CONFIG_DR_25SPS                      (0x0040)        // 25 samples per second
    #define AD3344_REG_CONFIG_DR_50SPS                      (0x0060)        // 50 samples per second
    #define AD3344_REG_CONFIG_DR_100SPS                     (0x0080)        // 100 samples per second (default)
    #define AD3344_REG_CONFIG_DR_250SPS                     (0x00A0)        // 250 samples per second
    #define AD3344_REG_CONFIG_DR_500SPS                     (0x00C0)        // 500 samples per second
    #define AD3344_REG_CONFIG_DR_1000SPS                    (0x00E0)        // 1000 samples per second

    #define AD3344_REG_CONFIG_PULL_UP_EN_MASK               (0x0008)
    #define AD3344_REG_CONFIG_PULL_UP_DIS                   (0x0000)        // Pullup resistor disabled on DOUT/DRDY pin
    #define AD3344_REG_CONFIG_PULL_UP_EN                    (0x0008)        // Pullup resistor enabled on DOUT/DRDY pin (default)

    #define AD3344_REG_CONFIG_NOP_MASK                      (0x0006)
    #define AD3344_REG_CONFIG_NOP_INV_0                     (0x0000)        // Invalid data, do not update the contents of the Config register
    #define AD3344_REG_CONFIG_NOP_VALID                     (0x0002)        // Valid data, update the Config register (default)
    #define AD3344_REG_CONFIG_NOP_INV_1                     (0x0004)        // Invalid data, do not update the contents of the Config register
    #define AD3344_REG_CONFIG_NOP_INV_2                     (0x0006)        // Invalid data, do not update the contents of the Config register
    
    #define AD3344_CONFIG_RESERVED_MASK                            ((uint16_t) 0x0001)
    #define AD3344_RESERVED_VALUE                                  ((uint16_t) 0x0001)


/*****************************************************************************************
                                API 
*****************************************************************************************/
/* delay us */
void delay_us(uint32_t t);
/* exti-line enable (PA6) */
void ad3344_Exti_enable(void);
/* exti-line disable */
void ad3344_Exti_disable(void);
/* GD30AD3344 transmit data */
uint16_t AD3344_Send_Data(uint16_t config_d);
/* GD30AD3344 Config Register(32bit trans) */
uint16_t ad3344_read_data32(uint16_t config_d, uint16_t *config);
/* GD30AD3344 Config Register(16bit trans) */
uint16_t ad3344_read_data16(uint16_t config_d);
/* GD30AD3344 Read Register */
uint16_t ad3344_read_regs(uint8_t addr);
void ad3344_process(void);
void ad3344_ExtRef(void);
/* GD30AD3344 Init */
void ad3344_init(uint16_t config_d);
/* GD30AD3344 stop conversion */
void ad3344_stop_conver(void);
/* GD30AD3344 reset */
void ad3344_reset(void);

#ifdef __cplusplus
}
#endif //__cplusplus
#endif //__GD30AD3344__H
