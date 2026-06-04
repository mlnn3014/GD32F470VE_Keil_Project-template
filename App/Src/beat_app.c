#include "beat_app.h"

#include "data_app.h"
#include "frame_app.h"
#include "systick.h"

#define BEAT_MS 1000

static uint32_t last_ms;
static uint8_t first = 1;

void beat_task(void)
{
    uint8_t data[2];
    uint32_t now = systick_get_ms();
    uint16_t id;

    if ((first == 0) && ((uint32_t)(now - last_ms) < BEAT_MS))
    {
        return;
    }

    first = 0;
    last_ms = now;
    id = data_get_device_id();
    data[0] = (uint8_t)(id >> 8);
    data[1] = (uint8_t)id;

    frame_send(FRAME_TYPE_BEAT, data, sizeof(data));
}
