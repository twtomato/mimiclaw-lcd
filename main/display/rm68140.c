/*
 * RM68140 320x480 TFT driver — 8-bit 8080 parallel, pure GPIO bit-bang.
 *
 * Why bit-bang instead of esp_lcd i80?
 *   The esp_lcd hardware i80 peripheral uses DMA.  On this panel the DMA
 *   path was unreliable (all-white output); software GPIO toggling — the
 *   same technique used by TFT_eSPI on Arduino — works reliably.
 *
 * GPIO pins are read from mimi_config.h → Kconfig (menuconfig).
 */

#include "rm68140.h"
#include "mimi_config.h"

#include "driver/gpio.h"
#include "soc/gpio_reg.h"   /* GPIO_OUT_W1TS_REG / W1TC_REG */
#include "soc/soc.h"        /* REG_WRITE */
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "rm68140";

static const int DATA_PINS[8] = {
    MIMI_RM_D0, MIMI_RM_D1, MIMI_RM_D2, MIMI_RM_D3,
    MIMI_RM_D4, MIMI_RM_D5, MIMI_RM_D6, MIMI_RM_D7,
};

/* ── Fast GPIO helpers ───────────────────────────────────────────
 * Precompute a 256-entry lookup table that maps each byte value to
 * the GPIO_OUT bitmask for D0-D7.  Then a single write8() costs
 * 4 × REG_WRITE (≈ 4 CPU cycles each) instead of 10 gpio_set_level
 * calls (~200 ns each) — roughly 10× faster pixel throughput.      */

/* GPIO0-31 use GPIO_OUT_W1TS/W1TC_REG; GPIO32+ use GPIO_OUT1_W1TS/W1TC_REG.
 * Split masks into lo (bit = pin) and hi (bit = pin-32) groups so any data
 * pin can be placed on any GPIO regardless of whether it is below or above 32. */
static uint32_t s_byte_mask_lo[256]; /* byte → W1TS mask for GPIO0-31  */
static uint32_t s_byte_mask_hi[256]; /* byte → W1TS mask for GPIO32-53 */
static uint32_t s_data_mask_lo;      /* all data pins in GPIO0-31       */
static uint32_t s_data_mask_hi;      /* all data pins in GPIO32-53      */

static void precompute_masks(void)
{
    s_data_mask_lo = 0;
    s_data_mask_hi = 0;
    for (int i = 0; i < 8; i++) {
        int p = DATA_PINS[i];
        if (p < 32) s_data_mask_lo |= (1u << p);
        else        s_data_mask_hi |= (1u << (p - 32));
    }

    for (int b = 0; b < 256; b++) {
        uint32_t lo = 0, hi = 0;
        for (int i = 0; i < 8; i++) {
            if (b & (1 << i)) {
                int p = DATA_PINS[i];
                if (p < 32) lo |= (1u << p);
                else        hi |= (1u << (p - 32));
            }
        }
        s_byte_mask_lo[b] = lo;
        s_byte_mask_hi[b] = hi;
    }
}

static inline void write8(uint8_t byte)
{
    REG_WRITE(GPIO_OUT_W1TC_REG,  s_data_mask_lo);          /* clear GPIO0-31  */
    REG_WRITE(GPIO_OUT1_W1TC_REG, s_data_mask_hi);          /* clear GPIO32+   */
    REG_WRITE(GPIO_OUT_W1TS_REG,  s_byte_mask_lo[byte]);    /* set   GPIO0-31  */
    REG_WRITE(GPIO_OUT1_W1TS_REG, s_byte_mask_hi[byte]);    /* set   GPIO32+   */
    REG_WRITE(GPIO_OUT_W1TC_REG,  1u << MIMI_RM_WR_PIN);   /* WR low          */
    REG_WRITE(GPIO_OUT_W1TS_REG,  1u << MIMI_RM_WR_PIN);   /* WR high         */
}

static void write_cmd(uint8_t cmd)
{
    REG_WRITE(GPIO_OUT_W1TC_REG, 1u << MIMI_RM_DC_PIN);  /* DC low  */
    write8(cmd);
    REG_WRITE(GPIO_OUT_W1TS_REG, 1u << MIMI_RM_DC_PIN);  /* DC high */
}

static void write_data8(uint8_t data) { write8(data); }

/* ── Public API ──────────────────────────────────────────────────*/

esp_err_t rm68140_init(void)
{
    /* Configure all control + data pins as outputs */
    uint64_t mask =
        (1ULL << MIMI_RM_CS_PIN)  | (1ULL << MIMI_RM_DC_PIN)  |
        (1ULL << MIMI_RM_RST_PIN) | (1ULL << MIMI_RM_WR_PIN)  |
        (1ULL << MIMI_RM_RD_PIN)  |
        (1ULL << MIMI_RM_D0) | (1ULL << MIMI_RM_D1) |
        (1ULL << MIMI_RM_D2) | (1ULL << MIMI_RM_D3) |
        (1ULL << MIMI_RM_D4) | (1ULL << MIMI_RM_D5) |
        (1ULL << MIMI_RM_D6) | (1ULL << MIMI_RM_D7);
#if MIMI_RM_BL_PIN >= 0
    mask |= (1ULL << MIMI_RM_BL_PIN);
#endif

    gpio_config_t g = {
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
        .pin_bit_mask = mask,
    };
    gpio_config(&g);
    precompute_masks();   /* build byte→GPIO lookup table */

    /* Safe idle state */
    gpio_set_level(MIMI_RM_CS_PIN,  1);
    gpio_set_level(MIMI_RM_WR_PIN,  1);
    gpio_set_level(MIMI_RM_RD_PIN,  1);   /* RD idle-high: write-only */
    gpio_set_level(MIMI_RM_DC_PIN,  1);
    gpio_set_level(MIMI_RM_RST_PIN, 1);
#if MIMI_RM_BL_PIN >= 0
    gpio_set_level(MIMI_RM_BL_PIN, 0);    /* backlight off until ready */
#endif

    /* Hardware reset */
    gpio_set_level(MIMI_RM_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(MIMI_RM_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(150));

    /* Init sequence (SWRESET → SLPOUT → COLMOD → MADCTL → DISPON) */
    gpio_set_level(MIMI_RM_CS_PIN, 0);   /* CS assert */

    write_cmd(0x01);                      /* SWRESET */
    vTaskDelay(pdMS_TO_TICKS(200));

    write_cmd(0x11);                      /* SLPOUT */
    vTaskDelay(pdMS_TO_TICKS(250));

    write_cmd(0x3A); write_data8(0x55);   /* COLMOD: 16-bit RGB565 */
    write_cmd(0x36); write_data8(0x48);   /* MADCTL: MX + BGR */

    write_cmd(0x29);                      /* DISPON */
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_LOGI(TAG, "init done (%dx%d, bit-bang GPIO)", RM68140_WIDTH, RM68140_HEIGHT);
    return ESP_OK;
}

void rm68140_set_window(int x0, int y0, int x1, int y1)
{
    write_cmd(0x2A);
    write_data8(x0 >> 8); write_data8(x0 & 0xFF);
    write_data8(x1 >> 8); write_data8(x1 & 0xFF);

    write_cmd(0x2B);
    write_data8(y0 >> 8); write_data8(y0 & 0xFF);
    write_data8(y1 >> 8); write_data8(y1 & 0xFF);

    write_cmd(0x2C);   /* RAMWR */
}

void rm68140_write_pixels(const uint16_t *data, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        write8(data[i] >> 8);
        write8(data[i] & 0xFF);
    }
}

void rm68140_set_backlight(bool on)
{
#if MIMI_RM_BL_PIN >= 0
    gpio_set_level(MIMI_RM_BL_PIN, on ? 1 : 0);
#else
    (void)on;
#endif
}
