/**
 ****************************************************************************************************
 * @file        ymodem.c
 * @author      Music Player Project
 * @version     V1.0
 * @date        2026-04-01
 * @brief       YMODEM协议实现（接收端）
 ****************************************************************************************************
 */

#include "./YMODEM/ymodem.h"
#include "./SYSTEM/uart_esp32/uart_esp32.h"
#include "string.h"
#include "stdio.h"

/* 全局变量 */
ymodem_ctrl_t g_ymodem_ctrl;

/**
 * @brief       CRC16计算（XMODEM/YMODEM标准）
 * @param       data: 数据指针
 * @param       len: 数据长度
 * @retval      CRC16值
 */
uint16_t ymodem_crc16(uint8_t *data, uint16_t len)
{
    uint16_t crc = 0;
    uint16_t i;
    
    while (len--)
    {
        crc ^= (uint16_t)(*data++) << 8;
        for (i = 0; i < 8; i++)
        {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc = crc << 1;
        }
    }
    return crc;
}

/**
 * @brief       初始化YMODEM
 * @retval      无
 */
void ymodem_init(void)
{
    memset(&g_ymodem_ctrl, 0, sizeof(ymodem_ctrl_t));
    g_ymodem_ctrl.state = YMODEM_STATE_IDLE;
}

/**
 * @brief       开始接收
 * @retval      无
 */
void ymodem_start_receive(void)
{
    g_ymodem_ctrl.state = YMODEM_STATE_WAIT_HEADER;
    g_ymodem_ctrl.retry_count = 0;
    g_ymodem_ctrl.timeout_counter = 0;
    memset(&g_ymodem_ctrl.file_info, 0, sizeof(ymodem_file_info_t));
    
    /* 发送'C'，请求使用CRC16校验 */
    uart_esp32_send_byte(YMODEM_C);
}

/**
 * @brief       处理YMODEM数据包
 * @param       data: 数据指针
 * @param       len: 数据长度
 * @retval      错误码
 */
ymodem_error_t ymodem_process_packet(uint8_t *data, uint16_t len)
{
    uint8_t header;
    uint8_t seq, seq_inv;
    uint16_t data_len;
    uint16_t crc_received, crc_calculated;
    
    /* 检查最小长度 */
    if (len < 1) {
        return YMODEM_ERR_LENGTH;
    }
    
    header = data[0];
    
    /* 处理EOT（传输结束） */
    if (header == YMODEM_EOT)
    {
        uart_esp32_send_byte(YMODEM_ACK);
        g_ymodem_ctrl.state = YMODEM_STATE_COMPLETE;
        return YMODEM_OK;
    }
    
    /* 处理CAN（取消传输） */
    if (header == YMODEM_CAN)
    {
        g_ymodem_ctrl.state = YMODEM_STATE_ABORTED;
        return YMODEM_ERR_ABORTED;
    }
    
    /* 检查帧头并验证长度 */
    if (header == YMODEM_SOH)
    {
        /* 128字节包：1(帧头) + 1(序号) + 1(序号反码) + 128(数据) + 2(CRC) = 133 */
        if (len != YMODEM_PACKET_128_SIZE) {
            uart_esp32_send_byte(YMODEM_NAK);
            return YMODEM_ERR_LENGTH;
        }
        data_len = YMODEM_PACKET_SIZE_128;
    }
    else if (header == YMODEM_STX)
    {
        /* 1024字节包：1(帧头) + 1(序号) + 1(序号反码) + 1024(数据) + 2(CRC) = 1029 */
        if (len != YMODEM_PACKET_1024_SIZE) {
            uart_esp32_send_byte(YMODEM_NAK);
            return YMODEM_ERR_LENGTH;
        }
        data_len = YMODEM_PACKET_SIZE_1024;
    }
    else
    {
        /* 无效帧头 */
        uart_esp32_send_byte(YMODEM_NAK);
        return YMODEM_ERR_INVALID_HEADER;
    }
    
    /* 验证包序号 */
    seq = data[1];
    seq_inv = data[2];
    if (seq != (uint8_t)(~seq_inv))
    {
        uart_esp32_send_byte(YMODEM_NAK);
        return YMODEM_ERR_INVALID_SEQ;
    }
    
    /* CRC16校验 */
    crc_received = (data[len - 2] << 8) | data[len - 1];
    crc_calculated = ymodem_crc16(data + 3, data_len);
    if (crc_received != crc_calculated)
    {
        uart_esp32_send_byte(YMODEM_NAK);
        g_ymodem_ctrl.retry_count++;
        return YMODEM_ERR_CRC;
    }
    
    /* 处理文件头包（序号为0） */
    if (seq == 0)
    {
        if (g_ymodem_ctrl.state == YMODEM_STATE_WAIT_HEADER)
        {
            /* 解析文件名和文件大小 */
            char *filename_ptr = (char *)(data + 3);
            char *filesize_ptr = filename_ptr + strlen(filename_ptr) + 1;
            
            strncpy(g_ymodem_ctrl.file_info.filename, filename_ptr, sizeof(g_ymodem_ctrl.file_info.filename) - 1);
            sscanf(filesize_ptr, "%lu", &g_ymodem_ctrl.file_info.filesize);
            
            g_ymodem_ctrl.file_info.received_bytes = 0;
            g_ymodem_ctrl.file_info.current_seq = 1;
            g_ymodem_ctrl.state = YMODEM_STATE_RECEIVING;
            
            /* 发送ACK和'C'，准备接收数据包 */
            uart_esp32_send_byte(YMODEM_ACK);
            uart_esp32_send_byte(YMODEM_C);
            
            return YMODEM_OK;
        }
    }
    else
    {
        /* 数据包 */
        if (g_ymodem_ctrl.state == YMODEM_STATE_RECEIVING)
        {
            /* 验证序号连续性 */
            if (seq != g_ymodem_ctrl.file_info.current_seq)
            {
                /* 序号不连续，可能是重传的包，检查是否是上一个包 */
                if (seq == (uint8_t)(g_ymodem_ctrl.file_info.current_seq - 1))
                {
                    /* 是重传的包，直接ACK */
                    uart_esp32_send_byte(YMODEM_ACK);
                    return YMODEM_OK;
                }
                else
                {
                    uart_esp32_send_byte(YMODEM_NAK);
                    return YMODEM_ERR_INVALID_SEQ;
                }
            }
            
            /* 数据有效，更新接收字节数 */
            uint16_t actual_data_len = data_len;
            if (g_ymodem_ctrl.file_info.received_bytes + data_len > g_ymodem_ctrl.file_info.filesize)
            {
                /* 最后一包可能不满 */
                actual_data_len = g_ymodem_ctrl.file_info.filesize - g_ymodem_ctrl.file_info.received_bytes;
            }
            
            g_ymodem_ctrl.file_info.received_bytes += actual_data_len;
            g_ymodem_ctrl.file_info.current_seq++;
            
            /* 发送ACK */
            uart_esp32_send_byte(YMODEM_ACK);
            
            /* 注意：实际的数据写入由file_transfer模块处理 */
            /* 这里只返回数据指针偏移：data + 3 */
            
            return YMODEM_OK;
        }
    }
    
    return YMODEM_ERR_UNKNOWN;
}

/**
 * @brief       中止传输
 * @retval      无
 */
void ymodem_abort(void)
{
    /* 发送两个CAN */
    uart_esp32_send_byte(YMODEM_CAN);
    uart_esp32_send_byte(YMODEM_CAN);
    
    g_ymodem_ctrl.state = YMODEM_STATE_ABORTED;
}

/**
 * @brief       获取当前状态
 * @retval      状态
 */
ymodem_state_t ymodem_get_state(void)
{
    return g_ymodem_ctrl.state;
}

/**
 * @brief       获取文件信息
 * @param       info: 文件信息结构指针
 * @retval      无
 */
void ymodem_get_file_info(ymodem_file_info_t *info)
{
    memcpy(info, &g_ymodem_ctrl.file_info, sizeof(ymodem_file_info_t));
}
