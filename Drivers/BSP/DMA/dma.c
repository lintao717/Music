/**
 ****************************************************************************************************
 * @file        dma.c
 * @author      Music Player Project
 * @version     V1.0
 * @date        2026-04-01
 * @brief       DMA配置实现
 ****************************************************************************************************
 */

#include "./BSP/DMA/dma.h"

/**
 * @brief       DMA基础初始化
 * @note        使能DMA时钟，配置由各外设驱动完成
 * @retval      无
 */
void dma_init(void)
{
    /* 使能DMA1时钟 */
    __HAL_RCC_DMA1_CLK_ENABLE();
    
    /* 使能DMA2时钟 */
    __HAL_RCC_DMA2_CLK_ENABLE();
}
