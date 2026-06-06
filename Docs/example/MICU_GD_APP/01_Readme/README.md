# MICU_GD_APP 工程说明

这是 CIMC/MICU 板卡的 GD32F470 App 工程，配套 `MICU_GD_BootLoader` 使用，工具链为 Keil AC5 + CMSIS5。

App 链接到 APP1 运行区，从 `0x0800D000` 启动。OTA 通过 RS485 串口接收完整 `App_ota.bin` 文件，App 写入下载缓存区后复位，由 BootLoader 完成校验、备份、搬运和回滚。

## 目录结构

```text
MICU_GD_APP
|-- 01_Readme                         工程项目说明
|-- CMSIS                             内核/芯片支持文件
|   `-- Driver
|-- Function                          用户应用功能
|   |-- adc_app.c/.h
|   |-- btn_app.c/.h
|   |-- flash_fs_app.c/.h
|   |-- led_app.c/.h
|   |-- oled_app.c/.h
|   |-- rtc_app.c/.h
|   |-- scheduler.c/.h
|   `-- usart_app.c/.h
|-- HardWare                          板级与硬件驱动
|   |-- bsp
|   |-- ebtn
|   |-- gd25qxx
|   |-- gd30ad3344
|   |-- oled
|   `-- ringbuffer
|-- HeaderFiles                       公共头文件集合
|   |-- common                        Boot/App 共用 OTA 参数和分区
|   `-- User                          main、systick、中断头文件
|-- Library                           库文件
|   |-- GD32F4xx_standard_peripheral  GD32 标准外设库
|   `-- Third_Party                   第三方库
|-- project                           Keil 工程文件和生成文件
|   |-- Project.uvprojx
|   |-- App_F470.sct
|   |-- make_ota_package.bat
|   `-- Tools                         OTA 打包工具
|-- Protocol                          协议程序
|   `-- ota_uart
|-- Startup                           启动文件
`-- System                            系统入口和中断
    `-- src
```

## 工程信息

| 项目 | 内容 |
| --- | --- |
| Keil 工程 | `project/Project.uvprojx` |
| Scatter 文件 | `project/App_F470.sct` |
| App 运行地址 | `0x0800D000` |
| App 运行大小 | `0x00013000` |
| 向量表重定位 | `System/src/main.c` 中 `SCB->VTOR = 0x0800D000UL` |
| OTA 组件 | `Protocol/ota_uart/` |
| OTA 包头配置 | `HeaderFiles/common/ota_package_config.h` |
| 分区定义 | `HeaderFiles/common/bl_partition.h` |
| Boot 参数 | `HeaderFiles/common/bl_param.h` |

## Flash 分区

| 分区 | 起始地址 | 大小 | 作用 |
| --- | ---: | ---: | --- |
| BootLoader | `0x08000000` | `0x0000C000` | 上电启动、校验 OTA、搬运 App |
| Param | `0x0800C000` | `0x00001000` | OTA 状态、大小、CRC、地址等参数 |
| APP1 | `0x0800D000` | `0x00013000` | 当前运行 App |
| Backup | `0x08020000` | `0x00020000` | 升级前备份 APP1，用于失败回滚 |
| Middle | `0x08040000` | `0x00033000` | 预留中间区域 |
| Download | `0x08073000` | `0x0000D000` | OTA 下载缓存区 |

当前 OTA 允许接收的 raw `App.bin` 最大为 `0x0000D000` 字节。

## OTA 文件格式

发送给设备的文件是：

```text
[32 字节 OTA 头][raw App.bin]
```

也就是 `project/output/App_ota.bin`。不要发送 `App.bin`，也不要用 YMODEM。

OTA 头字段来自 `HeaderFiles/common/ota_package_config.h`：

| 字段 | 当前值/说明 |
| --- | --- |
| magic | `0x12345678` |
| header_version | `3` |
| app_version | `0x00010000` |
| target_addr | `0x0800D000` |
| image_type | `1` |
| app_size | 打包工具自动填入 |
| app_crc32 | 打包工具按 raw `App.bin` 自动计算 |
| header_crc32 | 打包工具按 OTA 头前 `0x1C` 字节自动计算 |

## 生成 OTA 固件

Keil 工程已配置 After Build User 命令：

```bat
fromelf --bin --output=.\output\App.bin .\output\Project.axf
.\Tools\ota_stream_pack.exe --config ..\HeaderFiles\common\ota_package_config.h --bin .\output\App.bin --out .\output\App_ota.bin
```

正常 Build 后会生成：

| 文件 | 说明 |
| --- | --- |
| `project/output/Project.hex` | 调试器下载 App 使用 |
| `project/output/App.bin` | 原始 App 固件，不带 OTA 头 |
| `project/output/App_ota.bin` | OTA 发送文件，带 32 字节 OTA 头 |

如果 Keil 没有自动生成，也可以运行：

```bat
project/make_ota_package.bat
```

## OTA 升级方法

1. 烧录 BootLoader，并确认当前 App 能正常运行。
2. 编译 App 工程，生成 `project/output/App_ota.bin`。
3. 打开串口助手，选择 RS485 对应串口。
4. 串口参数使用 `115200, 8N1`。
5. 选择“发送文件”或“发送二进制文件”，发送 `project/output/App_ota.bin`。
6. App 校验 OTA 头和整包 CRC，通过后写入 Boot 参数并复位。
7. BootLoader 从 Download 区校验新固件，备份旧 APP1，再把新固件搬运到 APP1。
8. 升级完成后跳转运行新的 App。

## 串口说明

当前 OTA 和 DEBUG 共用 RS485 串口：

| 功能 | 当前配置 |
| --- | --- |
| 外设 | `USART1` |
| TX/RX | `PA2 / PA3` |
| RS485 方向控制 | `PA1` |
| 波特率 | `115200` |
| OTA DMA 缓冲 | `8192` 字节 |

RS485 方向脚：`1 = TX`，`0 = RX`。OTA 接收时设备需要处于 RX。

## 常见问题

- 串口发送后没反应：确认发送的是 `App_ota.bin`，不是 `App.bin`；确认串口是 RS485 的 `USART1`。
- 提示 magic/header 错误：上位机或打包工具生成的头和 `ota_package_config.h` 不一致。
- 提示 CRC mismatch：通常是串口发送丢字节、发送了错误文件、或者发送过程中设备复位。
- 提示 App.bin too large：raw `App.bin` 超过 `OTA_PACKAGE_MAX_SIZE`，需要减小 App 体积或调整分区。
- BootLoader 没升级：确认 App 已经写入参数页，并且 BootLoader 和 App 的 `bl_partition.h`、`bl_param.h` 保持一致。
