/**
 ****************************************************************************************************
 * @file        file_transfer.c
 * @author      Music Player Project
 * @version     V1.0
 * @date        2026-04-01
 * @brief       文件传输管理模块实现
 ****************************************************************************************************
 */

#include "./file_transfer/file_transfer.h"
#include "./SYSTEM/uart_esp32/uart_esp32.h"
#include "./YMODEM/ymodem.h"
#include "./FATFS/exfuns/sd_helper.h"
#include "./SYSTEM/delay/delay.h"
#include "string.h"
#include "stdio.h"

/* 传输控制结构 */
typedef struct {
    transfer_state_t state;
    FIL file;
    char save_path[256];
    transfer_info_t info;
    transfer_progress_callback_t callback;
    uint32_t last_update_time;
} transfer_ctrl_t;

static transfer_ctrl_t g_transfer_ctrl;

/**
 * @brief       初始化文件传输模块
 * @retval      无
 */
void file_transfer_init(void)
{
    memset(&g_transfer_ctrl, 0, sizeof(transfer_ctrl_t));
    g_transfer_ctrl.state = TRANSFER_IDLE;
    
    /* 初始化YMODEM */
    ymodem_init();
}

/**
 * @brief       开始文件传输
 * @param       save_path: 保存路径（如"0:/music/"）
 * @param       callback: 进度回调函数
 * @retval      0: 成功, 1: 失败
 */
uint8_t file_transfer_start(const char *save_path, transfer_progress_callback_t callback)
{
    if (g_transfer_ctrl.state != TRANSFER_IDLE)
    {
        return 1;  /* 正在传输中 */
    }
    
    /* 保存配置 */
    strncpy(g_transfer_ctrl.save_path, save_path, sizeof(g_transfer_ctrl.save_path) - 1);
    g_transfer_ctrl.callback = callback;
    
    /* 重置传输信息 */
    memset(&g_transfer_ctrl.info, 0, sizeof(transfer_info_t));
    g_transfer_ctrl.info.start_time = HAL_GetTick();
    
    /* 启动YMODEM接收 */
    ymodem_start_receive();
    
    g_transfer_ctrl.state = TRANSFER_WAITING;
    
    return 0;
}

/**
 * @brief       处理文件头包
 * @param       data: 数据指针
 * @param       len: 数据长度
 * @retval      0: 成功, 1: 失败
 */
static uint8_t handle_file_header(uint8_t *data, uint16_t len)
{
    FRESULT res;
    ymodem_file_info_t file_info;
    char full_path[512];
    
    /* 获取文件信息 */
    ymodem_get_file_info(&file_info);
    
    /* 构建完整路径 */
    snprintf(full_path, sizeof(full_path), "%s%s", 
             g_transfer_ctrl.save_path, file_info.filename);
    
    /* 创建文件 */
    res = f_open(&g_transfer_ctrl.file, full_path, FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK)
    {
        printf("Failed to create file: %s (error %d)\r\n", full_path, res);
        ymodem_abort();
        return 1;
    }
    
    /* 预分配文件空间（关键优化） */
    res = sd_preallocate_file(&g_transfer_ctrl.file, file_info.filesize);
    if (res != FR_OK)
    {
        printf("Warning: f_expand failed (error %d), transfer may be slower\r\n", res);
        /* 继续传输，只是速度会慢一些 */
    }
    else
    {
        printf("File preallocated: %lu bytes\r\n", file_info.filesize);
    }
    
    /* 初始化写缓冲 */
    res = sd_buffered_write_init(&g_transfer_ctrl.file);
    if (res != FR_OK)
    {
        printf("Failed to init write buffer\r\n");
        f_close(&g_transfer_ctrl.file);
        ymodem_abort();
        return 1;
    }
    
    /* 更新传输信息 */
    strncpy(g_transfer_ctrl.info.filename, file_info.filename, 
            sizeof(g_transfer_ctrl.info.filename) - 1);
    g_transfer_ctrl.info.total_bytes = file_info.filesize;
    g_transfer_ctrl.info.received_bytes = 0;
    
    printf("Receiving file: %s (%lu bytes)\r\n", file_info.filename, file_info.filesize);
    
    return 0;
}

/**
 * @brief       处理数据包
 * @param       data: 数据指针（指向数据部分，不含帧头）
 * @param       len: 数据长度
 * @retval      0: 成功, 1: 失败
 */
static uint8_t handle_data_packet(uint8_t *data, uint16_t len)
{
    FRESULT res;
    ymodem_file_info_t file_info;
    
    /* 获取当前接收进度 */
    ymodem_get_file_info(&file_info);
    
    /* 计算实际要写入的数据长度（最后一包可能不满） */
    uint16_t actual_len = len;
    if (file_info.received_bytes > g_transfer_ctrl.info.total_bytes)
    {
        actual_len = g_transfer_ctrl.info.total_bytes - 
                     (file_info.received_bytes - len);
    }
    
    /* 写入SD卡（使用缓冲写入） */
    res = sd_buffered_write(&g_transfer_ctrl.file, data, actual_len);
    if (res != FR_OK)
    {
        printf("Failed to write data (error %d)\r\n", res);
        ymodem_abort();
        return 1;
    }
    
    /* 更新传输信息 */
    g_transfer_ctrl.info.received_bytes = file_info.received_bytes;
    g_transfer_ctrl.info.elapsed_time = HAL_GetTick() - g_transfer_ctrl.info.start_time;
    
    /* 计算进度和速度 */
    if (g_transfer_ctrl.info.total_bytes > 0)
    {
        g_transfer_ctrl.info.progress = 
            (g_transfer_ctrl.info.received_bytes * 100) / g_transfer_ctrl.info.total_bytes;
    }
    
    if (g_transfer_ctrl.info.elapsed_time > 0)
    {
        g_transfer_ctrl.info.speed_kbps = 
            (g_transfer_ctrl.info.received_bytes / g_transfer_ctrl.info.elapsed_time);
    }
    
    /* 每500ms调用一次进度回调 */
    if (g_transfer_ctrl.callback != NULL)
    {
        uint32_t current_time = HAL_GetTick();
        if (current_time - g_transfer_ctrl.last_update_time >= 500)
        {
            g_transfer_ctrl.callback(&g_transfer_ctrl.info);
            g_transfer_ctrl.last_update_time = current_time;
        }
    }
    
    return 0;
}

/**
 * @brief       完成传输
 * @retval      无
 */
static void complete_transfer(void)
{
    FRESULT res;
    
    /* 刷新缓冲区 */
    res = sd_buffered_write_flush(&g_transfer_ctrl.file);
    if (res != FR_OK)
    {
        printf("Failed to flush buffer (error %d)\r\n", res);
    }
    
    /* 关闭文件 */
    f_close(&g_transfer_ctrl.file);
    
    /* 释放写缓冲 */
    sd_buffered_write_deinit();
    
    /* 最后一次进度回调 */
    if (g_transfer_ctrl.callback != NULL)
    {
        g_transfer_ctrl.callback(&g_transfer_ctrl.info);
    }
    
    printf("Transfer complete: %s (%lu bytes, %lu ms)\r\n",
           g_transfer_ctrl.info.filename,
           g_transfer_ctrl.info.received_bytes,
           g_transfer_ctrl.info.elapsed_time);
    
    g_transfer_ctrl.state = TRANSFER_COMPLETE;
}

/**
 * @brief       文件传输任务（需在主循环中调用）
 * @retval      无
 */
void file_transfer_task(void)
{
    uint8_t *rx_data;
    uint16_t rx_len;
    ymodem_error_t err;
    ymodem_state_t ymodem_state;
    
    /* 检查是否有接收到数据 */
    if (uart_esp32_get_rx_data(&rx_data, &rx_len) == ESP32_UART_OK)
    {
        /* 处理YMODEM数据包 */
        err = ymodem_process_packet(rx_data, rx_len);
        
        /* 获取YMODEM状态 */
        ymodem_state = ymodem_get_state();
        
        if (ymodem_state == YMODEM_STATE_RECEIVING)
        {
            if (g_transfer_ctrl.state == TRANSFER_WAITING)
            {
                /* 收到文件头包，创建文件 */
                if (handle_file_header(rx_data, rx_len) == 0)
                {
                    g_transfer_ctrl.state = TRANSFER_RECEIVING;
                }
                else
                {
                    g_transfer_ctrl.state = TRANSFER_ERROR;
                }
            }
            else if (g_transfer_ctrl.state == TRANSFER_RECEIVING)
            {
                /* 收到数据包，写入文件 */
                if (err == YMODEM_OK)
                {
                    /* 数据在 rx_data + 3 位置，长度根据帧头判断 */
                    uint16_t data_len = (rx_data[0] == YMODEM_SOH) ? 
                                        YMODEM_PACKET_SIZE_128 : YMODEM_PACKET_SIZE_1024;
                    handle_data_packet(rx_data + 3, data_len);
                }
            }
        }
        else if (ymodem_state == YMODEM_STATE_COMPLETE)
        {
            /* 传输完成 */
            complete_transfer();
        }
        else if (ymodem_state == YMODEM_STATE_ABORTED || ymodem_state == YMODEM_STATE_ERROR)
        {
            /* 传输中止或错误 */
            if (g_transfer_ctrl.state == TRANSFER_RECEIVING)
            {
                f_close(&g_transfer_ctrl.file);
                sd_buffered_write_deinit();
            }
            g_transfer_ctrl.state = TRANSFER_ABORTED;
            printf("Transfer aborted\r\n");
        }
        
        /* 清除接收标志 */
        uart_esp32_clear_rx_flag();
    }
}

/**
 * @brief       中止传输
 * @retval      无
 */
void file_transfer_abort(void)
{
    ymodem_abort();
    
    if (g_transfer_ctrl.state == TRANSFER_RECEIVING)
    {
        f_close(&g_transfer_ctrl.file);
        sd_buffered_write_deinit();
    }
    
    g_transfer_ctrl.state = TRANSFER_ABORTED;
}

/**
 * @brief       获取传输状态
 * @retval      状态
 */
transfer_state_t file_transfer_get_state(void)
{
    return g_transfer_ctrl.state;
}

/**
 * @brief       获取传输信息
 * @param       info: 信息结构指针
 * @retval      无
 */
void file_transfer_get_info(transfer_info_t *info)
{
    memcpy(info, &g_transfer_ctrl.info, sizeof(transfer_info_t));
}
