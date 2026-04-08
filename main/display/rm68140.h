#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define RM68140_WIDTH   320
#define RM68140_HEIGHT  480

/* Initialise all GPIO pins, hardware-reset the panel, and send the
 * init sequence.  Must be called once before any other rm68140_* fn. */
esp_err_t rm68140_init(void);

/* Set the pixel write window (inclusive) and issue RAMWR.
 * Subsequent rm68140_write_pixels() calls fill this rectangle.    */
void rm68140_set_window(int x0, int y0, int x1, int y1);

/* Write `count` RGB565 pixels (big-endian) into the current window. */
void rm68140_write_pixels(const uint16_t *data, uint32_t count);

/* Control backlight GPIO.  No-op if MIMI_RM_BL_PIN == -1.         */
void rm68140_set_backlight(bool on);
