/**
 ****************************************************************************************************
 * @file        uart_esp32.c
 * @author      Music Player Project
 * @version     V1.0
 * @date        2026-04-01
 * @brief       UART1 ESP32通信驱动实现（使用DMA + HAL_UARTEx_ReceiveToIdle_DMA）
 ****************************************************************************************************
 */

#include "./SYSTEM/uart_esp32/uart_esp32.h"
#include "./SYSTEM/delay/delay.h"
#include "string.h"

/* 全局变量 */
UART_HandleTypeDef g_uart_esp32_handle;     /* UART句柄 */
DMA_HandleTypeDef g_dma_esp32_rx_handle;    /* DMA RX句柄 */
DMA_HandleTypeDef g_dma_esp32_tx_handle;    /* DMA TX句柄 */
esp32_uart_t g_esp32_uart;                  /* ESP32 UART控制结构 */

/**
 * @brief       UART1初始化函数
 * @param       baudrate: 波特率（推荐921600）
 * @retval      无
 */
void uart_esp32_init(uint32_t baudrate)
{
    /* 清零控制结构 */
    memset(&g_esp32_uart, 0, sizeof(esp32_uart_t));
    
    /* 配置UART1 */
    g_uart_esp32_handle.Instance = ESP32_UART;
    g_uart_esp32_handle.Init.BaudRate = baudrate;
    g_uart_esp32_handle.Init.WordLength = UART_WORDLENGTH_8B;
    g_uart_esp32_handle.Init.StopBits = UART_STOPBITS_1;
    g_uart_esp32_handle.Init.Parity = UART_PARITY_NONE;
    g_uart_esp32_handle.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    g_uart_esp32_handle.Init.Mode = UART_MODE_TX_RX;
    g_uart_esp32_handle.Init.OverSampling = UART_OVERSAMPLING_16;
    
    HAL_UART_Init(&g_uart_esp32_handle);
    
    /* 启动DMA接收（使用ReceiveToIdle_DMA，零丢包） */
    HAL_UARTEx_ReceiveToIdle_DMA(&g_uart_esp32_handle, 
                                  g_esp32_uart.rx_buffer, 
                                  ESP32_RX_BUFFER_SIZE);
}

/**
 * @brief       UART接收事件回调（IDLE中断或DMA完成时触发）
 * @param       huart: UART句柄
 * @param       Size: 接收到的数据长度
 * @retval      无
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == ESP32_UART)
    {
        /* 保存接收长度 */
        g_esp32_uart.rx_len = Size;
        g_esp32_uart.rx_complete = 1;
        
        /* DMA会自动重启，无需手动调用 */
    }
}

/**
 * @brief       发送单个字节（阻塞方式）
 * @param       data: 要发送的字节
 * @retval      ESP32_UART_OK: 成功
 */
uint8_t uart_esp32_send_byte(uint8_t data)
{
    HAL_UART_Transmit(&g_uart_esp32_handle, &data, 1, 100);
    return ESP32_UART_OK;
}

/**
 * @brief       发送数据（DMA方式）
 * @param       data: 数据指针
 * @param       len: 数据长度
 * @retval      ESP32_UART_OK: 成功
 */
uint8_t uart_esp32_send_data(uint8_t *data, uint16_t len)
{
    if (len > ESP32_TX_BUFFER_SIZE) {
        return ESP32_UART_ERROR;
    }
    
    /* 复制到发送缓冲区 */
    memcpy(g_esp32_uart.tx_buffer, data, len);
    
    /* 启动DMA发送 */
    g_esp32_uart.tx_busy = 1;
    HAL_UART_Transmit_DMA(&g_uart_esp32_handle, g_esp32_uart.tx_buffer, len);
    
    return ESP32_UART_OK;
}

/**
 * @brief       获取接收到的数据
 * @param       data: 数据指针（输出）
 * @param       len: 数据长度（输出）
 * @retval      ESP32_UART_OK: 有数据
 *              ESP32_UART_ERROR: 无数据
 */
uint8_t uart_esp32_get_rx_data(uint8_t **data, uint16_t *len)
{
    if (g_esp32_uart.rx_complete)
    {
        *data = g_esp32_uart.rx_buffer;
        *len = g_esp32_uart.rx_len;
        return ESP32_UART_OK;
    }
    return ESP32_UART_ERROR;
}

/**
 * @brief       清除接收完成标志
 * @retval      无
 */
void uart_esp32_clear_rx_flag(void)
{
    g_esp32_uart.rx_complete = 0;
    g_esp32_uart.rx_len = 0;
}

/**
 * @brief       UART1中断服务函数
 * @retval      无
 */
void ESP32_UART_IRQHandler(void)
{
    HAL_UART_IRQHandler(&g_uart_esp32_handle);
}

/**
 * @brief       DMA2 Stream5中断服务函数（UART1 RX）
 * @retval      无
 */
void DMA2_Stream5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&g_dma_esp32_rx_handle);
}

/**
 * @brief       DMA2 Stream7中断服务函数（UART1 TX）
 * @retval      无
 */
void DMA2_Stream7_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&g_dma_esp32_tx_handle);
}

/**
 * @brief       DMA发送完成回调
 * @param       huart: UART句柄
 * @retval      无
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == ESP32_UART)
    {
        g_esp32_uart.tx_busy = 0;
    }
}
