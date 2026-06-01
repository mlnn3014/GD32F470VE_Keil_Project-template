#include "ring_buffer.h"

#include <string.h>

static uint8_t ring_buffer_size_valid(uint16_t size)
{
    return ((size != 0U) && ((size & (uint16_t)(size - 1U)) == 0U)) ? 1U : 0U;
}

static uint8_t ring_buffer_valid(const ring_buffer_t *ring)
{
    if ((ring == 0) || (ring->buf == 0) || (ring->size == 0U) ||
        (ring->count > ring->size)) {
        return 0U;
    }

    return 1U;
}

void ring_buffer_init(ring_buffer_t *ring, uint8_t *buf, uint16_t size)
{
    if (ring == 0) {
        return;
    }

    ring->buf = 0;
    ring->size = 0U;
    ring->mask = 0U;
    ring->read = 0U;
    ring->write = 0U;
    ring->count = 0U;

    if ((buf == 0) || (ring_buffer_size_valid(size) == 0U)) {
        return;
    }

    ring->buf = buf;
    ring->size = size;
    ring->mask = (uint16_t)(size - 1U);
}

void ring_buffer_reset(ring_buffer_t *ring)
{
    if (ring_buffer_valid(ring) == 0U) {
        return;
    }

    ring->read = 0U;
    ring->write = 0U;
    ring->count = 0U;
}

uint16_t ring_buffer_write(ring_buffer_t *ring, const uint8_t *data, uint16_t len)
{
    uint16_t write_count;
    uint16_t first_len;
    uint16_t second_len;
    uint16_t free_count;

    if ((ring_buffer_valid(ring) == 0U) || (data == 0) || (len == 0U)) {
        return 0U;
    }

    free_count = (uint16_t)(ring->size - ring->count);
    write_count = (len < free_count) ? len : free_count;
    if (write_count == 0U) {
        return 0U;
    }

    first_len = write_count;
    if ((uint32_t)ring->write + first_len > ring->size) {
        first_len = (uint16_t)(ring->size - ring->write);
    }
    second_len = (uint16_t)(write_count - first_len);

    (void)memcpy(&ring->buf[ring->write], data, first_len);
    if (second_len != 0U) {
        (void)memcpy(ring->buf, &data[first_len], second_len);
    }

    ring->write = (uint16_t)((ring->write + write_count) & ring->mask);
    ring->count = (uint16_t)(ring->count + write_count);

    return write_count;
}

uint16_t ring_buffer_read(ring_buffer_t *ring, uint8_t *data, uint16_t len)
{
    uint16_t read_count;
    uint16_t first_len;
    uint16_t second_len;

    if ((ring_buffer_valid(ring) == 0U) || (data == 0) || (len == 0U)) {
        return 0U;
    }

    read_count = (len < ring->count) ? len : ring->count;
    if (read_count == 0U) {
        return 0U;
    }

    first_len = read_count;
    if ((uint32_t)ring->read + first_len > ring->size) {
        first_len = (uint16_t)(ring->size - ring->read);
    }
    second_len = (uint16_t)(read_count - first_len);

    (void)memcpy(data, &ring->buf[ring->read], first_len);
    if (second_len != 0U) {
        (void)memcpy(&data[first_len], ring->buf, second_len);
    }

    ring->read = (uint16_t)((ring->read + read_count) & ring->mask);
    ring->count = (uint16_t)(ring->count - read_count);

    return read_count;
}

uint16_t ring_buffer_available(const ring_buffer_t *ring)
{
    if (ring_buffer_valid(ring) == 0U) {
        return 0U;
    }

    return ring->count;
}

uint16_t ring_buffer_free(const ring_buffer_t *ring)
{
    if (ring_buffer_valid(ring) == 0U) {
        return 0U;
    }

    return (uint16_t)(ring->size - ring->count);
}

uint8_t ring_buffer_is_full(const ring_buffer_t *ring)
{
    if (ring_buffer_valid(ring) == 0U) {
        return 0U;
    }

    return (ring->count >= ring->size) ? 1U : 0U;
}

uint8_t ring_buffer_is_empty(const ring_buffer_t *ring)
{
    if (ring_buffer_valid(ring) == 0U) {
        return 1U;
    }

    return (ring->count == 0U) ? 1U : 0U;
}

const uint8_t *ring_buffer_read_ptr(const ring_buffer_t *ring)
{
    if ((ring_buffer_valid(ring) == 0U) || (ring->count == 0U)) {
        return 0;
    }

    return &ring->buf[ring->read];
}

uint16_t ring_buffer_read_linear(const ring_buffer_t *ring)
{
    uint16_t len;

    if ((ring_buffer_valid(ring) == 0U) || (ring->count == 0U)) {
        return 0U;
    }

    len = ring->count;
    if ((uint32_t)ring->read + len > ring->size) {
        len = (uint16_t)(ring->size - ring->read);
    }

    return len;
}

void ring_buffer_drop(ring_buffer_t *ring, uint16_t len)
{
    if ((ring_buffer_valid(ring) == 0U) || (len == 0U)) {
        return;
    }

    if (len > ring->count) {
        len = ring->count;
    }

    ring->read = (uint16_t)((ring->read + len) & ring->mask);
    ring->count = (uint16_t)(ring->count - len);
}
