#ifndef CIMC_APP_H
#define CIMC_APP_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void cimc_app_init(void);
void cimc_app_task(void);
void cimc_app_process_ascii_frame(const char *ascii, size_t len);
void cimc_app_send_heartbeat(void);
uint16_t cimc_app_get_device_id(void);

/* Non-zero while the device is in the periodic auto-report (0x0302) state.
 * Used by the OLED status line ("AutoSample"/"IDLE") and the sample LED. */
uint8_t cimc_app_is_auto_reporting(void);

#ifdef __cplusplus
}
#endif

#endif /* CIMC_APP_H */
