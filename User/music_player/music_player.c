/**
 ****************************************************************************************************
 * @file        music_player.c
 * @brief       单机MP3播放器 - 目录扫描 + FatFs读取 + VS1053B解码
 *
 * 使用方法：
 *   1. SD卡根目录建立 MUSIC 文件夹，放入 MP3/WAV/FLAC 等文件
 *   2. 调用 music_player_init() 初始化
 *   3. 调用 music_player_run()  进入播放主循环
 *
 * 按键：KEY0=下一曲  KEY1=上一曲  KEY2=音量+  WKUP=音量-
 ****************************************************************************************************
 */

#include "music_player.h"
#include "./BSP/SRAM/sram.h"
#include "./BSP/SDIO/sdio_sdcard.h"
#include "./BSP/ATK_MO1053/atk_mo1053.h"
#include "./BSP/ATK_MO1053/patch_flac.h"
#include "./BSP/24CXX/24cxx.h"
#include "./BSP/OLED/oled.h"
#include "./BSP/KEY/key.h"
#include "./BSP/LED/led.h"
#include "./FATFS/exfuns/exfuns.h"
#include "./MALLOC/malloc.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "stm32f4xx_hal.h"
#include "string.h"
#include "stdio.h"

/******************************************************************************************/
/* 全局变量 */

music_player_t g_music_player = {0};

volatile uint32_t g_play_byte_offset = 0;   /* 当前4KB块起始文件位置（PVD中断可读） */
volatile uint16_t g_play_file_index  = 0;   /* 当前歌曲索引（PVD中断可读） */

static uint32_t g_resume_offset = 0;        /* 上次断点位置（仅第一首歌用一次） */

/******************************************************************************************/
/* 内部函数声明 */

static uint8_t mp_handle_key(void);
static uint8_t bp_calc_checksum(const Breakpoint_t *bp);
static void mp_oled_show_filename(const char *name);
static void mp_oled_show_state(void);
static void mp_oled_show_progress(uint16_t cur_sec, uint16_t total_sec);

/******************************************************************************************/

/**
 * @brief  扫描指定路径下的音乐文件，将目录偏移记录到 offset_tbl
 * @param  path       : 目录路径，如 "0:/MUSIC"
 * @param  offset_tbl : 存放每个音乐文件在目录中的 dptr 偏移（uint16_t数组）
 * @param  max_num    : 最多扫描文件数
 * @retval 找到的音乐文件总数
 */
uint16_t music_scan_files(const char *path, uint32_t *offset_tbl, uint16_t max_num)
{
    DIR     dir;
    FILINFO *finfo;
    uint16_t count = 0;
    uint8_t  ftype;

    finfo = (FILINFO *)mymalloc(SRAMIN, sizeof(FILINFO));
    if (!finfo) return 0;

    if (f_opendir(&dir, path) != FR_OK)
    {
        myfree(SRAMIN, finfo);
        return 0;
    }

    while (count < max_num)
    {
        uint32_t dptr_save = dir.dptr;
        if (f_readdir(&dir, finfo) != FR_OK || finfo->fname[0] == 0) break;

        ftype = exfuns_file_type(finfo->fname);
        if ((ftype & 0xF0) == 0x40)    /* 音频文件类 */
        {
            offset_tbl[count] = dptr_save;
            count++;
        }
    }

    f_closedir(&dir);
    myfree(SRAMIN, finfo);
    return count;
}

/**
 * @brief  播放一首歌曲
 * @param  pname : 完整路径文件名，如 "0:/MUSIC/test.mp3"
 * @retval MP_ACTION_NEXT / MP_ACTION_PREV / MP_ACTION_STOP / MP_ACTION_NONE(出错)
 */
uint8_t music_play_song(const char *pname)
{
    FIL     *fp;
    uint8_t *buf;
    UINT     br;
    uint8_t  res, action;
    uint8_t  ftype;
    uint16_t i;
    uint8_t  key;
    uint32_t playtime_last = 0;
    uint32_t chunk_start   = 0;   /* 记录本次 f_read 前的文件指针，即 4KB 块起始位置 */
    uint32_t dreq_low_cnt  = 0;   /* 统计DREQ低电平次数，用于调试 */

    action = MP_ACTION_NONE;

    fp  = (FIL *)mymalloc(SRAMIN, sizeof(FIL));
    buf = (uint8_t *)mymalloc(SRAMIN, MUSIC_READ_BUF_SIZE);

    if (!fp || !buf)
    {
        printf("[MP] malloc failed\r\n");
        myfree(SRAMIN, fp);
        myfree(SRAMIN, buf);
        return MP_ACTION_NEXT;
    }

    /* 重启 VS1053 准备播放新歌 */
    atk_mo1053_restart_play();
    atk_mo1053_set_all();
    atk_mo1053_reset_decode_time();

    /* FLAC 需要加载 patch */
    ftype = exfuns_file_type((char *)pname);
    if (ftype == T_FLAC)
    {
        atk_mo1053_load_patch((uint16_t *)vs1053b_patch, VS1053B_PATCHLEN);
    }

    res = f_open(fp, pname, FA_READ);
    if (res != FR_OK)
    {
        printf("[MP] f_open failed: %d  file: %s\r\n", res, pname);
        myfree(SRAMIN, fp);
        myfree(SRAMIN, buf);
        return MP_ACTION_NEXT;
    }

    g_music_player.file_size = fp->obj.objsize;
    g_music_player.state     = MP_STATE_PLAYING;

    /* 断点续播：跳转到上次保存的 4KB 块起始位置 */
    if (g_resume_offset > 0)
    {
        f_lseek(fp, g_resume_offset);
        printf("[BP] Seek to offset %lu\r\n", (unsigned long)g_resume_offset);
        g_resume_offset = 0;
    }

    atk_mo1053_spi_speed_high();

    printf("[MP] Playing: %s  (%lu bytes)\r\n", pname, (unsigned long)g_music_player.file_size);

    while (1)
    {
        chunk_start = f_tell(fp);           /* 保存本块起始位置 */
        g_play_byte_offset = chunk_start;   /* 同步到 volatile 全局变量，PVD 中断可读 */
        res = f_read(fp, buf, MUSIC_READ_BUF_SIZE, &br);

        /* ---- 外层按键轮询：每读一块就检查，不依赖DREQ状态 ---- */
        key = mp_handle_key();
        if (key != MP_ACTION_NONE)
        {
            action = key;
            goto play_done;
        }

        i = 0;
        while (i < br)
        {
            if (atk_mo1053_send_music_data(buf + i) == 0)
            {
                i += 32;
            }
            else
            {
                /* VS1053 数据缓冲区暂满（DREQ=LOW） */
                dreq_low_cnt++;
                if (dreq_low_cnt == 1)
                {
                    printf("[DBG] DREQ went LOW (DREQ is working)\r\n");
                }

                key = mp_handle_key();
                if (key != MP_ACTION_NONE)
                {
                    action = key;
                    goto play_done;
                }

                /* 每秒更新一次进度 */
                {
                    uint32_t cur = atk_mo1053_get_decode_time();
                    if (cur != playtime_last)
                    {
                        playtime_last = cur;
                        uint16_t total_sec = 0;
                        uint16_t bitrate = atk_mo1053_get_bitrate();
                        if (bitrate)
                            total_sec = (uint16_t)((g_music_player.file_size / bitrate) / 125);
                        mp_oled_show_progress((uint16_t)cur, total_sec);
                        printf("[MP] %02lu:%02lu / %02u:%02u  %ukbps\r\n",
                               cur / 60, cur % 60,
                               total_sec / 60, total_sec % 60,
                               bitrate);
                        LED0_TOGGLE();
                    }
                }
            }
        }

        /* 文件读完或出错 */
        if (br < MUSIC_READ_BUF_SIZE || res != FR_OK)
        {
            action = MP_ACTION_NEXT;
            break;
        }
    }

play_done:
    f_close(fp);
    myfree(SRAMIN, fp);
    myfree(SRAMIN, buf);
    g_music_player.state = MP_STATE_STOPPED;
    return action;
}

/**
 * @brief  初始化播放器：挂载SD卡，扫描 MUSIC 文件夹
 * @retval 0=成功, 1=失败
 */
uint8_t music_player_init(void)
{
    uint8_t ret;

    /* 初始化外部SRAM（内存管理依赖） */
    sram_init();

    /* 初始化内存管理 */
    my_mem_init(SRAMIN);
    my_mem_init(SRAMEX);

    /* 为 FatFs 申请工作区内存 */
    ret = exfuns_init();
    if (ret)
    {
        printf("[MP] exfuns_init failed\r\n");
        return 1;
    }

    /* 等待 SD 卡上电稳定，然后重试初始化 */
    {
        uint8_t sd_err = 1;
        uint8_t retry;
        for (retry = 0; retry < 5 && sd_err != 0; retry++)
        {
            delay_ms(500);
            sd_err = sd_init();
            printf("[MP] sd_init() try%d = %d\r\n", retry + 1, sd_err);
        }
        if (sd_err != 0)
        {
            printf("[MP] SD HW FAIL\r\n");
            return 1;
        }
    }

    /* 挂载 SD 卡（卷号 0） */
    {
        FRESULT fr = f_mount(fs[0], "0:", 1);
        printf("[MP] f_mount() = %d\r\n", fr);
        if (fr != FR_OK)
        {
            printf("[MP] SD card mount failed!\r\n");
            return 1;
        }
    }

    printf("[MP] SD card mounted OK\r\n");

    /* 初始化 VS1053B */
    ret = atk_mo1053_init();
    if (ret)
    {
        printf("[MP] VS1053 init failed!\r\n");
        return 1;
    }

    atk_mo1053_reset();
    atk_mo1053_soft_reset();

    /* 默认音量 */
    g_music_player.volume = MUSIC_VOLUME_DEFAULT;
    atk_mo1053_set_volume(g_music_player.volume);
    atk_mo1053_set_all();

    printf("[MP] VS1053 init OK, vol=%d\r\n", g_music_player.volume);

    /* 配置 PVD 掉电保护中断 */
    pvd_init();

    g_music_player.state = MP_STATE_IDLE;
    return 0;
}

/**
 * @brief  播放器主循环（不返回）
 *         自动顺序播放 MUSIC 目录下所有音频文件，支持按键切换
 */
void music_player_run(void)
{
    DIR      dir;
    FILINFO *finfo;
    uint32_t *offset_tbl;
    char    *pname;
    uint8_t  action;

    /* 检查 MUSIC 目录 */
    while (f_opendir(&dir, MUSIC_PATH) != FR_OK)
    {
        printf("[MP] MUSIC folder not found, retrying...\r\n");
        delay_ms(1000);
    }
    f_closedir(&dir);

    /* 申请内存 */
    finfo      = (FILINFO *)mymalloc(SRAMIN, sizeof(FILINFO));
    pname      = (char *)mymalloc(SRAMIN, FF_MAX_LFN + 16);
    offset_tbl = (uint32_t *)mymalloc(SRAMIN, MUSIC_MAX_FILES * sizeof(uint32_t));

    if (!finfo || !pname || !offset_tbl)
    {
        printf("[MP] memory alloc failed\r\n");
        while (1) { LED0_TOGGLE(); delay_ms(200); }
    }

    /* 扫描文件 */
    g_music_player.total = music_scan_files(MUSIC_PATH, offset_tbl, MUSIC_MAX_FILES);

    if (g_music_player.total == 0)
    {
        printf("[MP] No music files in %s\r\n", MUSIC_PATH);
        while (1) { LED0_TOGGLE(); delay_ms(500); }
    }

    printf("[MP] Found %d music file(s)\r\n", g_music_player.total);

    g_music_player.index = 0;

    /* 断点续播：从 EEPROM 读取并校验断点 */
    {
        Breakpoint_t bp;
        if (bp_load(&bp) == 0
            && bp.file_index < g_music_player.total
            && bp.volume >= 100 && bp.volume <= 250)
        {
            g_music_player.index  = bp.file_index;
            g_music_player.volume = bp.volume;
            atk_mo1053_set_volume(bp.volume);
            g_resume_offset = bp.byte_offset;
            printf("[BP] Resume: idx=%d offset=%lu vol=%d\r\n",
                   bp.file_index,
                   (unsigned long)bp.byte_offset,
                   bp.volume);
            bp_invalidate();
        }
        else
        {
            printf("[BP] No valid breakpoint, start from beginning\r\n");
        }
    }

    /* ---- 主播放循环 ---- */
    while (1)
    {
        /* 重新打开目录，跳转到当前文件的偏移 */
        if (f_opendir(&dir, MUSIC_PATH) != FR_OK) continue;
        dir_sdi(&dir, offset_tbl[g_music_player.index]);
        if (f_readdir(&dir, finfo) != FR_OK || finfo->fname[0] == 0)
        {
            f_closedir(&dir);
            g_music_player.index = 0;
            continue;
        }
        f_closedir(&dir);

        /* 保存文件名并拼接完整路径 */
        strncpy(g_music_player.file_name, finfo->fname, FF_MAX_LFN);
        snprintf(pname, FF_MAX_LFN + 16, "%s/%s", MUSIC_PATH, finfo->fname);

        /* 同步歌曲索引到 volatile 变量，供 PVD 中断读取 */
        g_play_file_index  = g_music_player.index;
        g_play_byte_offset = 0;

        /* OLED 显示曲目信息 */
        mp_oled_show_filename(finfo->fname);
        mp_oled_show_state();

        printf("[MP] [%d/%d] %s\r\n",
               g_music_player.index + 1,
               g_music_player.total,
               pname);

        /* 播放 */
        action = music_play_song(pname);

        /* 按键动作切歌 */
        switch (action)
        {
            case MP_ACTION_NEXT:
                g_music_player.index++;
                if (g_music_player.index >= g_music_player.total)
                    g_music_player.index = 0;
                break;

            case MP_ACTION_PREV:
                if (g_music_player.index > 0)
                    g_music_player.index--;
                else
                    g_music_player.index = g_music_player.total - 1;
                break;

            default:
                /* 错误或其他情况：自动下一曲 */
                g_music_player.index++;
                if (g_music_player.index >= g_music_player.total)
                    g_music_player.index = 0;
                break;
        }
        /* 每首歌播完后更新断点信息（正常挂起保留副本） */
        bp_save();

        delay_ms(100);
    }
}

/******************************************************************************************/
/* OLED 显示辅助函数 */

static void mp_oled_show_filename(const char *name)
{
    char buf[22];
    oled_clear();
    oled_show_string(0, 0, ">> MP3 Player <<");
    strncpy(buf, name, 21);
    buf[21] = '\0';
    oled_show_string(0, 2, buf);
    oled_refresh();
}

static void mp_oled_show_state(void)
{
    char buf[22];
    snprintf(buf, sizeof(buf), "%d/%d Vol:%d",
             g_music_player.index + 1,
             g_music_player.total,
             g_music_player.volume);
    oled_show_string(0, 4, buf);
    oled_refresh();
}

static void mp_oled_show_progress(uint16_t cur_sec, uint16_t total_sec)
{
    char buf[22];
    snprintf(buf, sizeof(buf), "%02u:%02u / %02u:%02u",
             cur_sec / 60, cur_sec % 60,
             total_sec / 60, total_sec % 60);
    oled_show_string(0, 6, buf);
    oled_refresh();
}

/******************************************************************************************/
/* 断点续播功能 */

/**
 * @brief  计算断点数据校验和（各字节异或）
 */
static uint8_t bp_calc_checksum(const Breakpoint_t *bp)
{
    const uint8_t *p = (const uint8_t *)bp;
    uint8_t  cs = 0;
    uint8_t  i;
    /* 校验 magic(4) + file_index(2) + byte_offset(4) + volume(1) = 11 字节 */
    for (i = 0; i < (sizeof(Breakpoint_t) - 1); i++)
    {
        cs ^= p[i];
    }
    return cs;
}

/**
 * @brief  将当前播放状态写入 EEPROM（不使用 printf，适合 PVD 中断上下文）
 */
void bp_save_isr(void)
{
    Breakpoint_t bp;
    bp.magic       = BREAKPOINT_MAGIC;
    bp.file_index  = g_play_file_index;
    bp.byte_offset = g_play_byte_offset;
    bp.volume      = g_music_player.volume;
    bp.checksum    = bp_calc_checksum(&bp);
    at24cxx_page_write(BREAKPOINT_EEPROM_ADDR,     (uint8_t *)&bp,      8);
    at24cxx_page_write(BREAKPOINT_EEPROM_ADDR + 8, (uint8_t *)&bp + 8, (uint8_t)(sizeof(Breakpoint_t) - 8));
}

/**
 * @brief  将当前播放状态写入 EEPROM（带日志，常规上下文使用）
 *   @note  使用2次页写（8+4字节），总耗时约10ms
 */
void bp_save(void)
{
    Breakpoint_t bp;
    bp.magic       = BREAKPOINT_MAGIC;
    bp.file_index  = g_play_file_index;
    bp.byte_offset = g_play_byte_offset;
    bp.volume      = g_music_player.volume;
    bp.checksum    = bp_calc_checksum(&bp);

    /* AT24C02 页大小 = 8 字节：先写前8字节，再写后4字节 */
    at24cxx_page_write(BREAKPOINT_EEPROM_ADDR,     (uint8_t *)&bp,      8);
    at24cxx_page_write(BREAKPOINT_EEPROM_ADDR + 8, (uint8_t *)&bp + 8, (uint8_t)(sizeof(Breakpoint_t) - 8));

    printf("[BP] Saved: idx=%d offset=%lu vol=%d\r\n",
           bp.file_index,
           (unsigned long)bp.byte_offset,
           bp.volume);
}

/**
 * @brief  从 EEPROM 读取并校验断点
 * @retval 0=有效, 1=无效
 */
uint8_t bp_load(Breakpoint_t *bp)
{
    at24cxx_read(BREAKPOINT_EEPROM_ADDR, (uint8_t *)bp, sizeof(Breakpoint_t));
    if (bp->magic != BREAKPOINT_MAGIC)
    {
        return 1;
    }
    if (bp->checksum != bp_calc_checksum(bp))
    {
        printf("[BP] Checksum mismatch\r\n");
        return 1;
    }
    return 0;
}

/**
 * @brief  清除 EEPROM 中的断点 magic，防止下次重复续播
 */
void bp_invalidate(void)
{
    uint32_t zero = 0;
    at24cxx_write(BREAKPOINT_EEPROM_ADDR, (uint8_t *)&zero, 4);
    printf("[BP] Breakpoint invalidated\r\n");
}

/**
 * @brief  配置 PVD（可编程电压检测器），阈値约2.9V
 */
void pvd_init(void)
{
    PWR_PVDTypeDef pvd_cfg = {0};
    __HAL_RCC_PWR_CLK_ENABLE();
    pvd_cfg.PVDLevel = PWR_PVDLEVEL_6;            /* 阈値约2.9V */
    pvd_cfg.Mode    = PWR_PVD_MODE_IT_RISING;     /* 上升沿（电压下降至阈値时触发） */
    HAL_PWR_ConfigPVD(&pvd_cfg);
    HAL_NVIC_SetPriority(PVD_IRQn, 0, 0);        /* 最高优先级 */
    HAL_NVIC_EnableIRQ(PVD_IRQn);
    HAL_PWR_EnablePVD();
    printf("[PVD] Configured at ~2.9V\r\n");
}

/**
 * @brief  处理按键并返回动作
 * @retval MP_ACTION_* 枚举值，无按键时返回 MP_ACTION_NONE
 */
static uint8_t mp_handle_key(void)
{
    uint8_t key = key_scan(0);

    switch (key)
    {
        case KEY0_PRES:     /* PF1 → 上一曲 */
            return MP_ACTION_PREV;

        case KEY1_PRES:     /* PF2 → 音量+ */
            music_volume_up();
            return MP_ACTION_NONE;

        case KEY2_PRES:     /* PF3 → 下一曲 */
            return MP_ACTION_NEXT;

        default:
            return MP_ACTION_NONE;
    }
}

void music_volume_up(void)
{
    if (g_music_player.volume < 250)
    {
        g_music_player.volume += 5;
        atk_mo1053_set_volume(g_music_player.volume);
        printf("[MP] Vol+ => %d\r\n", g_music_player.volume);
        mp_oled_show_state();
    }
}

void music_volume_down(void)
{
    if (g_music_player.volume >= 5)
    {
        g_music_player.volume -= 5;
        atk_mo1053_set_volume(g_music_player.volume);
        printf("[MP] Vol- => %d\r\n", g_music_player.volume);
        mp_oled_show_state();
    }
}
