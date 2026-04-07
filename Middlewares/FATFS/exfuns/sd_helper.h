/**
 ****************************************************************************************************
 * @file        sd_helper.h
 * @author      Music Player Project
 * @version     V1.0
 * @date        2026-04-01
 * @brief       SD卡FatFs辅助函数（文件预分配、写缓冲）
 ****************************************************************************************************
 */

#ifndef __SD_HELPER_H
#define __SD_HELPER_H

#include "ff.h"

/* 写缓冲区大小 */
#define SD_WRITE_BUFFER_SIZE    4096

/* 函数声明 */
FRESULT sd_preallocate_file(FIL *fp, FSIZE_t size);         /* 预分配文件空间 */
FRESULT sd_buffered_write_init(FIL *fp);                    /* 初始化写缓冲 */
FRESULT sd_buffered_write(FIL *fp, const void *data, UINT len);  /* 缓冲写入 */
FRESULT sd_buffered_write_flush(FIL *fp);                   /* 刷新缓冲区 */
void sd_buffered_write_deinit(void);                        /* 释放写缓冲 */

#endif
