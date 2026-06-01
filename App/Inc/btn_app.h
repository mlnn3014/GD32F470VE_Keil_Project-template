#ifndef BTN_APP_H
#define BTN_APP_H

#include <stdint.h>
#include "btn_bsp.h"

#ifdef __cplusplus
extern "C" {
#endif

/* BTN App 初始化按键 BSP，并周期扫描按键事件。 */
void btn_app_init(void);
void btn_task(void);

#ifdef __cplusplus
}
#endif

#endif /* BTN_APP_H */
