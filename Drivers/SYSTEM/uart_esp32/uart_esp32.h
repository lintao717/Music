/**
 ****************************************************************************************************
 * @file        uart_esp32.h
 * @author      Music Player Project
 * @version     V1.0
 * @date        2026-04-01
 * @brief       UART1 ESP32通信驱动（使用DMA + HAL_UARTEx_ReceiveToIdle_DMA）
 ****************************************************************************************************
 */

#ifndef __UART_ESP32_H
#define __UART_ESP32_H

#include "stdio.h"
#include "./SYSTEM/sys/sys.h"

/*******************************************************************************************************/
/* UART1引脚定义 - 与ESP32通信 */

#define ESP32_UART_TX_GPIO_PORT              GPIOA
#define ESP32_UART_TX_GPIO_PIN               GPIO_PIN_9
#define ESP32_UART_TX_GPIO_AF                GPIO_AF7_USART1
#define ESP32_UART_TX_GPIO_CLK_ENABLE()      do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)

#define ESP32_UART_RX_GPIO_PORT              GPIOA
#define ESP32_UART_RX_GPIO_PIN               GPIO_PIN_10
#define ESP32_UART_RX_GPIO_AF                GPIO_AF7_USART1
#define ESP32_UART_RX_GPIO_CLK_ENABLE()      do{ __HAL_RCC_GPIOA_CLK_ENABLE(); }while(0)

#define ESP32_UART                           USART1
#define ESP32_UART_IRQn                      USART1_IRQn
#define ESP32_UART_IRQHandler                USART1_IRQHandler
#define ESP32_UART_CLK_ENABLE()              do{ __HAL_RCC_USART1_CLK_ENABLE(); }while(0)

/* DMA配置 */
#define ESP32_UART_DMA_CLK_ENABLE()          do{ __HAL_RCC_DMA2_CLK_ENABLE(); }while(0)
#define ESP32_UART_DMA_RX_STREAM             DMA2_Stream5
#define ESP32_UART_DMA_RX_CHANNEL            DMA_CHANNEL_4
#define ESP32_UART_DMA_TX_STREAM             DMA2_Stream7
#define ESP32_UART_DMA_TX_CHANNEL            DMA_CHANNEL_4

/*******************************************************************************************************/

/* 缓冲区大小定义 */
#define ESP32_RX_BUFFER_SIZE    2048        /* 接收缓冲区大小（足够接收YMODEM 1KB包） */
#define ESP32_TX_BUFFER_SIZE    128         /* 发送缓冲区大小（用于ACK/NAK等控制字符） */

/* UART状态定义 */
#define ESP32_UART_OK           0
#define ESP32_UART_ERROR        1
#define ESP32_UART_BUSY         2
#define ESP32_UART_TIMEOUT      3

/*******************************************************************************************************/

/* ESP32 UART控制结构体 */
typedef struct {
    uint8_t rx_buffer[ESP32_RX_BUFFER_SIZE];    /* 接收缓冲区 */
    uint8_t tx_buffer[ESP32_TX_BUFFER_SIZE];    /* 发送缓冲区 */
    uint16_t rx_len;                            /* 当前接收长度 */
    volatile uint8_t rx_complete;               /* 接收完成标志 */
    volatile uint8_t tx_busy;                   /* 发送忙标志 */
} esp32_uart_t;

/*******************************************************************************************************/

/* 外部变量声明 */
extern UART_HandleTypeDef g_uart_esp32_handle;  /* UART句柄 */
extern DMA_HandleTypeDef g_dma_esp32_rx_handle; /* DMA RX句柄 */
extern DMA_HandleTypeDef g_dma_esp32_tx_handle; /* DMA TX句柄 */
extern esp32_uart_t g_esp32_uart;               /* ESP32 UART控制结构 */

/*******************************************************************************************************/

/* 函数声明 */
void uart_esp32_init(uint32_t baudrate);                        /* 初始化UART1 */
uint8_t uart_esp32_send_byte(uint8_t data);                     /* 发送单个字节 */
uint8_t uart_esp32_send_data(uint8_t *data, uint16_t len);      /* 发送数据（DMA） */
uint8_t uart_esp32_get_rx_data(uint8_t **data, uint16_t *len);  /* 获取接收到的数据 */
void uart_esp32_clear_rx_flag(void);                            /* 清除接收完成标志 */

#endif

