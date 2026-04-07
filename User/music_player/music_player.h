/**
 ****************************************************************************************************
 * @file        music_player.h
 * @brief       单机MP3播放器模块 - 目录扫描 + VS1053B播放控制
 ****************************************************************************************************
 */

#ifndef __MUSIC_PLAYER_H
#define __MUSIC_PLAYER_H

#include "./SYSTEM/sys/sys.h"
#include "./FATFS/source/ff.h"

/******************************************************************************************/
/* 配置参数 */

#define MUSIC_PATH              "0:/MUSIC"          /* SD卡音乐目录 */
#define MUSIC_READ_BUF_SIZE     4096                /* 单次读取缓冲区大小 (字节) */
#define MUSIC_MAX_FILES         200                 /* 最大支持文件数 */
#define MUSIC_VOLUME_DEFAULT    220                 /* 默认音量（100~250）*/

/******************************************************************************************/
/* 播放器状态 */

typedef enum
{
    MP_STATE_IDLE    = 0,   /* 空闲 */
    MP_STATE_PLAYING = 1,   /* 正在播放 */
    MP_STATE_PAUSED  = 2,   /* 暂停 */
    MP_STATE_STOPPED = 3,   /* 停止 */
} mp_state_t;

/* 按键动作返回值 */
#define MP_ACTION_NONE      0
#define MP_ACTION_NEXT      1   /* 下一曲 */
#define MP_ACTION_PREV      2   /* 上一曲 */
#define MP_ACTION_STOP      3   /* 停止/退出 */

/******************************************************************************************/
/* 播放器控制结构 */

typedef struct
{
    uint16_t    total;          /* 音乐文件总数 */
    uint16_t    index;          /* 当前播放索引 (0-based) */
    mp_state_t  state;          /* 播放器状态 */
    uint8_t     volume;         /* 当前音量 (100~250, 显示值 = (vol-100)/5, 即 0~30) */
    uint32_t    file_size;      /* 当前文件大小 (字节) */
    char        file_name[FF_MAX_LFN + 1];  /* 当前文件名 */
} music_player_t;

extern music_player_t g_music_player;

/******************************************************************************************/
/* 断点续播数据结构 */

#define BREAKPOINT_MAGIC    0x5AA5A55AUL    /* 有效断点标记 */
#define BREAKPOINT_EEPROM_ADDR  0           /* 存储在 AT24C02 的起始地址 */

typedef __PACKED_STRUCT
{
    uint32_t magic;         /* 魔数 0x5AA5A55A，判断数据是否有效 */
    uint16_t file_index;    /* 当前歌曲在列表中的索引 */
    uint32_t byte_offset;   /* 歌曲当前播放进度（4KB块起始字节偏移） */
    uint8_t  volume;        /* 当前音量 */
    uint8_t  checksum;      /* 各字节异或校验 */
} Breakpoint_t;             /* 共 12 字节，2次页写完成，约10ms */

/* 供 PVD 中断处理器访问的播放状态（volatile 保证中断可见性） */
extern volatile uint32_t g_play_byte_offset;    /* 当前4KB块起始文件位置 */
extern volatile uint16_t g_play_file_index;     /* 当前歌曲索引 */

/******************************************************************************************/
/* 接口函数 */

uint8_t music_player_init(void);        /* 初始化播放器（SD卡挂载 + 扫描文件）*/
void    music_player_run(void);         /* 播放器主循环（不返回）*/

uint16_t music_scan_files(const char *path, uint32_t *offset_tbl, uint16_t max_num);
uint8_t  music_play_song(const char *pname);    /* 播放一首歌，返回 MP_ACTION_* */

void music_volume_up(void);
void music_volume_down(void);

void bp_save(void);             /* 将当前状态写入 EEPROM（带日志） */
void bp_save_isr(void);         /* 将当前状态写入 EEPROM（无日志，ISR专用）*/
uint8_t bp_load(Breakpoint_t *bp);  /* 读取并校验，0=有效 */
void bp_invalidate(void);       /* 清除 magic（正常播放结束时调用）*/
void pvd_init(void);            /* 配置PVD电压检测中断 */

#endif
