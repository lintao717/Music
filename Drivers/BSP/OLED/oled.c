#include "./BSP/OLED/oled.h"
#include "./SYSTEM/delay/delay.h"
#include "string.h"

#define OLED_TIMEOUT_MS  100

static I2C_HandleTypeDef hi2c1;
static uint8_t oled_buffer[OLED_WIDTH * (OLED_HEIGHT / 8)];
static uint8_t oled_font_ready = 0;

static void oled_write_cmd(uint8_t cmd)
{
    uint8_t data[2];
    data[0] = 0x00;   /* Co = 0, D/C# = 0 */
    data[1] = cmd;
    HAL_I2C_Master_Transmit(&hi2c1, (OLED_I2C_ADDR << 1), data, 2, OLED_TIMEOUT_MS);
}

static void oled_write_data(uint8_t *buf, uint16_t len)
{
    /* I2C data stream starts with control byte 0x40 */
    uint8_t tx[1 + OLED_WIDTH];
    if (len > OLED_WIDTH)
    {
        len = OLED_WIDTH;
    }
    tx[0] = 0x40;
    for (uint16_t i = 0; i < len; i++)
    {
        tx[1 + i] = buf[i];
    }
    HAL_I2C_Master_Transmit(&hi2c1, (OLED_I2C_ADDR << 1), tx, (uint16_t)(len + 1), OLED_TIMEOUT_MS);
}

static void oled_i2c_init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 400000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    GPIO_InitTypeDef gpio_init_struct;

    if (hi2c->Instance == I2C1)
    {
        __HAL_RCC_I2C1_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        gpio_init_struct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
        gpio_init_struct.Mode = GPIO_MODE_AF_OD;
        gpio_init_struct.Pull = GPIO_PULLUP;
        gpio_init_struct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio_init_struct.Alternate = GPIO_AF4_I2C1;
        HAL_GPIO_Init(GPIOB, &gpio_init_struct);
    }
}

void oled_init(void)
{
    oled_i2c_init();
    delay_ms(100);

    oled_write_cmd(0xAE); /* Display OFF */
    oled_write_cmd(0xD5); /* Set display clock divide ratio/oscillator frequency */
    oled_write_cmd(0x80);
    oled_write_cmd(0xA8); /* Set multiplex ratio */
    oled_write_cmd(0x3F);
    oled_write_cmd(0xD3); /* Set display offset */
    oled_write_cmd(0x00);
    oled_write_cmd(0x40); /* Set display start line */
    oled_write_cmd(0x8D); /* Charge pump */
    oled_write_cmd(0x14);
    oled_write_cmd(0x20); /* Memory addressing mode */
    oled_write_cmd(0x00); /* Horizontal addressing mode */
    oled_write_cmd(0xA1); /* Segment remap */
    oled_write_cmd(0xC8); /* COM scan direction */
    oled_write_cmd(0xDA); /* COM pins hardware configuration */
    oled_write_cmd(0x12);
    oled_write_cmd(0x81); /* Contrast */
    oled_write_cmd(0x7F);
    oled_write_cmd(0xD9); /* Pre-charge period */
    oled_write_cmd(0xF1);
    oled_write_cmd(0xDB); /* VCOMH deselect level */
    oled_write_cmd(0x40);
    oled_write_cmd(0xA4); /* Output follows RAM */
    oled_write_cmd(0xA6); /* Normal display */
    oled_write_cmd(0x2E); /* Deactivate scroll */
    oled_write_cmd(0xAF); /* Display ON */

    oled_clear();
    oled_refresh();
}

void oled_clear(void)
{
    oled_fill(OLED_COLOR_BLACK);
}

void oled_fill(uint8_t color)
{
    memset(oled_buffer, color ? 0xFF : 0x00, sizeof(oled_buffer));
}

void oled_refresh(void)
{
    uint8_t page;

    for (page = 0; page < (OLED_HEIGHT / 8); page++)
    {
        uint8_t col_low = (uint8_t)(OLED_COLUMN_OFFSET & 0x0F);
        uint8_t col_high = (uint8_t)((OLED_COLUMN_OFFSET >> 4) & 0x0F);

        oled_write_cmd((uint8_t)(0xB0 + page));
        oled_write_cmd((uint8_t)(0x00 + col_low));
        oled_write_cmd((uint8_t)(0x10 + col_high));

        oled_write_data(&oled_buffer[OLED_WIDTH * page], OLED_WIDTH);
    }
}

void oled_draw_pixel(uint8_t x, uint8_t y, uint8_t color)
{
    uint16_t index;
    uint8_t mask;

    if (x >= OLED_WIDTH || y >= OLED_HEIGHT)
    {
        return;
    }

    index = (uint16_t)(x + (y / 8) * OLED_WIDTH);
    mask = (uint8_t)(1U << (y % 8));

    if (color)
    {
        oled_buffer[index] |= mask;
    }
    else
    {
        oled_buffer[index] &= (uint8_t)(~mask);
    }
}

void oled_draw_hline(uint8_t x, uint8_t y, uint8_t len, uint8_t color)
{
    for (uint8_t i = 0; i < len; i++)
    {
        oled_draw_pixel((uint8_t)(x + i), y, color);
    }
}

void oled_draw_vline(uint8_t x, uint8_t y, uint8_t len, uint8_t color)
{
    for (uint8_t i = 0; i < len; i++)
    {
        oled_draw_pixel(x, (uint8_t)(y + i), color);
    }
}

void oled_draw_line(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, uint8_t color)
{
    int16_t dx = (int16_t)x1 - (int16_t)x0;
    int16_t dy = (int16_t)y1 - (int16_t)y0;
    int16_t sx = (dx >= 0) ? 1 : -1;
    int16_t sy = (dy >= 0) ? 1 : -1;
    int16_t err;

    dx = (dx >= 0) ? dx : -dx;
    dy = (dy >= 0) ? dy : -dy;
    err = (dx > dy ? dx : -dy) / 2;

    while (1)
    {
        oled_draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1)
        {
            break;
        }
        int16_t e2 = err;
        if (e2 > -dx)
        {
            err -= dy;
            x0 = (uint8_t)((int16_t)x0 + sx);
        }
        if (e2 < dy)
        {
            err += dx;
            y0 = (uint8_t)((int16_t)y0 + sy);
        }
    }
}

void oled_draw_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)
{
    if (w == 0 || h == 0)
    {
        return;
    }
    oled_draw_hline(x, y, w, color);
    oled_draw_hline(x, (uint8_t)(y + h - 1), w, color);
    oled_draw_vline(x, y, h, color);
    oled_draw_vline((uint8_t)(x + w - 1), y, h, color);
}

void oled_fill_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)
{
    for (uint8_t iy = 0; iy < h; iy++)
    {
        oled_draw_hline(x, (uint8_t)(y + iy), w, color);
    }
}

void oled_draw_circle(uint8_t x0, uint8_t y0, uint8_t r, uint8_t color)
{
    int16_t x = 0;
    int16_t y = r;
    int16_t d = 3 - (2 * (int16_t)r);

    while (x <= y)
    {
        oled_draw_pixel((uint8_t)(x0 + x), (uint8_t)(y0 + y), color);
        oled_draw_pixel((uint8_t)(x0 - x), (uint8_t)(y0 + y), color);
        oled_draw_pixel((uint8_t)(x0 + x), (uint8_t)(y0 - y), color);
        oled_draw_pixel((uint8_t)(x0 - x), (uint8_t)(y0 - y), color);
        oled_draw_pixel((uint8_t)(x0 + y), (uint8_t)(y0 + x), color);
        oled_draw_pixel((uint8_t)(x0 - y), (uint8_t)(y0 + x), color);
        oled_draw_pixel((uint8_t)(x0 + y), (uint8_t)(y0 - x), color);
        oled_draw_pixel((uint8_t)(x0 - y), (uint8_t)(y0 - x), color);

        if (d < 0)
        {
            d += (4 * x) + 6;
        }
        else
        {
            d += (4 * (x - y)) + 10;
            y--;
        }
        x++;
    }
}

void oled_draw_bitmap(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *bitmap)
{
    uint8_t bytes_per_row = (uint8_t)((w + 7) / 8);

    for (uint8_t j = 0; j < h; j++)
    {
        for (uint8_t i = 0; i < w; i++)
        {
            uint16_t byte_index = (uint16_t)j * bytes_per_row + (i >> 3);
            uint8_t byte = bitmap[byte_index];
            uint8_t bit = (uint8_t)(0x80 >> (i & 0x07));
            oled_draw_pixel((uint8_t)(x + i), (uint8_t)(y + j), (byte & bit) ? OLED_COLOR_WHITE : OLED_COLOR_BLACK);
        }
    }
}

void oled_show_char(uint8_t x, uint8_t page, char ch)
{
    (void)x;
    (void)page;
    (void)ch;
}

void oled_show_string(uint8_t x, uint8_t page, const char *str)
{
    while (*str != '\0')
    {
        if (x > (OLED_WIDTH - 8))
        {
            x = 0;
            page = (uint8_t)(page + 2);
        }

        if (page > 6)
        {
            break;
        }

        oled_show_char(x, page, *str);
        x = (uint8_t)(x + 8);
        str++;
    }
}

void oled_draw_char(uint8_t x, uint8_t y, char ch, uint8_t size, uint8_t mode)
{
    (void)x;
    (void)y;
    (void)ch;
    (void)size;
    (void)mode;
}

void oled_draw_string(uint8_t x, uint8_t y, const char *str, uint8_t size, uint8_t mode)
{
    uint8_t x0 = x;

    while (*str != '\0')
    {
        if (x > (uint8_t)(OLED_WIDTH - size / 2))
        {
            x = 0;
            y = (uint8_t)(y + size);
        }

        if (y > (uint8_t)(OLED_HEIGHT - size))
        {
            break;
        }

        if (*str == '\r')
        {
            y = (uint8_t)(y + size);
            x = x0;
            str++;
            continue;
        }

        oled_draw_char(x, y, *str, size, mode);
        x = (uint8_t)(x + size / 2);
        str++;
    }
}

uint8_t oled_chinese_init(void)
{
    oled_font_ready = 0;
    return 1;
}

static void oled_get_hz_mat(const uint8_t *code, uint8_t *mat, uint8_t size)
{
    uint8_t i;
    uint8_t csize = (uint8_t)((size / 8 + ((size % 8) ? 1 : 0)) * size);
    (void)code;
    
    for (i = 0; i < csize; i++)
    {
        mat[i] = 0x00;
    }
}

void oled_show_chinese(uint8_t x, uint8_t y, const uint8_t *gbk, uint8_t size, uint8_t mode)
{
    uint8_t temp, t, t1;
    uint8_t y0 = y;
    uint8_t csize = (uint8_t)((size / 8 + ((size % 8) ? 1 : 0)) * size);
    uint8_t mat[128];

    if (csize > sizeof(mat))
    {
        return;
    }

    if (!oled_font_ready)
    {
        return;
    }

    oled_get_hz_mat(gbk, mat, size);

    for (t = 0; t < csize; t++)
    {
        temp = mat[t];
        for (t1 = 0; t1 < 8; t1++)
        {
            if (temp & 0x80)
            {
                oled_draw_pixel(x, y, OLED_COLOR_WHITE);
            }
            else if (mode == OLED_DRAW_NORMAL)
            {
                oled_draw_pixel(x, y, OLED_COLOR_BLACK);
            }

            temp <<= 1;
            y++;
            if ((uint8_t)(y - y0) == size)
            {
                y = y0;
                x++;
                break;
            }
        }
    }
}

void oled_show_string_gbk(uint8_t x, uint8_t y, uint8_t width, uint8_t height, const char *str, uint8_t size, uint8_t mode)
{
    uint8_t x0 = x;
    uint8_t y0 = y;
    uint8_t bHz = 0;
    const uint8_t *pstr = (const uint8_t *)str;

    while (*pstr != 0)
    {
        if (!bHz)
        {
            if (*pstr > 0x80)
            {
                bHz = 1;
            }
            else
            {
                if (x > (uint8_t)(x0 + width - size / 2))
                {
                    y = (uint8_t)(y + size);
                    x = x0;
                }

                if (y > (uint8_t)(y0 + height - size))
                {
                    break;
                }

                if (*pstr == 13)
                {
                    y = (uint8_t)(y + size);
                    x = x0;
                    pstr++;
                }
                else
                {
                    oled_draw_char(x, y, (char)(*pstr), size, mode);
                    pstr++;
                    x = (uint8_t)(x + size / 2);
                }
            }
        }
        else
        {
            if (x > (uint8_t)(x0 + width - size))
            {
                y = (uint8_t)(y + size);
                x = x0;
            }

            if (y > (uint8_t)(y0 + height - size))
            {
                break;
            }

            oled_show_chinese(x, y, pstr, size, mode);
            pstr += 2;
            x = (uint8_t)(x + size);
            bHz = 0;
        }
    }
}
