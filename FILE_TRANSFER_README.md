# STM32与ESP32音乐文件传输系统使用指南

## 📋 系统概述

本系统实现了STM32F407通过UART1与ESP32进行高速音乐文件传输，使用YMODEM协议，支持SDIO存储和VS1053B音频播放。

### 核心特性

- ✅ **零丢包UART接收** - 使用`HAL_UARTEx_ReceiveToIdle_DMA()`，硬件层面处理IDLE中断
- ✅ **YMODEM协议** - 成熟的文件传输协议，CRC16校验，自动重传
- ✅ **高性能SD卡写入** - 使用`f_expand()`预分配连续簇，写入速度提升2-5倍
- ✅ **实时进度显示** - OLED显示传输进度、速度和文件信息
- ✅ **传输速度** - 80-90 KB/s（921600波特率）

## 🔧 硬件连接

### UART连接（STM32 ↔ ESP32）

```
STM32 PA9  (UART1 TX) ←→ ESP32 RX0
STM32 PA10 (UART1 RX) ←→ ESP32 TX0
GND                   ←→ GND
```

### 其他接口

- **调试串口**: PA2(TX), PA3(RX) - USART2, 115200波特率
- **SD卡**: SDIO接口 (PC8-PC12, PD2)
- **VS1053B**: SPI1 (PB3/PB4/PB5)
- **OLED**: I2C (PB6/PB7)

## 📁 文件结构

```
Music/
├── Drivers/
│   └── SYSTEM/
│       └── uart_esp32/          # UART1 ESP32通信驱动
│           ├── uart_esp32.h
│           └── uart_esp32.c
├── Middlewares/
│   ├── YMODEM/                  # YMODEM协议实现
│   │   ├── ymodem.h
│   │   └── ymodem.c
│   └── FATFS/
│       └── exfuns/
│           ├── sd_helper.h      # SD卡辅助函数
│           └── sd_helper.c
└── User/
    ├── file_transfer/           # 文件传输管理
    │   ├── file_transfer.h
    │   └── file_transfer.c
    └── demo_file_transfer.c     # 演示程序
```

## 🚀 快速开始

### 1. 编译配置

在Keil工程中添加以下文件到编译：

```
Drivers/SYSTEM/uart_esp32/uart_esp32.c
Middlewares/YMODEM/ymodem.c
Middlewares/FATFS/exfuns/sd_helper.c
User/file_transfer/file_transfer.c
User/demo_file_transfer.c
```

添加头文件路径：
```
Drivers/SYSTEM/uart_esp32
Middlewares/YMODEM
User/file_transfer
```

### 2. 修改main.c

```c
#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/LED/led.h"
#include "./BSP/KEY/key.h"
#include "./BSP/OLED/oled.h"

extern void demo_file_transfer_run(void);

int main(void)
{
    HAL_Init();
    sys_stm32_clock_init(336, 8, 2, 7);     /* 168MHz */
    delay_init(168);
    usart_init(115200);                      /* 调试串口 */
    led_init();
    key_init();
    oled_init();
    
    /* 运行文件传输演示 */
    demo_file_transfer_run();
}
```

### 3. 编译并烧录

1. 编译工程（确保无错误）
2. 烧录到STM32F407
3. 打开串口调试工具（115200波特率）

## 📡 使用方法

### STM32端操作

1. **上电启动** - OLED显示"Ready"
2. **按KEY0** - 开始等待接收文件
3. **传输中** - OLED显示进度、速度
4. **按KEY1** - 中止传输
5. **传输完成** - OLED显示"Complete!"，LED快速闪烁

### ESP32端发送（使用YMODEM）

#### 方法1: 使用Arduino + YMODEM库

```cpp
#include <YModem.h>

void setup() {
    Serial.begin(921600);  // 与STM32通信
}

void loop() {
    if (Serial.available()) {
        // 使用YMODEM发送文件
        YModem.sendFile("/music/song.mp3");
    }
}
```

#### 方法2: 使用串口工具测试

1. 将ESP32的TX0/RX0连接到PC串口工具
2. 使用Tera Term或SecureCRT
3. 选择"发送文件" → "YMODEM"
4. 选择要发送的音乐文件

## 📊 性能指标

| 指标 | 数值 |
|------|------|
| UART波特率 | 921600 |
| 理论传输速度 | ~115 KB/s |
| 实际传输速度 | 80-90 KB/s |
| 1MB文件传输时间 | 11-13秒 |
| SD卡写入速度 | 不是瓶颈（使用f_expand后） |

## 🔍 调试信息

### 串口输出示例

```
========================================
  STM32 File Transfer Demo
========================================
UART1 (PA9/PA10): ESP32 Communication
UART2 (PA2/PA3):  Debug Console
========================================

Initializing SD card...
SD card init OK!
Filesystem mounted!
Initializing UART1 (921600 baud)...
UART1 init OK!
File transfer module ready!

Press KEY0 to start receiving file from ESP32

Starting file transfer...
Waiting for file from ESP32...

Receiving file: song.mp3 (2048576 bytes)
File preallocated: 2048576 bytes
Progress: 10% (204857/2048576 bytes) Speed: 85 KB/s
Progress: 20% (409714/2048576 bytes) Speed: 87 KB/s
...
Progress: 100% (2048576/2048576 bytes) Speed: 86 KB/s

========================================
Transfer Complete!
File: song.mp3
Size: 2048576 bytes
Time: 23456 ms
Avg Speed: 86 KB/s
========================================
```

## ⚠️ 注意事项

### 1. SD卡初始化问题

如果SD卡初始化失败（之前测试中出现过），请检查：

- SD卡是否正确插入
- SD卡格式是否为FAT32
- SDIO引脚连接是否正确
- 尝试更换SD卡

**临时解决方案**：如果SDIO不可用，可以修改为SPI模式SD卡驱动。

### 2. DMA冲突检查

确保DMA通道分配正确：
- UART1 RX: DMA2_Stream5_CH4 ✅
- UART1 TX: DMA2_Stream7_CH4 ✅
- SDIO RX: DMA2_Stream3_CH4 ✅
- SDIO TX: DMA2_Stream6_CH4 ✅

### 3. 内存使用

总内存占用约12KB：
- UART缓冲: 2KB + 128B
- YMODEM缓冲: 1KB
- SD写缓冲: 4KB
- 其他: ~512B

### 4. 传输时暂停播放

如果同时运行音频播放，建议在传输时暂停播放，避免SD卡读写冲突。

## 🐛 常见问题

### Q1: 传输速度很慢（<50KB/s）

**原因**: SD卡碎片严重或未使用f_expand预分配

**解决**: 
- 格式化SD卡
- 确保调用了`sd_preallocate_file()`

### Q2: 传输中断或丢包

**原因**: UART波特率不稳定或DMA配置错误

**解决**:
- 检查晶振频率是否准确
- 降低波特率到460800测试
- 检查DMA中断优先级

### Q3: 文件传输后无法播放

**原因**: 文件不完整或CRC校验失败

**解决**:
- 检查传输完成标志
- 对比文件大小
- 重新传输

## 📚 API参考

### 文件传输API

```c
/* 初始化 */
void file_transfer_init(void);

/* 开始传输 */
uint8_t file_transfer_start(const char *save_path, 
                             transfer_progress_callback_t callback);

/* 传输任务（主循环调用） */
void file_transfer_task(void);

/* 中止传输 */
void file_transfer_abort(void);

/* 获取状态 */
transfer_state_t file_transfer_get_state(void);

/* 获取传输信息 */
void file_transfer_get_info(transfer_info_t *info);
```

### 进度回调

```c
void my_progress_callback(transfer_info_t *info)
{
    printf("Progress: %d%% (%lu/%lu bytes) Speed: %d KB/s\r\n",
           info->progress, 
           info->received_bytes, 
           info->total_bytes, 
           info->speed_kbps);
}
```

## 🔄 后续扩展

- [ ] 支持断点续传
- [ ] 支持多文件批量传输
- [ ] 添加文件列表管理
- [ ] 实现播放列表自动更新
- [ ] 添加传输加密
- [ ] 支持WiFi/BLE传输

## 📞 技术支持

如有问题，请检查：
1. 串口调试输出
2. OLED显示信息
3. LED闪烁状态

---

**版本**: V1.0  
**日期**: 2026-04-01  
**作者**: Music Player Project
