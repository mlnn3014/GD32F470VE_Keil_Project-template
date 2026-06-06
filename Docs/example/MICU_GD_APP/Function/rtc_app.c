/* Licence
* Company: MCUSTUDIO
* Auther: Ahypnis.
* Version: V0.10
* Time: 2026/04/29
* Note:
*/
#include "mcu_cimc_gd32f470vet6.h"

extern rtc_parameter_struct rtc_initpara;

/*!
    \brief      display the current time
    \param[in]  none
    \param[out] none
    \retval     none
*/
void rtc_task(void)
{
    rtc_current_time_get(&rtc_initpara);
}
