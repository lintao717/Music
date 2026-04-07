/**
 ****************************************************************************************************
 * @file        sd_helper.c
 * @author      Music Player Project
 * @version     V1.0
 * @date        2026-04-01
 * @brief       SD卡FatFs辅助函数实现
 ****************************************************************************************************
 */

#include "./FATFS/exfuns/sd_helper.h"
#include "string.h"
#include "stdlib.h"

/* 写缓冲区 */
static uint8_t *g_write_buffer = NULL;
static UINT g_buffer_index = 0;
static FIL *g_current_file = NULL;

/**
 * @brief       预分配文件空间（使用f_expand）
 * @param       fp: 文件指针
 * @param       size: 预分配大小
 * @retval      FR_OK: 成功
 */
FRESULT sd_preallocate_file(FIL *fp, FSIZE_t size)
{
    FRESULT res;
    
    /* 使用f_expand预分配连续簇 */
    res = f_expand(fp, size, 1);  /* 1表示预分配连续空间 */
    
    if (res == FR_OK)
    {
        /* 预分配成功，将文件指针移回开头 */
        f_lseek(fp, 0);
    }
    
    return res;
}

/**
 * @brief       初始化写缓冲
 * @param       fp: 文件指针
 * @retval      FR_OK: 成功
 */
FRESULT sd_buffered_write_init(FIL *fp)
{
    /* 分配缓冲区 */
    if (g_write_buffer == NULL)
    {
        g_write_buffer = (uint8_t *)malloc(SD_WRITE_BUFFER_SIZE);
        if (g_write_buffer == NULL)
        {
            return FR_NOT_ENOUGH_CORE;
        }
    }
    
    g_buffer_index = 0;
    g_current_file = fp;
    
    return FR_OK;
}

/**
 * @brief       缓冲写入
 * @param       fp: 文件指针
 * @param       data: 数据指针
 * @param       len: 数据长度
 * @retval      FR_OK: 成功
 */
FRESULT sd_buffered_write(FIL *fp, const void *data, UINT len)
{
    FRESULT res = FR_OK;
    UINT bytes_written;
    const uint8_t *src = (const uint8_t *)data;
    UINT remaining = len;
    
    while (remaining > 0)
    {
        UINT space_available = SD_WRITE_BUFFER_SIZE - g_buffer_index;
        UINT to_copy = (remaining < space_available) ? remaining : space_available;
        
        /* 复制到缓冲区 */
        memcpy(g_write_buffer + g_buffer_index, src, to_copy);
        g_buffer_index += to_copy;
        src += to_copy;
        remaining -= to_copy;
        
        /* 如果缓冲区满了，写入SD卡 */
        if (g_buffer_index >= SD_WRITE_BUFFER_SIZE)
        {
            res = f_write(fp, g_write_buffer, SD_WRITE_BUFFER_SIZE, &bytes_written);
            if (res != FR_OK || bytes_written != SD_WRITE_BUFFER_SIZE)
            {
                return res;
            }
            g_buffer_index = 0;
        }
    }
    
    return FR_OK;
}

/**
 * @brief       刷新缓冲区到SD卡
 * @param       fp: 文件指针
 * @retval      FR_OK: 成功
 */
FRESULT sd_buffered_write_flush(FIL *fp)
{
    FRESULT res = FR_OK;
    UINT bytes_written;
    
    if (g_buffer_index > 0)
    {
        res = f_write(fp, g_write_buffer, g_buffer_index, &bytes_written);
        if (res == FR_OK)
        {
            g_buffer_index = 0;
        }
    }
    
    /* 同步到SD卡 */
    f_sync(fp);
    
    return res;
}

/**
 * @brief       释放写缓冲
 * @retval      无
 */
void sd_buffered_write_deinit(void)
{
    if (g_write_buffer != NULL)
    {
        free(g_write_buffer);
        g_write_buffer = NULL;
    }
    g_buffer_index = 0;
    g_current_file = NULL;
}
