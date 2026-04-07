#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/LED/led.h"
#include "./BSP/KEY/key.h"
#include "./BSP/24CXX/24cxx.h"
#include "music_player.h"


int main(void)
{
    HAL_Init();                                 /* 初始化HAL库 */
    sys_stm32_clock_init(336, 8, 2, 7);         /* 设置时钟,168Mhz */
    delay_init(168);                            /* 延时初始化 */
    usart_init(115200);                         /* 串口初始化为115200 */
    led_init();                                 /* 初始化LED */
    key_init();                                 /* 初始化按键 */
    at24cxx_init();                             /* 初始化EEPROM(AT24C02) */

    printf("\r\n========== MP3 Player ==========\r\n");

    /* 初始化播放器（内存 + FatFs挂载 + VS1053初始化） */
    if (music_player_init() != 0)
    {
        printf("music_player_init FAILED\r\n");
        while (1) { LED0_TOGGLE(); delay_ms(500); }
    }

    printf("Init OK. Starting playback...\r\n");

    /* 进入播放主循环（不返回） */
    music_player_run();
}

