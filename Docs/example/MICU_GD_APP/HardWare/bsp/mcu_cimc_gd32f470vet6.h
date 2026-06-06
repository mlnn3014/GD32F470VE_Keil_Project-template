/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2025/06/05
* Note:
*/
#ifndef MCU_CIMC_GD32F470VET6_H
#define MCU_CIMC_GD32F470VET6_H

#include "gd32f4xx.h"
#include "gd32f4xx_dma.h"
#include "systick.h"

#include "ebtn.h"
#include "oled.h"
#include "gd25qxx.h"
#include "gd30ad3344.h"

#include "flash_fs_app.h"
#include "led_app.h"
#include "adc_app.h"
#include "oled_app.h"
#include "usart_app.h"
#include "rtc_app.h"
#include "btn_app.h"
#include "scheduler.h"
#include "ota_uart.h"
#include "driver_rs485.h"
#include "cimc_app.h"

#include "perf_counter.h"

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif
/***************************************************************************************************************/
/* LED IO map
 * LED1 -> PD8
 * LED2 -> PD9
 * LED3 -> PD10
 * LED4 -> PD11
 * LED5 -> PD12
 * LED6 -> PD13
 */
#define LED_PORT        GPIOD
#define LED_CLK_PORT    RCU_GPIOD

#define LED1_PIN        GPIO_PIN_8     /* PD8  */
#define LED2_PIN        GPIO_PIN_9     /* PD9  */
#define LED3_PIN        GPIO_PIN_10    /* PD10 */
#define LED4_PIN        GPIO_PIN_11    /* PD11 */
#define LED5_PIN        GPIO_PIN_12    /* PD12 */
#define LED6_PIN        GPIO_PIN_13    /* PD13 */

/* LED active level config: 1 = active-high, 0 = active-low */
#define LED_ACTIVE_HIGH 0

#if LED_ACTIVE_HIGH
#define LED_WRITE(pin, on) do { if(on) GPIO_BOP(LED_PORT) = (pin); else GPIO_BC(LED_PORT) = (pin); } while(0)
#else
#define LED_WRITE(pin, on) do { if(on) GPIO_BC(LED_PORT) = (pin); else GPIO_BOP(LED_PORT) = (pin); } while(0)
#endif

#define LED1_SET(x)     do { LED_WRITE(LED1_PIN, (x)); } while(0)
#define LED2_SET(x)     do { LED_WRITE(LED2_PIN, (x)); } while(0)
#define LED3_SET(x)     do { LED_WRITE(LED3_PIN, (x)); } while(0)
#define LED4_SET(x)     do { LED_WRITE(LED4_PIN, (x)); } while(0)
#define LED5_SET(x)     do { LED_WRITE(LED5_PIN, (x)); } while(0)
#define LED6_SET(x)     do { LED_WRITE(LED6_PIN, (x)); } while(0)

#define LED1_TOGGLE     do { GPIO_TG(LED_PORT) = LED1_PIN; } while(0)
#define LED2_TOGGLE     do { GPIO_TG(LED_PORT) = LED2_PIN; } while(0)
#define LED3_TOGGLE     do { GPIO_TG(LED_PORT) = LED3_PIN; } while(0)
#define LED4_TOGGLE     do { GPIO_TG(LED_PORT) = LED4_PIN; } while(0)
#define LED5_TOGGLE     do { GPIO_TG(LED_PORT) = LED5_PIN; } while(0)
#define LED6_TOGGLE     do { GPIO_TG(LED_PORT) = LED6_PIN; } while(0)

#define LED1_ON         do { LED_WRITE(LED1_PIN, 1); } while(0)
#define LED2_ON         do { LED_WRITE(LED2_PIN, 1); } while(0)
#define LED3_ON         do { LED_WRITE(LED3_PIN, 1); } while(0)
#define LED4_ON         do { LED_WRITE(LED4_PIN, 1); } while(0)
#define LED5_ON         do { LED_WRITE(LED5_PIN, 1); } while(0)
#define LED6_ON         do { LED_WRITE(LED6_PIN, 1); } while(0)

#define LED1_OFF        do { LED_WRITE(LED1_PIN, 0); } while(0)
#define LED2_OFF        do { LED_WRITE(LED2_PIN, 0); } while(0)
#define LED3_OFF        do { LED_WRITE(LED3_PIN, 0); } while(0)
#define LED4_OFF        do { LED_WRITE(LED4_PIN, 0); } while(0)
#define LED5_OFF        do { LED_WRITE(LED5_PIN, 0); } while(0)
#define LED6_OFF        do { LED_WRITE(LED6_PIN, 0); } while(0)


// FUNCTION
void bsp_led_init(void);

/***************************************************************************************************************/
/* KEY IO map
 * KEY1 -> PE15
 * KEY2 -> PE13
 * KEY3 -> PE11
 * KEY4 -> PE9
 * KEY5 -> PE7
 * KEY6 -> PB0
 * KEYW -> PA0, wakeup key
 */
#define KEYE_PORT        GPIOE
#define KEYB_PORT        GPIOB
#define KEYA_PORT        GPIOA
#define KEYE_CLK_PORT    RCU_GPIOE
#define KEYB_CLK_PORT    RCU_GPIOB
#define KEYA_CLK_PORT    RCU_GPIOA

#define KEY1_PIN        GPIO_PIN_15    /* PE15 */
#define KEY2_PIN        GPIO_PIN_13    /* PE13 */
#define KEY3_PIN        GPIO_PIN_11    /* PE11 */
#define KEY4_PIN        GPIO_PIN_9     /* PE9  */
#define KEY5_PIN        GPIO_PIN_7     /* PE7  */
#define KEY6_PIN        GPIO_PIN_0     /* PB0  */
#define KEYW_PIN        GPIO_PIN_0     /* PA0  */

#define KEY1_READ       gpio_input_bit_get(KEYE_PORT, KEY1_PIN)    /* PE15 */
#define KEY2_READ       gpio_input_bit_get(KEYE_PORT, KEY2_PIN)    /* PE13 */
#define KEY3_READ       gpio_input_bit_get(KEYE_PORT, KEY3_PIN)    /* PE11 */
#define KEY4_READ       gpio_input_bit_get(KEYE_PORT, KEY4_PIN)    /* PE9  */
#define KEY5_READ       gpio_input_bit_get(KEYE_PORT, KEY5_PIN)    /* PE7  */
#define KEY6_READ       gpio_input_bit_get(KEYB_PORT, KEY6_PIN)    /* PB0  */
#define KEYW_READ       gpio_input_bit_get(KEYA_PORT, KEYW_PIN)    /* PA0  */

// FUNCTION
void bsp_btn_init(void);
void bsp_wkup_key_exti_init(void);
void bsp_enter_deepsleep(void);
void bsp_enter_deepsleep_rtc_wakeup(uint16_t seconds);

/***************************************************************************************************************/

/* OLED */
#define I2C0_OWN_ADDRESS7      0x72
#define I2C0_SLAVE_ADDRESS7    0x82
#define I2C0_DATA_ADDRESS      (uint32_t)&I2C_DATA(I2C0)

#define OLED_PORT        GPIOB
#define OLED_CLK_PORT    RCU_GPIOB
#define OLED_DAT_PIN     GPIO_PIN_9
#define OLED_CLK_PIN     GPIO_PIN_8

// FUNCTION
void bsp_oled_init(void);

/***************************************************************************************************************/

/* gd25qxx */

#define SPI_PORT              GPIOB
#define SPI_CLK_PORT          RCU_GPIOB

#define SPI_NSS               GPIO_PIN_12
#define SPI_SCK               GPIO_PIN_13
#define SPI_MISO              GPIO_PIN_14
#define SPI_MOSI              GPIO_PIN_15

// FUNCTION
void bsp_gd25qxx_init(void);

/***************************************************************************************************************/

/* gd30ad3344 */

// DMA DMA1 CH0 CH5

/*
*   SPI_MODE_0  SPI_CK_PL_LOW_PH_1EDGE
*   SPI_MODE_1  SPI_CK_PL_LOW_PH_2EDGE
*   SPI_MODE_2  SPI_CK_PL_HIGH_PH_1EDGE
*   SPI_MODE_3  SPI_CK_PL_HIGH_PH_2EDGE
*/

#define SPI_MODE_0       SPI_CK_PL_LOW_PH_1EDGE
#define SPI_MODE_1       SPI_CK_PL_LOW_PH_2EDGE
#define SPI_MODE_2       SPI_CK_PL_HIGH_PH_1EDGE
#define SPI_MODE_3       SPI_CK_PL_HIGH_PH_2EDGE

#define GD30_SPIMODE           SPI_MODE_1

#define GD30_SPI               SPI0
#define GD30_DMA               DMA1
#define GD30_DMA_CHANNEL_TX    DMA_CH5
#define GD30_DMA_CHANNEL_RX    DMA_CH0
#define GD30_DMA_SUBPERI       DMA_SUBPERI3

#define GD30_DMA_RCU           RCU_DMA1
#define GD30_SPI_RCU           RCU_SPI0

#define GD30_SPI_PORT          GPIOA
#define GD30_SPI_PORT_RCU      RCU_GPIOA
#define GD30_SPI_SCK           GPIO_PIN_5
#define GD30_SPI_MISO          GPIO_PIN_6
#define GD30_SPI_MOSI          GPIO_PIN_7

#define GD30_CS_PORT           GPIOE
#define GD30_CS_PORT_RCU       RCU_GPIOE
#define GD30_CS_PIN            GPIO_PIN_8

#define GD30_CS_LOW()          gpio_bit_reset(GD30_CS_PORT, GD30_CS_PIN)
#define GD30_CS_HIGH()         gpio_bit_set  (GD30_CS_PORT, GD30_CS_PIN)

// FUNCTION
void bsp_gd30ad3344_init(void);

/***************************************************************************************************************/

/* USART */
/* RS485 mapping synced from temp/GD_Firmware_Template_v1:
 * USART1 TX/RX = PA2/PA3, RS485 direction = PA1, 1 = TX, 0 = RX.
 */
#define RS485_CS_PORT             GPIOA
#define RS485_CS_PORT_RCU         RCU_GPIOA
#define RS485_CS_PIN              GPIO_PIN_1
#define RS485_CS_SET(x)           do { if(x) GPIO_BOP(RS485_CS_PORT) = RS485_CS_PIN; else GPIO_BC(RS485_CS_PORT) = RS485_CS_PIN; } while(0)

/* DEBUG UART: used by my_printf(...), shared with OTA on the RS485 port. */
#define DEBUG_USART               USART1
#define RS232_RS485_USART         USART1
#define DEBUG_USART_RCU           RCU_USART1
#define DEBUG_USART_AF            GPIO_AF_7
#define DEBUG_USART_PORT          GPIOA
#define DEBUG_USART_PORT_RCU      RCU_GPIOA
#define DEBUG_USART_TX_PIN        GPIO_PIN_2
#define DEBUG_USART_RX_PIN        GPIO_PIN_3
#define DEBUG_UART_RX_DMA         DMA0
#define DEBUG_UART_RX_DMA_RCU     RCU_DMA0
#define DEBUG_UART_RX_DMA_CH      DMA_CH5
#define DEBUG_UART_RX_DMA_SUBPERI DMA_SUBPERI4
#define DEBUG_USART_BAUDRATE      19200U
#define DEBUG_UART_RXBUF_SIZE     512U
#define DEBUG_UART_RDATA_ADDRESS  ((uint32_t)&USART_DATA(DEBUG_USART))

/*
 * OTA UART: used by OTA receiver (DMA + IDLE interrupt).
 * Change only this block to switch OTA UARTx.
 *
 * Common choices on GD32F470:
 *  - USART1 (PA2/PA3 RS485 on this board)
 *  - USART2 (PB10/PB11)
 *  - UART3 / UART4 / USART5 / UART6 / UART7
 *
 * NOTE:
 * 1) Keep UART AF/pins, DMA controller/channel/subperi, and IRQN matched.
 * 2) Current config below uses USART2 on PB10/PB11.
 */
#define OTA_UART_PERIPH           USART1
#define OTA_UART_RCU              RCU_USART1
#define OTA_UART_IRQN             38  /* USART1_IRQn, must be a literal for #elif */
#define OTA_UART_PORT             GPIOA
#define OTA_UART_PORT_RCU         RCU_GPIOA
#define OTA_UART_TX_PIN           GPIO_PIN_2
#define OTA_UART_RX_PIN           GPIO_PIN_3
#define OTA_UART_AF               GPIO_AF_7
#define OTA_UART_DMA              DMA0
#define OTA_UART_DMA_RCU          RCU_DMA0
#define OTA_UART_DMA_CH           DMA_CH5
#define OTA_UART_DMA_SUBPERI      DMA_SUBPERI4
#define OTA_UART_BAUDRATE         19200U
#define OTA_UART_RXBUF_SIZE       8192U
#define OTA_UART_RDATA_ADDRESS    ((uint32_t)&USART_DATA(OTA_UART_PERIPH))

// FUNCTION
void bsp_usart_init(void);
void bsp_ota_uart_dma_rearm(void);
uint32_t bsp_ota_uart_dma_received_len(void);

/***************************************************************************************************************/

/* ADC */
// DMA DMA1 CH4

#define ADC_DMA         DMA1
#define ADC_DMA_CHANNEL DMA_CH4
#define ADC_DMA_SUBPERI DMA_SUBPERI0

#define ADC1_PORT       GPIOC
#define ADC1_CLK_PORT   RCU_GPIOC

#define ADC1_PIN        GPIO_PIN_0
#define ADC_CH1_PIN     GPIO_PIN_1
#define ADC_CH1_CHANNEL ADC_CHANNEL_11

#define ADC_VREF_SOURCE_PC2       1
#if ADC_VREF_SOURCE_PC2
#define ADC_VREF_PIN    GPIO_PIN_2
#define ADC_VREF_CHANNEL          ADC_CHANNEL_12
#else
#define ADC_VREF_CHANNEL          ADC_CHANNEL_17
#endif

// FUNCTION
void bsp_adc_init(void);

/***************************************************************************************************************/

/* DAC */
#define CONVERT_NUM                     (1)
#define DAC0_R12DH_ADDRESS              (0x40007408)  /* 12??????DAC??????????? */

#define DAC1_PORT       GPIOA
#define DAC1_CLK_PORT   RCU_GPIOA

#define DAC1_PIN        GPIO_PIN_4

// FUNCTION
void bsp_dac_init(void);

/***************************************************************************************************************/

/* RTC */
#define RTC_CLOCK_SOURCE_LXTAL
#define BKP_VALUE    0x32F0

// FUNCTION
int bsp_rtc_init(void);

/***************************************************************************************************************/

#ifdef __cplusplus
  }
#endif

#endif /* MCU_CIMC_GD32F470VET6_H */
