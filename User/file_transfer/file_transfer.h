/**
 ****************************************************************************************************
 * @file        file_transfer.h
 * @author      Music Player Project
 * @version     V1.0
 * @date        2026-04-01
 * @brief       文件传输管理模块（整合YMODEM + FatFs）
 ****************************************************************************************************
 */

#ifndef __FILE_TRANSFER_H
#define __FILE_TRANSFER_H

#include "./SYSTEM/sys/sys.h"
#include "./FATFS/source/ff.h"	

/* 传输状态 */
typedef enum {
    TRANSFER_IDLE = 0,          /* 空闲 */
    TRANSFER_WAITING,           /* 等待开始 */
    TRANSFER_RECEIVING,         /* 接收中 */
    TRANSFER_COMPLETE,          /* 完成 */
    TRANSFER_ERROR,             /* 错误 */
    TRANSFER_ABORTED            /* 已中止 */
} transfer_state_t;

/* 传输统计信息 */
typedef struct {
    char filename[256];         /* 文件名 */
    uint32_t total_bytes;       /* 总字节数 */
    uint32_t received_bytes;    /* 已接收字节数 */
    uint32_t start_time;        /* 开始时间（ms） */
    uint32_t elapsed_time;      /* 已用时间（ms） */
    uint16_t speed_kbps;        /* 传输速度（KB/s） */
    uint8_t progress;           /* 进度百分比 */
} transfer_info_t;

/* 进度回调函数类型 */
typedef void (*transfer_progress_callback_t)(transfer_info_t *info);

/* 函数声明 */
void file_transfer_init(void);                                      /* 初始化 */
uint8_t file_transfer_start(const char *save_path, 
                             transfer_progress_callback_t callback); /* 开始传输 */
void file_transfer_task(void);                                      /* 传输任务（需在主循环中调用） */
void file_transfer_abort(void);                                     /* 中止传输 */
transfer_state_t file_transfer_get_state(void);                     /* 获取状态 */
void file_transfer_get_info(transfer_info_t *info);                 /* 获取传输信息 */

#endif
