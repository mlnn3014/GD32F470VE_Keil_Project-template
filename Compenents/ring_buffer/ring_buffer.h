#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *buf;
    uint16_t size;
    uint16_t mask;
    uint16_t read;
    uint16_t write;
    uint16_t count;
} ring_buffer_t;

/*
 * 轻量字节环形缓冲区。
 *
 * 注意：
 * - size 必须是 2 的幂，底层 buf 生命周期必须长于 ring_buffer_t。
 * - 本库不做并发保护，ISR/任务同时访问时由调用者加临界区或锁。
 * - write/read 可能只处理部分数据，返回实际处理字节数。
 * - read_ptr/read_linear/drop 用于 DMA 或零拷贝连续读取场景；
 *   read_ptr 返回的指针在后续 write/read/drop/reset 后可能失效。
 */
void ring_buffer_init(ring_buffer_t *ring, uint8_t *buf, uint16_t size);
void ring_buffer_reset(ring_buffer_t *ring);
uint16_t ring_buffer_write(ring_buffer_t *ring, const uint8_t *data, uint16_t len);
uint16_t ring_buffer_read(ring_buffer_t *ring, uint8_t *data, uint16_t len);
uint16_t ring_buffer_available(const ring_buffer_t *ring);
uint16_t ring_buffer_free(const ring_buffer_t *ring);
uint8_t ring_buffer_is_full(const ring_buffer_t *ring);
uint8_t ring_buffer_is_empty(const ring_buffer_t *ring);
const uint8_t *ring_buffer_read_ptr(const ring_buffer_t *ring);
uint16_t ring_buffer_read_linear(const ring_buffer_t *ring);
void ring_buffer_drop(ring_buffer_t *ring, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* RING_BUFFER_H */
