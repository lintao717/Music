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
/* 接口函数 */

uint8_t music_player_init(void);        /* 初始化播放器（SD卡挂载 + 扫描文件）*/
void    music_player_run(void);         /* 播放器主循环（不返回）*/

uint16_t music_scan_files(const char *path, uint32_t *offset_tbl, uint16_t max_num);
uint8_t  music_play_song(const char *pname);    /* 播放一首歌，返回 MP_ACTION_* */

void music_volume_up(void);
void music_volume_down(void);

#endif
