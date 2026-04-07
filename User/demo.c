/**
 * Hardware Test Program for STM32F407
 * Tests: KEY, OLED, AT24C02, SD Card
 */

#include "./SYSTEM/delay/delay.h"
#include "./SYSTEM/usart/usart.h"
#include "./BSP/LED/led.h"
#include "./BSP/KEY/key.h"
#include "./BSP/OLED/oled.h"
#include "./BSP/24CXX/24cxx.h"
#include "./BSP/SDIO/sdio_sdcard.h"
#include "string.h"
#include "stdio.h"

/* Test status structure */
typedef struct {
    uint8_t key_ok;
    uint8_t oled_ok;
    uint8_t eeprom_ok;
    uint8_t sd_ok;
} test_status_t;

static test_status_t g_test_status = {0};

/**
 * @brief  Test KEY buttons
 * @retval 0-success, 1-fail
 */
static uint8_t test_key(void)
{
    uint8_t key;
    uint8_t test_count = 0;
    
    printf("\r\n=== KEY Test ===");
    printf("\r\nPress KEY0, KEY1, or KEY2 to test...");
    printf("\r\nWaiting for key press (timeout 10s)...\r\n");
    
    while (test_count < 100)
    {
        key = key_scan(0);
        
        if (key == KEY0_PRES)
        {
            printf("KEY0 Pressed - OK!\r\n");
            LED0_TOGGLE();
            return 0;
        }
        else if (key == KEY1_PRES)
        {
            printf("KEY1 Pressed - OK!\r\n");
            LED0_TOGGLE();
            return 0;
        }
        else if (key == KEY2_PRES)
        {
            printf("KEY2 Pressed - OK!\r\n");
            LED0_TOGGLE();
            return 0;
        }
        
        delay_ms(100);
        test_count++;
    }
    
    printf("KEY Test TIMEOUT - FAIL!\r\n");
    return 1;
}

/**
 * @brief  Test OLED display
 * @retval 0-success, 1-fail
 */
static uint8_t test_oled(void)
{
    printf("\r\n=== OLED Test ===");
    printf("\r\nTesting OLED display...\r\n");
    
    /* Clear screen */
    oled_clear();
    oled_refresh();
    delay_ms(200);
    
    /* Test fill */
    oled_fill(OLED_COLOR_WHITE);
    oled_refresh();
    delay_ms(500);
    
    oled_clear();
    oled_refresh();
    delay_ms(200);
    
    /* Draw rectangles */
    oled_draw_rect(10, 10, 108, 44, OLED_COLOR_WHITE);
    oled_draw_rect(12, 12, 104, 40, OLED_COLOR_WHITE);
    oled_refresh();
    delay_ms(500);
    
    /* Draw circles */
    oled_clear();
    oled_draw_circle(64, 32, 20, OLED_COLOR_WHITE);
    oled_draw_circle(64, 32, 15, OLED_COLOR_WHITE);
    oled_refresh();
    delay_ms(500);
    
    /* Draw lines */
    oled_clear();
    oled_draw_line(0, 0, 127, 63, OLED_COLOR_WHITE);
    oled_draw_line(0, 63, 127, 0, OLED_COLOR_WHITE);
    oled_refresh();
    delay_ms(500);
    
    oled_clear();
    oled_refresh();
    
    printf("OLED Test - OK!\r\n");
    return 0;
}

/**
 * @brief  Test AT24C02 EEPROM
 * @retval 0-success, 1-fail
 */
static uint8_t test_eeprom(void)
{
    uint8_t write_buf[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08,
                              0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    uint8_t read_buf[16] = {0};
    uint8_t i;
    uint8_t result;
    
    printf("\r\n=== AT24C02 EEPROM Test ===");
    printf("\r\nChecking AT24C02...\r\n");
    
    /* Check AT24C02 */
    result = at24cxx_check();
    if (result != 0)
    {
        printf("AT24C02 Not Found - FAIL!\r\n");
        return 1;
    }
    
    printf("AT24C02 Found!\r\n");
    
    /* Write test data */
    printf("Writing test data...\r\n");
    at24cxx_write(0, write_buf, 16);
    delay_ms(10);
    
    /* Read data */
    printf("Reading data...\r\n");
    at24cxx_read(0, read_buf, 16);
    
    /* Compare data */
    printf("Verifying data...\r\n");
    for (i = 0; i < 16; i++)
    {
        if (write_buf[i] != read_buf[i])
        {
            printf("Data mismatch at byte %d: wrote 0x%02X, read 0x%02X - FAIL!\r\n", 
                   i, write_buf[i], read_buf[i]);
            return 1;
        }
    }
    
    printf("AT24C02 Test - OK!\r\n");
    return 0;
}

/**
 * @brief  Test SD card
 * @retval 0-success, 1-fail
 */
static uint8_t test_sdcard(void)
{
    uint8_t result;
    HAL_SD_CardInfoTypeDef card_info;
    uint32_t total_size_mb;
    
    printf("\r\n=== SD Card Test ===");
    printf("\r\nInitializing SD card...\r\n");
    
    /* Initialize SD card */
    result = sd_init();
    if (result != 0)
    {
        printf("SD Card Init Failed (Error: %d) - FAIL!\r\n", result);
        return 1;
    }
    
    printf("SD Card Init - OK!\r\n");
    
    /* Get SD card info */
    result = get_sd_card_info(&card_info);
    if (result == 0)
    {
        total_size_mb = SD_TOTAL_SIZE_MB(&g_sdcard_handler);
        printf("SD Card Info:\r\n");
        printf("  Card Type: %d\r\n", card_info.CardType);
        printf("  Card Version: %d\r\n", card_info.CardVersion);
        printf("  Block Size: %d bytes\r\n", card_info.BlockSize);
        printf("  Block Count: %d\r\n", card_info.BlockNbr);
        printf("  Capacity: %d MB\r\n", total_size_mb);
    }
    
    printf("SD Card Test - OK!\r\n");
    return 0;
}

/**
 * @brief  Display test results on OLED
 */
static void display_test_results(void)
{
    oled_clear();
    
    /* Draw title box */
    oled_draw_rect(0, 0, 128, 16, OLED_COLOR_WHITE);
    oled_draw_line(0, 16, 127, 16, OLED_COLOR_WHITE);
    
    /* Draw dividers */
    oled_draw_line(0, 32, 127, 32, OLED_COLOR_WHITE);
    oled_draw_line(0, 48, 127, 48, OLED_COLOR_WHITE);
    oled_draw_line(64, 16, 64, 64, OLED_COLOR_WHITE);
    
    /* Display status indicators */
    /* KEY status - top left */
    if (g_test_status.key_ok)
        oled_fill_rect(5, 18, 8, 8, OLED_COLOR_WHITE);
    else
        oled_draw_rect(5, 18, 8, 8, OLED_COLOR_WHITE);
    
    /* OLED status - middle left */
    if (g_test_status.oled_ok)
        oled_fill_rect(5, 34, 8, 8, OLED_COLOR_WHITE);
    else
        oled_draw_rect(5, 34, 8, 8, OLED_COLOR_WHITE);
    
    /* EEPROM status - bottom left */
    if (g_test_status.eeprom_ok)
        oled_fill_rect(5, 50, 8, 8, OLED_COLOR_WHITE);
    else
        oled_draw_rect(5, 50, 8, 8, OLED_COLOR_WHITE);
    
    /* SD Card status - top right */
    if (g_test_status.sd_ok)
        oled_fill_rect(70, 18, 8, 8, OLED_COLOR_WHITE);
    else
        oled_draw_rect(70, 18, 8, 8, OLED_COLOR_WHITE);
    
    oled_refresh();
}

/**
 * @brief  Hardware test main function
 */
void hardware_test_run(void)
{
    uint8_t all_pass = 1;
    
    printf("\r\n\r\n");
    printf("========================================\r\n");
    printf("  STM32F407 Hardware Test Program\r\n");
    printf("========================================\r\n");
    printf("Testing: KEY, OLED, AT24C02, SD Card\r\n");
    printf("========================================\r\n");
    
    /* Test OLED */
    if (test_oled() == 0)
    {
        g_test_status.oled_ok = 1;
    }
    else
    {
        g_test_status.oled_ok = 0;
        all_pass = 0;
    }
    
    /* Test AT24C02 */
    if (test_eeprom() == 0)
    {
        g_test_status.eeprom_ok = 1;
    }
    else
    {
        g_test_status.eeprom_ok = 0;
        all_pass = 0;
    }
    
    /* Test SD Card */
    if (test_sdcard() == 0)
    {
        g_test_status.sd_ok = 1;
    }
    else
    {
        g_test_status.sd_ok = 0;
        all_pass = 0;
    }
    
    /* Test KEY */
    if (test_key() == 0)
    {
        g_test_status.key_ok = 1;
    }
    else
    {
        g_test_status.key_ok = 0;
        all_pass = 0;
    }
    
    /* Display test results */
    printf("\r\n========================================\r\n");
    printf("  Test Results Summary\r\n");
    printf("========================================\r\n");
    printf("OLED    : %s\r\n", g_test_status.oled_ok ? "PASS" : "FAIL");
    printf("AT24C02 : %s\r\n", g_test_status.eeprom_ok ? "PASS" : "FAIL");
    printf("SD Card : %s\r\n", g_test_status.sd_ok ? "PASS" : "FAIL");
    printf("KEY     : %s\r\n", g_test_status.key_ok ? "PASS" : "FAIL");
    printf("========================================\r\n");
    
    if (all_pass)
    {
        printf("All Tests PASSED!\r\n");
    }
    else
    {
        printf("Some Tests FAILED!\r\n");
    }
    printf("========================================\r\n\r\n");
    
    /* Display results on OLED */
    display_test_results();
    
    /* LED blink after test completion */
    while (1)
    {
        if (all_pass)
        {
            LED0_TOGGLE();
            delay_ms(200);  /* Fast blink = all pass */
        }
        else
        {
            LED0_TOGGLE();
            delay_ms(1000);  /* Slow blink = some failed */
        }
    }
}
