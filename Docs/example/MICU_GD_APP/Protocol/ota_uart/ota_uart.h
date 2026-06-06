#ifndef OTA_UART_H
#define OTA_UART_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 复位 OTA 串口接收状态。
 *
 * 清空 OTA 解析状态和软件环形缓冲区，但不会擦除已经写入下载缓存区的固件数据。
 *
 * @return 无
 */
void ota_uart_reset_state(void);

/**
 * @brief 向 OTA 接收器喂入串口原始数据。
 *
 * UART DMA 轮询层收到新字节后调用本函数。数据会先写入 OTA 软件环形缓冲区，
 * 后续由 ota_uart_task() 解析和写 Flash。
 *
 * @param[in] data 新收到的串口数据指针
 * @param[in] len data 中的数据长度，单位字节
 * @return 无
 */
void ota_uart_process_frame(const uint8_t *data, uint32_t len);

/**
 * @brief 执行 OTA 接收状态机。
 *
 * 本函数负责查找 ota_stream_header_t、校验 OTA 头、擦除下载缓存区、将后续
 * raw App.bin 写入 Flash、校验整包 CRC、提交 BootLoader 参数，并复位进入
 * BootLoader 完成安装。
 *
 * @return 无
 */
void ota_uart_task(void);
uint8_t ota_uart_request_bootloader_upgrade(void);

/**
 * @brief 将当前 APP 的通信参数（设备 ID + 波特率码）写入 BootLoader 参数页。
 *
 * 进入 BootLoader 后，BL 需要按 APP 当前的设备 ID 与波特率收发：
 *   - 波特率：M-01 把上位机切到 115200 后，BL 必须同步，否则收不到 0x0502/0x0503；
 *   - 设备 ID：L-01 把 ID 改成非 0x0001 后，BL 必须按该 ID 匹配并应答，否则 N-02/N-03 不得分。
 * 若与已存参数相同则跳过擦写，避免无谓的 Flash 磨损。
 *
 * @param[in] device_id 当前设备 ID（0x0001~0xFFFE）
 * @param[in] baud_code 题目协议 4.5.1(7) 定义的波特率映射码: 0x11/0x12/0x13/0x14
 * @return 写入成功返回 1，失败返回 0
 */
uint8_t ota_uart_save_comm_params(uint16_t device_id, uint8_t baud_code);

#ifdef __cplusplus
}
#endif

#endif /* OTA_UART_H */

