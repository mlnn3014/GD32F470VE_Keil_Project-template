#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t *buf;   // 实际存数据的 buffer
    uint16_t size;  // buffer 大小, 必须是 2 的幂
    uint16_t mask;  // 环形下标掩码
    uint16_t read;  // 读下标
    uint16_t write; // 写下标
    uint16_t count; // 已存字节数
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
void ring_buffer_init(ring_buffer_t *ring, uint8_t *buf, uint16_t size); // 初始化 ring buffer
void ring_buffer_reset(ring_buffer_t *ring);                            // 清空读写指针
uint16_t ring_buffer_write(ring_buffer_t *ring, const uint8_t *data, uint16_t len); // 写入数据
uint16_t ring_buffer_read(ring_buffer_t *ring, uint8_t *data, uint16_t len);        // 读出数据
uint16_t ring_buffer_available(const ring_buffer_t *ring);              // 可读字节数
uint16_t ring_buffer_free(const ring_buffer_t *ring);                   // 剩余空间
uint8_t ring_buffer_is_full(const ring_buffer_t *ring);                 // 是否已满
uint8_t ring_buffer_is_empty(const ring_buffer_t *ring);                // 是否为空
const uint8_t *ring_buffer_read_ptr(const ring_buffer_t *ring);         // 当前连续可读指针
uint16_t ring_buffer_read_linear(const ring_buffer_t *ring);            // 当前连续可读长度
void ring_buffer_drop(ring_buffer_t *ring, uint16_t len);               // 丢弃指定长度数据

#ifdef __cplusplus
}
#endif

#endif /* RING_BUFFER_H */
