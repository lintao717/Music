# STM32文件传输系统编译配置指南

## 📋 需要添加到工程的文件

### 1. DMA驱动
```
Drivers/BSP/DMA/dma.c
```

### 2. UART ESP32通信驱动
```
Drivers/SYSTEM/uart_esp32/uart_esp32.c
```

### 3. YMODEM协议
```
Middlewares/YMODEM/ymodem.c
```

### 4. SD卡辅助函数
```
Middlewares/FATFS/exfuns/sd_helper.c
```

### 5. 文件传输管理
```
User/file_transfer/file_transfer.c
```

### 6. 演示程序
```
User/demo_file_transfer.c
```

## 📂 头文件路径配置

在Keil工程设置中添加以下Include Paths：

```
Drivers/BSP/DMA
Drivers/SYSTEM/uart_esp32
Middlewares/YMODEM
Middlewares/FATFS/exfuns
User/file_transfer
```

## 🔧 Keil工程配置步骤

### 方法1: 手动添加（推荐）

1. **打开工程** - 双击`.uvprojx`文件
2. **添加文件组**（如果没有）：
   - 右键Project → Add Group
   - 创建以下组：
     - `BSP/DMA`
     - `SYSTEM/uart_esp32`
     - `Middlewares/YMODEM`
     - `Middlewares/FATFS`
     - `User/file_transfer`

3. **添加源文件**：
   - 右键对应组 → Add Existing Files to Group
   - 选择上述列出的`.c`文件

4. **配置头文件路径**：
   - 右键Project → Options for Target
   - C/C++ → Include Paths
   - 添加上述头文件路径

### 方法2: 修改工程文件（高级）

直接编辑`.uvprojx`文件，在`<Groups>`节点下添加：

```xml
<Group>
  <GroupName>BSP/DMA</GroupName>
  <Files>
    <File>
      <FileName>dma.c</FileName>
      <FilePath>Drivers\BSP\DMA\dma.c</FilePath>
    </File>
  </Files>
</Group>

<Group>
  <GroupName>SYSTEM/uart_esp32</GroupName>
  <Files>
    <File>
      <FileName>uart_esp32.c</FileName>
      <FilePath>Drivers\SYSTEM\uart_esp32\uart_esp32.c</FilePath>
    </File>
  </Files>
</Group>

<!-- 其他组类似添加 -->
```

## 🔨 编译前检查

### 1. 检查DMA配置
确保以下DMA通道没有冲突：
- ✅ UART1 RX: DMA2_Stream5_Channel4
- ✅ UART1 TX: DMA2_Stream7_Channel4
- ✅ SDIO RX: DMA2_Stream3_Channel4
- ✅ SDIO TX: DMA2_Stream6_Channel4

### 2. 检查中断向量表
在`stm32f4xx_it.c`中应该有以下中断处理函数：

```c
/* UART1中断 */
void USART1_IRQHandler(void);

/* DMA2 Stream5中断（UART1 RX） */
void DMA2_Stream5_IRQHandler(void);

/* DMA2 Stream7中断（UART1 TX） */
void DMA2_Stream7_IRQHandler(void);
```

**注意**：这些函数已经在`uart_esp32.c`中定义，如果`stm32f4xx_it.c`中也有定义，需要删除或注释掉，避免重复定义。

### 3. 检查HAL库配置
在`stm32f4xx_hal_conf.h`中确保启用：
```c
#define HAL_UART_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_SD_MODULE_ENABLED
```

## 📝 main.c修改

将`main.c`中的主函数修改为：

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
    HAL_Init();                                 /* 初始化HAL库 */
    sys_stm32_clock_init(336, 8, 2, 7);         /* 设置时钟,168Mhz */
    delay_init(168);                            /* 延时初始化 */
    usart_init(115200);                         /* 串口初始化为115200 */
    led_init();                                 /* 初始化LED */
    key_init();                                 /* 初始化按键 */
    oled_init();                                /* 初始化OLED */
    
    /* 运行文件传输演示 */
    demo_file_transfer_run();
}
```

## ⚠️ 常见编译错误及解决

### 错误1: `undefined reference to 'HAL_UARTEx_ReceiveToIdle_DMA'`

**原因**: HAL库版本太旧

**解决**: 
- 更新HAL库到V1.7.0或更高版本
- 或者使用环形缓冲区方案（需要修改代码）

### 错误2: `multiple definition of 'USART1_IRQHandler'`

**原因**: `stm32f4xx_it.c`和`uart_esp32.c`中都定义了中断处理函数

**解决**: 
在`stm32f4xx_it.c`中注释掉或删除：
```c
// void USART1_IRQHandler(void) { ... }
// void DMA2_Stream5_IRQHandler(void) { ... }
// void DMA2_Stream7_IRQHandler(void) { ... }
```

### 错误3: `undefined reference to 'f_expand'`

**原因**: FatFs版本太旧或未启用该功能

**解决**: 
在`ffconf.h`中确保：
```c
#define FF_USE_EXPAND   1
```

### 错误4: 内存不足

**原因**: RAM使用超过限制

**解决**: 
- 减小缓冲区大小（在各头文件中修改）
- 或者使用外部SRAM

## 🎯 编译步骤

1. **清理工程**: Project → Clean Targets
2. **重新编译**: Project → Build Target (F7)
3. **检查输出**: 
   - 0 Error(s)
   - 0 Warning(s)（或少量可忽略的警告）
4. **查看内存使用**:
   ```
   Program Size: Code=xxxxx RO-data=xxxx RW-data=xxxx ZI-data=xxxx
   ```
   - 确保Code + RO-data < 1MB（Flash）
   - 确保RW-data + ZI-data < 192KB（RAM）

## 🚀 烧录和测试

1. **连接ST-Link**
2. **烧录程序**: Flash → Download (F8)
3. **复位运行**: Debug → Start/Stop Debug Session
4. **打开串口工具**: 115200波特率
5. **观察输出**: 应该看到初始化信息

## 📊 预期输出

```
========================================
  STM32 File Transfer Demo
========================================
UART1 (PA9/PA10): ESP32 Communication
UART2 (PA2/PA3):  Debug Console
========================================

Initializing DMA...
DMA init OK!
Initializing SD card...
SD card init OK!
Filesystem mounted!
Initializing UART1 (921600 baud)...
UART1 init OK!
File transfer module ready!

Press KEY0 to start receiving file from ESP32
```

## 🔍 调试技巧

### 1. 使用Keil调试器
- 设置断点在`uart_esp32_init()`
- 单步执行，检查DMA配置是否正确

### 2. 串口输出调试
- 在关键位置添加`printf`
- 观察程序执行流程

### 3. LED指示
- LED0闪烁表示正在传输
- 快速闪烁表示成功
- 慢速闪烁表示错误

## 📞 需要帮助？

如果遇到问题：
1. 检查上述常见错误
2. 查看串口输出
3. 使用调试器单步执行
4. 检查硬件连接

---

**版本**: V1.0  
**日期**: 2026-04-01
