/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2026/04/29
* Note:
*/
#include "mcu_cimc_gd32f470vet6.h"

extern uint16_t adc_value[2];
extern uint16_t convertarr[CONVERT_NUM];

float g_pt100_voltage = 0.0f;
float g_pt100_temperature = 0.0f;

static float pt100_voltage_to_temperature(float voltage)
{
    return voltage * 100.0f;
}

void adc_task(void)
{
    float result = 0;
    result = GD30AD3344_AD_Read(GD30AD3344_Channel_4 ,GD30AD3344_PGA_6V144);
    g_pt100_voltage = result;
    g_pt100_temperature = pt100_voltage_to_temperature(result);
}

