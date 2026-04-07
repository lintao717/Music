#ifndef __OLED_H
#define __OLED_H

#include "./SYSTEM/sys/sys.h"

/* OLED parameters */
#define OLED_WIDTH                128
#define OLED_HEIGHT               64

/* 7-bit I2C address (0x3C or 0x3D are common) */
#define OLED_I2C_ADDR             0x3C

/* Controller selection: 0 = SSD1306, 1 = SH1106 */
#define OLED_CTRL_SH1106          0

#if OLED_CTRL_SH1106
#define OLED_COLUMN_OFFSET        2
#else
#define OLED_COLUMN_OFFSET        0
#endif

#define OLED_COLOR_BLACK          0
#define OLED_COLOR_WHITE          1

#define OLED_DRAW_NORMAL          0
#define OLED_DRAW_OVERLAY         1

void oled_init(void);
void oled_clear(void);
void oled_fill(uint8_t color);
void oled_refresh(void);
void oled_draw_pixel(uint8_t x, uint8_t y, uint8_t color);

void oled_draw_hline(uint8_t x, uint8_t y, uint8_t len, uint8_t color);
void oled_draw_vline(uint8_t x, uint8_t y, uint8_t len, uint8_t color);
void oled_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color);
void oled_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void oled_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);
void oled_draw_circle(uint8_t x0, uint8_t y0, uint8_t r, uint8_t color);
void oled_draw_bitmap(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bitmap);

/* Page-based ASCII (8x16) helpers */
void oled_show_char(uint8_t x, uint8_t page, char ch);
void oled_show_string(uint8_t x, uint8_t page, const char *str);

/* Pixel-based ASCII */
void oled_draw_char(uint8_t x, uint8_t y, char ch, uint8_t size, uint8_t mode);
void oled_draw_string(uint8_t x, uint8_t y, const char *str, uint8_t size, uint8_t mode);

/* Chinese GBK (requires NORFLASH font library) */
uint8_t oled_chinese_init(void);
void oled_show_chinese(uint8_t x, uint8_t y, const uint8_t *gbk, uint8_t size, uint8_t mode);
void oled_show_string_gbk(uint8_t x, uint8_t y, uint8_t width, uint8_t height, const char *str, uint8_t size, uint8_t mode);

#endif
