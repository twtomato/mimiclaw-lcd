/*
 * ILI9341 240×320 TFT driver — SPI (HSPI / SPI2), hardware DMA.
 *
 * Pin mapping is read from mimi_config.h → Kconfig (menuconfig):
 *   MIMI_ILI_CS_PIN   — chip select
 *   MIMI_ILI_DC_PIN   — data / command
 *   MIMI_ILI_RST_PIN  — hardware reset
 *   MIMI_ILI_MOSI_PIN — MOSI (SDA)
 *   MIMI_ILI_SCLK_PIN — clock
 *   MIMI_ILI_BL_PIN   — backlight (-1 = always on / external)
 *   MIMI_ILI_SPI_HZ   — SPI clock frequency (default 40 MHz)
 */

#include "ili9341.h"
#include "mimi_config.h"

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "ili9341";

static spi_device_handle_t s_spi;

/* ── Low-level SPI helpers ──────────────────────────────────────────────── */

/* Send one byte as a command (DC low). Restores DC high on exit. */
static void write_cmd(uint8_t cmd)
{
    gpio_set_level(MIMI_ILI_DC_PIN, 0);
    spi_transaction_t t = {
        .length  = 8,
        .tx_data = { cmd },
        .flags   = SPI_TRANS_USE_TXDATA,
    };
    spi_device_polling_transmit(s_spi, &t);
    gpio_set_level(MIMI_ILI_DC_PIN, 1);
}

/* Send one data byte (DC high). */
static void write_data8(uint8_t d)
{
    spi_transaction_t t = {
        .length  = 8,
        .tx_data = { d },
        .flags   = SPI_TRANS_USE_TXDATA,
    };
    spi_device_polling_transmit(s_spi, &t);
}

/* ── Public API ─────────────────────────────────────────────────────────── */

esp_err_t ili9341_init(void)
{
    /* ── GPIO: DC, RST, BL ── */
    uint64_t gpio_mask = (1ULL << MIMI_ILI_DC_PIN) | (1ULL << MIMI_ILI_RST_PIN);
#if MIMI_ILI_BL_PIN >= 0
    gpio_mask |= (1ULL << MIMI_ILI_BL_PIN);
#endif
    gpio_config_t gc = {
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
        .pin_bit_mask = gpio_mask,
    };
    gpio_config(&gc);

    gpio_set_level(MIMI_ILI_DC_PIN,  1);
    gpio_set_level(MIMI_ILI_RST_PIN, 1);
#if MIMI_ILI_BL_PIN >= 0
    gpio_set_level(MIMI_ILI_BL_PIN, 0);   /* off until init done */
#endif

    /* ── SPI bus (HSPI / SPI2) ── */
    spi_bus_config_t bus = {
        .mosi_io_num     = MIMI_ILI_MOSI_PIN,
        .miso_io_num     = -1,
        .sclk_io_num     = MIMI_ILI_SCLK_PIN,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        /* Buffer large enough for a full LVGL 20-row stripe */
        .max_transfer_sz = ILI9341_WIDTH * 20 * 2 + 8,
    };
    esp_err_t err = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) return err;

    spi_device_interface_config_t dev = {
        .clock_speed_hz = MIMI_ILI_SPI_HZ,
        .mode           = 0,
        .spics_io_num   = MIMI_ILI_CS_PIN,
        .queue_size     = 1,
        .flags          = 0,
    };
    err = spi_bus_add_device(SPI2_HOST, &dev, &s_spi);
    if (err != ESP_OK) return err;

    /* ── Hardware reset ── */
    gpio_set_level(MIMI_ILI_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(MIMI_ILI_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(150));

    /* ── Init sequence ── */
    write_cmd(0x01);                       /* SWRESET */
    vTaskDelay(pdMS_TO_TICKS(150));

    write_cmd(0x11);                       /* SLPOUT */
    vTaskDelay(pdMS_TO_TICKS(120));

    write_cmd(0x3A); write_data8(0x55);    /* COLMOD: 16-bit RGB565 */
    write_cmd(0x36); write_data8(0x48);    /* MADCTL: MX | BGR — portrait */

    write_cmd(0xC0); write_data8(0x23);    /* PWCTR1 */
    write_cmd(0xC1); write_data8(0x10);    /* PWCTR2 */
    write_cmd(0xC5);                       /* VMCTR1 */
        write_data8(0x3E); write_data8(0x28);
    write_cmd(0xC7); write_data8(0x86);    /* VMCTR2 */
    write_cmd(0xB1);                       /* FRMCTR1 — 70 Hz */
        write_data8(0x00); write_data8(0x18);
    write_cmd(0xB6);                       /* DFUNCTR */
        write_data8(0x08); write_data8(0x82); write_data8(0x27);

    write_cmd(0x26); write_data8(0x01);    /* GAMMASET */
    write_cmd(0xE0);                       /* GMCTRP1 */
        write_data8(0x0F); write_data8(0x31); write_data8(0x2B);
        write_data8(0x0C); write_data8(0x0E); write_data8(0x08);
        write_data8(0x4E); write_data8(0xF1); write_data8(0x37);
        write_data8(0x07); write_data8(0x10); write_data8(0x03);
        write_data8(0x0E); write_data8(0x09); write_data8(0x00);
    write_cmd(0xE1);                       /* GMCTRN1 */
        write_data8(0x00); write_data8(0x0E); write_data8(0x14);
        write_data8(0x03); write_data8(0x11); write_data8(0x07);
        write_data8(0x31); write_data8(0xC1); write_data8(0x48);
        write_data8(0x08); write_data8(0x0F); write_data8(0x0C);
        write_data8(0x31); write_data8(0x36); write_data8(0x0F);

    write_cmd(0x11);                       /* SLPOUT (second time — safety) */
    vTaskDelay(pdMS_TO_TICKS(120));
    write_cmd(0x29);                       /* DISPON */
    vTaskDelay(pdMS_TO_TICKS(20));

#if MIMI_ILI_BL_PIN >= 0
    gpio_set_level(MIMI_ILI_BL_PIN, 1);   /* backlight on */
#endif

    ESP_LOGI(TAG, "init done (%dx%d, SPI %d MHz)",
             ILI9341_WIDTH, ILI9341_HEIGHT, MIMI_ILI_SPI_HZ / 1000000);
    return ESP_OK;
}

void ili9341_set_window(int x0, int y0, int x1, int y1)
{
    write_cmd(0x2A);                       /* CASET */
    write_data8(x0 >> 8); write_data8(x0 & 0xFF);
    write_data8(x1 >> 8); write_data8(x1 & 0xFF);

    write_cmd(0x2B);                       /* PASET */
    write_data8(y0 >> 8); write_data8(y0 & 0xFF);
    write_data8(y1 >> 8); write_data8(y1 & 0xFF);

    write_cmd(0x2C);                       /* RAMWR */
}

void ili9341_write_pixels(const uint16_t *data, uint32_t count)
{
    /* DC is already HIGH (data mode) after set_window → RAMWR command.
     * Data is pre-swapped by the LVGL flush callback (MSB first). */
    const uint8_t *p   = (const uint8_t *)data;
    uint32_t       rem = count * 2;

    while (rem > 0) {
        uint32_t chunk = (rem > 4092) ? 4092 : rem;
        spi_transaction_t t = {
            .length    = chunk * 8,
            .tx_buffer = p,
        };
        spi_device_polling_transmit(s_spi, &t);
        p   += chunk;
        rem -= chunk;
    }
}

void ili9341_set_backlight(bool on)
{
#if MIMI_ILI_BL_PIN >= 0
    gpio_set_level(MIMI_ILI_BL_PIN, on ? 1 : 0);
#else
    (void)on;
#endif
}
