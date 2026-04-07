/**
 ****************************************************************************************************
 * @file        demo_file_transfer.c
 * @author      Music Player Project
 * @version     V1.0
 * @date        2026-04-01
 * @brief       文件传输功能演示程序
 ****************************************************************************************************
 */

#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./SYSTEM/uart_esp32/uart_esp32.h"
#include "./BSP/LED/led.h"
#include "./BSP/KEY/key.h"
#include "./BSP/OLED/oled.h"
#include "./BSP/SDIO/sdio_sdcard.h"
#include "./BSP/DMA/dma.h"
#include "./file_transfer/file_transfer.h"
#include "./FATFS/source/ff.h"
#include "stdio.h"

/* FatFs工作区 */
FATFS g_fs;

/**
 * @brief       传输进度回调函数
 * @param       info: 传输信息
 * @retval      无
 */
void transfer_progress_callback(transfer_info_t *info)
{
    /* 在OLED上显示进度 */
    char buf[32];
    
    oled_clear();
    
    /* 显示文件名 */
    oled_show_string(0, 0, "Receiving:");
    oled_show_string(0, 16, info->filename);
    
    /* 显示进度 */
    snprintf(buf, sizeof(buf), "Progress: %d%%", info->progress);
    oled_show_string(0, 32, buf);
    
    /* 显示速度 */
    snprintf(buf, sizeof(buf), "Speed: %d KB/s", info->speed_kbps);
    oled_show_string(0, 48, buf);
    
    oled_refresh();
    
    /* 串口输出 */
    printf("Progress: %d%% (%lu/%lu bytes) Speed: %d KB/s\r\n",
           info->progress, info->received_bytes, info->total_bytes, info->speed_kbps);
    
    /* LED闪烁指示 */
    LED0_TOGGLE();
}

/**
 * @brief       文件传输演示主函数
 * @retval      无
 */
void demo_file_transfer_run(void)
{
    uint8_t key;
    FRESULT res;
    transfer_state_t state;
    
    printf("\r\n========================================\r\n");
    printf("  STM32 File Transfer Demo\r\n");
    printf("========================================\r\n");
    printf("UART1 (PA9/PA10): ESP32 Communication\r\n");
    printf("UART2 (PA2/PA3):  Debug Console\r\n");
    printf("========================================\r\n\r\n");
    
    /* 初始化DMA */
    printf("Initializing DMA...\r\n");
    dma_init();
    printf("DMA init OK!\r\n");
    
    /* 初始化SD卡 */
    printf("Initializing SD card...\r\n");
    if (sd_init() != 0)
    {
        printf("SD card init failed!\r\n");
        oled_clear();
        oled_show_string(0, 0, "SD Card Error!");
        oled_refresh();
        while (1)
        {
            LED0_TOGGLE();
            delay_ms(500);
        }
    }
    printf("SD card init OK!\r\n");
    
    /* 挂载文件系统 */
    res = f_mount(&g_fs, "0:", 1);
    if (res != FR_OK)
    {
        printf("Failed to mount filesystem (error %d)\r\n", res);
        oled_clear();
        oled_show_string(0, 0, "Mount Error!");
        oled_refresh();
        while (1)
        {
            LED0_TOGGLE();
            delay_ms(500);
        }
    }
    printf("Filesystem mounted!\r\n");
    
    /* 初始化UART1（ESP32通信，921600波特率） */
    printf("Initializing UART1 (921600 baud)...\r\n");
    uart_esp32_init(921600);
    printf("UART1 init OK!\r\n");
    
    /* 初始化文件传输模块 */
    printf("Initializing file transfer module...\r\n");
    file_transfer_init();
    printf("File transfer module ready!\r\n\r\n");
    
    /* OLED显示就绪 */
    oled_clear();
    oled_show_string(0, 0, "File Transfer");
    oled_show_string(0, 16, "Ready!");
    oled_show_string(0, 32, "Press KEY0 to");
    oled_show_string(0, 48, "start receive");
    oled_refresh();
    
    printf("Press KEY0 to start receiving file from ESP32\r\n");
    printf("Press KEY1 to abort transfer\r\n\r\n");
    
    while (1)
    {
        key = key_scan(0);
        
        /* KEY0: 开始接收 */
        if (key == KEY0_PRES)
        {
            state = file_transfer_get_state();
            
            if (state == TRANSFER_IDLE || state == TRANSFER_COMPLETE || 
                state == TRANSFER_ABORTED || state == TRANSFER_ERROR)
            {
                printf("Starting file transfer...\r\n");
                printf("Waiting for file from ESP32...\r\n\r\n");
                
                oled_clear();
                oled_show_string(0, 0, "Waiting for");
                oled_show_string(0, 16, "ESP32...");
                oled_refresh();
                
                /* 开始传输，保存到SD卡根目录 */
                file_transfer_start("0:/", transfer_progress_callback);
            }
        }
        
        /* KEY1: 中止传输 */
        if (key == KEY1_PRES)
        {
            state = file_transfer_get_state();
            
            if (state == TRANSFER_WAITING || state == TRANSFER_RECEIVING)
            {
                printf("Aborting transfer...\r\n");
                file_transfer_abort();
                
                oled_clear();
                oled_show_string(0, 0, "Transfer");
                oled_show_string(0, 16, "Aborted!");
                oled_refresh();
            }
        }
        
        /* 运行文件传输任务 */
        file_transfer_task();
        
        /* 检查传输状态 */
        state = file_transfer_get_state();
        
        if (state == TRANSFER_COMPLETE)
        {
            /* 传输完成 */
            transfer_info_t info;
            file_transfer_get_info(&info);
            
            oled_clear();
            oled_show_string(0, 0, "Complete!");
            oled_show_string(0, 16, info.filename);
            oled_refresh();
            
            printf("\r\n========================================\r\n");
            printf("Transfer Complete!\r\n");
            printf("File: %s\r\n", info.filename);
            printf("Size: %lu bytes\r\n", info.total_bytes);
            printf("Time: %lu ms\r\n", info.elapsed_time);
            printf("Avg Speed: %d KB/s\r\n", info.speed_kbps);
            printf("========================================\r\n\r\n");
            
            /* 快速闪烁LED表示成功 */
            for (int i = 0; i < 10; i++)
            {
                LED0_TOGGLE();
                delay_ms(100);
            }
            
            /* 重置为空闲状态 */
            file_transfer_init();
            
            /* 显示就绪界面 */
            oled_clear();
            oled_show_string(0, 0, "Ready for");
            oled_show_string(0, 16, "next file");
            oled_show_string(0, 32, "Press KEY0");
            oled_refresh();
        }
        else if (state == TRANSFER_ERROR || state == TRANSFER_ABORTED)
        {
            /* 传输失败或中止 */
            printf("Transfer failed or aborted\r\n");
            
            /* 慢速闪烁LED表示错误 */
            LED0_TOGGLE();
            delay_ms(1000);
            
            /* 5秒后重置 */
            static uint32_t error_time = 0;
            if (error_time == 0)
            {
                error_time = HAL_GetTick();
            }
            if (HAL_GetTick() - error_time > 5000)
            {
                file_transfer_init();
                error_time = 0;
                
                oled_clear();
                oled_show_string(0, 0, "Ready");
                oled_refresh();
            }
        }
        
        delay_ms(10);
    }
}
