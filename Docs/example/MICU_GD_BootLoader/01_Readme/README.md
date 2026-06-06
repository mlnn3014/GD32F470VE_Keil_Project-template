# MICU GD BootLoader

这是 CIMC/MICU 板卡的 GD32F470 BootLoader 工程，工具链为 Keil AC5 + CMSIS5。

## 工程入口

- Keil 工程：`../project/Project.uvprojx`
- 链接脚本：`../project/BootLoader_F470.sct`

## 目录结构

| 目录 | 说明 |
| --- | --- |
| `CMSIS` | GD32F4xx CMSIS 设备头和 `system_gd32f4xx.c` |
| `Function` | BootLoader 运行时使用的应用辅助模块，例如串口打印 |
| `HardWare` | 板级 BSP |
| `HeaderFiles` | 用户头文件、BootLoader 头文件和公共分区/参数头 |
| `Library` | GD32F4xx 标准外设库 |
| `Protocol` | BootLoader 升级、校验、搬运和回滚逻辑 |
| `Startup` | 启动汇编文件 |
| `System` | `main.c`、中断文件和 SysTick |
| `project` | Keil 工程、scatter 文件和编译输出目录 |

## OTA 流程

App 接收 `App_ota.bin` 并写入 Download 区，随后设置参数页并复位。BootLoader 上电后检查参数页和 Download 区 OTA 头，校验固件 CRC，通过后备份当前 APP1，再把新固件搬运到 APP1。若搬运或最终校验失败，会尝试从 Backup 区恢复旧 APP。

BootLoader 和 App 的 `HeaderFiles/common/bl_partition.h`、`HeaderFiles/common/bl_param.h` 必须保持一致。
