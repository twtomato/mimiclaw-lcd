#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#define SSD1306_WIDTH   128
#define SSD1306_HEIGHT  64
#define SSD1306_PAGES   (SSD1306_HEIGHT / 8)  /* 8 pages of 8 rows each */

/* Initialise I2C bus and display. Returns ESP_ERR_NOT_FOUND if no display
 * is detected; all subsequent calls become no-ops in that case. */
esp_err_t ssd1306_init(void);

/* Clear the in-RAM framebuffer (does not update the display). */
void ssd1306_clear(void);

/* Push the framebuffer to the display hardware. */
void ssd1306_flush(void);

/* Draw a single ASCII character.
 * col  : pixel column (0–127), advances 6 px per character (5 + 1 gap)
 * page : display page / row (0–7, each page = 8 pixel rows) */
void ssd1306_put_char(uint8_t col, uint8_t page, char c);

/* Draw a NUL-terminated string. Clips at the right edge of the display. */
void ssd1306_put_str(uint8_t col, uint8_t page, const char *str);

/* Draw a string at 2× scale (12 px tall, 12 px per character).
 * page must be 0–6 to leave room for the second page row. */
void ssd1306_put_str_large(uint8_t col, uint8_t page, const char *str);

/* Draw a horizontal line across the full width at pixel row y (0–63). */
void ssd1306_hline(uint8_t y);

/* Returns true if the display was successfully initialised. */
bool ssd1306_is_available(void);
