/**
 ****************************************************************************************************
 * @file        ymodem.h
 * @author      Music Player Project
 * @version     V1.0
 * @date        2026-04-01
 * @brief       YMODEM协议实现（接收端）
 ****************************************************************************************************
 */

#ifndef __YMODEM_H
#define __YMODEM_H

#include "./SYSTEM/sys/sys.h"

/* YMODEM协议控制字符 */
#define YMODEM_SOH      0x01    /* 128字节数据包起始 */
#define YMODEM_STX      0x02    /* 1024字节数据包起始 */
#define YMODEM_EOT      0x04    /* 传输结束 */
#define YMODEM_ACK      0x06    /* 确认应答 */
#define YMODEM_NAK      0x15    /* 否定应答 */
#define YMODEM_CAN      0x18    /* 取消传输 */
#define YMODEM_C        0x43    /* 'C' - 请求使用CRC16校验 */

/* 数据包大小 */
#define YMODEM_PACKET_SIZE_128      128
#define YMODEM_PACKET_SIZE_1024     1024
#define YMODEM_PACKET_HEADER_SIZE   3       /* 帧头(1) + 序号(1) + 序号反码(1) */
#define YMODEM_PACKET_CRC_SIZE      2       /* CRC16 */
#define YMODEM_PACKET_128_SIZE      (YMODEM_PACKET_HEADER_SIZE + YMODEM_PACKET_SIZE_128 + YMODEM_PACKET_CRC_SIZE)
#define YMODEM_PACKET_1024_SIZE     (YMODEM_PACKET_HEADER_SIZE + YMODEM_PACKET_SIZE_1024 + YMODEM_PACKET_CRC_SIZE)

/* 超时和重试 */
#define YMODEM_TIMEOUT_MS           3000    /* 3秒超时 */
#define YMODEM_MAX_RETRY            3       /* 最多重试3次 */

/* YMODEM状态 */
typedef enum {
    YMODEM_STATE_IDLE = 0,          /* 空闲 */
    YMODEM_STATE_WAIT_HEADER,       /* 等待文件头包 */
    YMODEM_STATE_RECEIVING,         /* 接收数据包 */
    YMODEM_STATE_COMPLETE,          /* 传输完成 */
    YMODEM_STATE_ERROR,             /* 错误 */
    YMODEM_STATE_ABORTED            /* 已中止 */
} ymodem_state_t;

/* YMODEM错误码 */
typedef enum {
    YMODEM_OK = 0,                  /* 成功 */
    YMODEM_ERR_INVALID_HEADER,      /* 无效帧头 */
    YMODEM_ERR_INVALID_SEQ,         /* 序号错误 */
    YMODEM_ERR_CRC,                 /* CRC校验失败 */
    YMODEM_ERR_LENGTH,              /* 长度错误 */
    YMODEM_ERR_TIMEOUT,             /* 超时 */
    YMODEM_ERR_ABORTED,             /* 已中止 */
    YMODEM_ERR_UNKNOWN              /* 未知错误 */
} ymodem_error_t;

/* YMODEM文件信息 */
typedef struct {
    char filename[256];             /* 文件名 */
    uint32_t filesize;              /* 文件大小 */
    uint32_t received_bytes;        /* 已接收字节数 */
    uint8_t current_seq;            /* 当前包序号 */
} ymodem_file_info_t;

/* YMODEM控制结构 */
typedef struct {
    ymodem_state_t state;           /* 当前状态 */
    ymodem_file_info_t file_info;   /* 文件信息 */
    uint8_t retry_count;            /* 重试计数 */
    uint32_t timeout_counter;       /* 超时计数器 */
} ymodem_ctrl_t;

/* 外部变量 */
extern ymodem_ctrl_t g_ymodem_ctrl;

/* 函数声明 */
void ymodem_init(void);                                         /* 初始化YMODEM */
void ymodem_start_receive(void);                                /* 开始接收 */
ymodem_error_t ymodem_process_packet(uint8_t *data, uint16_t len);  /* 处理数据包 */
void ymodem_abort(void);                                        /* 中止传输 */
ymodem_state_t ymodem_get_state(void);                          /* 获取状态 */
void ymodem_get_file_info(ymodem_file_info_t *info);            /* 获取文件信息 */
uint16_t ymodem_crc16(uint8_t *data, uint16_t len);             /* CRC16计算 */

#endif
