#include "ssd1306.h"
#include "mimi_config.h"

#include <string.h>
#include "esp_log.h"
#include "driver/i2c_master.h"

static const char *TAG = "ssd1306";

/* ── State ──────────────────────────────────────────────────────── */

static i2c_master_bus_handle_t s_bus = NULL;
static i2c_master_dev_handle_t s_dev = NULL;
static bool s_available = false;

/* 1 KB framebuffer: [page][col], page 0 = top */
static uint8_t s_fb[SSD1306_PAGES][SSD1306_WIDTH];

/* ── 5×7 font (ASCII 32–126) ───────────────────────────────────── */
/* Each character: 5 column bytes. Bit 0 = top pixel, bit 6 = bottom. */

static const uint8_t FONT5X7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* 32   */
    {0x00,0x00,0x5F,0x00,0x00}, /* 33 ! */
    {0x00,0x07,0x00,0x07,0x00}, /* 34 " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* 35 # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* 36 $ */
    {0x23,0x13,0x08,0x64,0x62}, /* 37 % */
    {0x36,0x49,0x55,0x22,0x50}, /* 38 & */
    {0x00,0x05,0x03,0x00,0x00}, /* 39 ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* 40 ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* 41 ) */
    {0x14,0x08,0x3E,0x08,0x14}, /* 42 * */
    {0x08,0x08,0x3E,0x08,0x08}, /* 43 + */
    {0x00,0x50,0x30,0x00,0x00}, /* 44 , */
    {0x08,0x08,0x08,0x08,0x08}, /* 45 - */
    {0x00,0x60,0x60,0x00,0x00}, /* 46 . */
    {0x20,0x10,0x08,0x04,0x02}, /* 47 / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 48 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 49 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 50 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 51 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 52 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 53 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 54 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 55 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 56 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 57 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* 58 : */
    {0x00,0x56,0x36,0x00,0x00}, /* 59 ; */
    {0x08,0x14,0x22,0x41,0x00}, /* 60 < */
    {0x14,0x14,0x14,0x14,0x14}, /* 61 = */
    {0x00,0x41,0x22,0x14,0x08}, /* 62 > */
    {0x02,0x01,0x51,0x09,0x06}, /* 63 ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* 64 @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 65 A */
    {0x7F,0x49,0x49,0x49,0x36}, /* 66 B */
    {0x3E,0x41,0x41,0x41,0x22}, /* 67 C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 68 D */
    {0x7F,0x49,0x49,0x49,0x41}, /* 69 E */
    {0x7F,0x09,0x09,0x09,0x01}, /* 70 F */
    {0x3E,0x41,0x49,0x49,0x7A}, /* 71 G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 72 H */
    {0x00,0x41,0x7F,0x41,0x00}, /* 73 I */
    {0x20,0x40,0x41,0x3F,0x01}, /* 74 J */
    {0x7F,0x08,0x14,0x22,0x41}, /* 75 K */
    {0x7F,0x40,0x40,0x40,0x40}, /* 76 L */
    {0x7F,0x02,0x04,0x02,0x7F}, /* 77 M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 78 N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 79 O */
    {0x7F,0x09,0x09,0x09,0x06}, /* 80 P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 81 Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* 82 R */
    {0x46,0x49,0x49,0x49,0x31}, /* 83 S */
    {0x01,0x01,0x7F,0x01,0x01}, /* 84 T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 85 U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 86 V */
    {0x3F,0x40,0x38,0x40,0x3F}, /* 87 W */
    {0x63,0x14,0x08,0x14,0x63}, /* 88 X */
    {0x07,0x08,0x70,0x08,0x07}, /* 89 Y */
    {0x61,0x51,0x49,0x45,0x43}, /* 90 Z */
    {0x00,0x7F,0x41,0x41,0x00}, /* 91 [ */
    {0x02,0x04,0x08,0x10,0x20}, /* 92 \ */
    {0x00,0x41,0x41,0x7F,0x00}, /* 93 ] */
    {0x04,0x02,0x01,0x02,0x04}, /* 94 ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* 95 _ */
    {0x00,0x01,0x02,0x04,0x00}, /* 96 ` */
    {0x20,0x54,0x54,0x54,0x78}, /* 97 a */
    {0x7F,0x48,0x44,0x44,0x38}, /* 98 b */
    {0x38,0x44,0x44,0x44,0x20}, /* 99 c */
    {0x38,0x44,0x44,0x48,0x7F}, /* 100 d */
    {0x38,0x54,0x54,0x54,0x18}, /* 101 e */
    {0x08,0x7E,0x09,0x01,0x02}, /* 102 f */
    {0x0C,0x52,0x52,0x52,0x3E}, /* 103 g */
    {0x7F,0x08,0x04,0x04,0x78}, /* 104 h */
    {0x00,0x44,0x7D,0x40,0x00}, /* 105 i */
    {0x20,0x40,0x44,0x3D,0x00}, /* 106 j */
    {0x7F,0x10,0x28,0x44,0x00}, /* 107 k */
    {0x00,0x41,0x7F,0x40,0x00}, /* 108 l */
    {0x7C,0x04,0x18,0x04,0x78}, /* 109 m */
    {0x7C,0x08,0x04,0x04,0x78}, /* 110 n */
    {0x38,0x44,0x44,0x44,0x38}, /* 111 o */
    {0x7C,0x14,0x14,0x14,0x08}, /* 112 p */
    {0x08,0x14,0x14,0x18,0x7C}, /* 113 q */
    {0x7C,0x08,0x04,0x04,0x08}, /* 114 r */
    {0x48,0x54,0x54,0x54,0x20}, /* 115 s */
    {0x04,0x3F,0x44,0x40,0x20}, /* 116 t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* 117 u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 118 v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 119 w */
    {0x44,0x28,0x10,0x28,0x44}, /* 120 x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 121 y */
    {0x44,0x64,0x54,0x4C,0x44}, /* 122 z */
    {0x00,0x08,0x36,0x41,0x00}, /* 123 { */
    {0x00,0x00,0x7F,0x00,0x00}, /* 124 | */
    {0x00,0x41,0x36,0x08,0x00}, /* 125 } */
    {0x10,0x08,0x08,0x10,0x08}, /* 126 ~ */
};

/* ── I2C helpers ────────────────────────────────────────────────── */

static esp_err_t i2c_write(const uint8_t *buf, size_t len)
{
    return i2c_master_transmit(s_dev, buf, len, 100 /* ms */);
}

/* Send a stream of commands in one I2C transaction (control byte 0x00). */
static esp_err_t send_cmds(const uint8_t *cmds, size_t len)
{
    /* Max init sequence < 32 bytes; stack buffer is fine. */
    uint8_t buf[48];
    if (len + 1 > sizeof(buf)) return ESP_ERR_INVALID_SIZE;
    buf[0] = 0x00;  /* Co=0, D/C=0: all following bytes are commands */
    memcpy(buf + 1, cmds, len);
    return i2c_write(buf, len + 1);
}

/* ── SSD1306 init sequence ──────────────────────────────────────── */

static const uint8_t INIT_SEQ[] = {
    0xAE,        /* display off */
    0xD5, 0x80,  /* clock divide ratio / oscillator frequency */
    0xA8, 0x3F,  /* multiplex ratio: 64 rows */
    0xD3, 0x00,  /* display offset: 0 */
    0x40,        /* display start line: 0 */
    0x8D, 0x14,  /* charge pump: enable */
    0x20, 0x00,  /* memory addressing mode: horizontal */
    0xA1,        /* segment re-map: col 127 → SEG0 */
    0xC8,        /* COM scan direction: remapped */
    0xDA, 0x12,  /* COM pins hardware config */
    0x81, 0xCF,  /* contrast */
    0xD9, 0xF1,  /* pre-charge period */
    0xDB, 0x40,  /* VCOMH deselect level */
    0xA4,        /* output follows RAM */
    0xA6,        /* normal display (not inverted) */
    0xAF,        /* display on */
};

/* ── Public API ─────────────────────────────────────────────────── */

esp_err_t ssd1306_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port           = MIMI_DISPLAY_I2C_PORT,
        .sda_io_num         = MIMI_DISPLAY_SDA_PIN,
        .scl_io_num         = MIMI_DISPLAY_SCL_PIN,
        .clk_source         = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt  = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(err));
        return err;
    }

    /* Probe before adding device so we don't hang on a missing display. */
    err = i2c_master_probe(s_bus, MIMI_DISPLAY_I2C_ADDR, 200);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SSD1306 not found at 0x%02X — display disabled",
                 MIMI_DISPLAY_I2C_ADDR);
        i2c_del_master_bus(s_bus);
        s_bus = NULL;
        return ESP_ERR_NOT_FOUND;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = MIMI_DISPLAY_I2C_ADDR,
        .scl_speed_hz    = 400000,
    };
    err = i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C device add failed: %s", esp_err_to_name(err));
        i2c_del_master_bus(s_bus);
        s_bus = NULL;
        return err;
    }

    err = send_cmds(INIT_SEQ, sizeof(INIT_SEQ));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SSD1306 init sequence failed: %s", esp_err_to_name(err));
        return err;
    }

    memset(s_fb, 0, sizeof(s_fb));
    s_available = true;
    ESP_LOGI(TAG, "SSD1306 ready (%dx%d) on SDA=%d SCL=%d",
             SSD1306_WIDTH, SSD1306_HEIGHT,
             MIMI_DISPLAY_SDA_PIN, MIMI_DISPLAY_SCL_PIN);
    return ESP_OK;
}

bool ssd1306_is_available(void)
{
    return s_available;
}

void ssd1306_clear(void)
{
    memset(s_fb, 0, sizeof(s_fb));
}

void ssd1306_flush(void)
{
    if (!s_available) return;

    /* Set full-screen window in horizontal addressing mode. */
    const uint8_t window[] = {
        0x21, 0x00, 0x7F,  /* column address: 0–127 */
        0x22, 0x00, 0x07,  /* page address:   0–7   */
    };
    if (send_cmds(window, sizeof(window)) != ESP_OK) return;

    /* Send all 1024 bytes of GDDRAM in one transaction.
     * Prepend the 0x40 data control byte. */
    static uint8_t tx[1 + SSD1306_WIDTH * SSD1306_PAGES];
    tx[0] = 0x40;
    for (int p = 0; p < SSD1306_PAGES; p++) {
        memcpy(tx + 1 + p * SSD1306_WIDTH, s_fb[p], SSD1306_WIDTH);
    }
    i2c_write(tx, sizeof(tx));
}

void ssd1306_put_char(uint8_t col, uint8_t page, char c)
{
    if (!s_available) return;
    if (page >= SSD1306_PAGES) return;
    if (c < 32 || c > 126) c = ' ';

    const uint8_t *glyph = FONT5X7[(uint8_t)(c - 32)];
    for (int i = 0; i < 5 && col + i < SSD1306_WIDTH; i++) {
        s_fb[page][col + i] = glyph[i];
    }
    /* 1-pixel gap column */
    if (col + 5 < SSD1306_WIDTH) {
        s_fb[page][col + 5] = 0x00;
    }
}

void ssd1306_put_str(uint8_t col, uint8_t page, const char *str)
{
    if (!s_available || !str) return;
    while (*str && col + 6 <= SSD1306_WIDTH) {
        ssd1306_put_char(col, page, *str++);
        col += 6;
    }
}

void ssd1306_put_str_large(uint8_t col, uint8_t page, const char *str)
{
    if (!s_available || !str) return;
    if (page + 1 >= SSD1306_PAGES) return;

    while (*str && col + 12 <= SSD1306_WIDTH) {
        char c = *str++;
        if (c < 32 || c > 126) c = ' ';
        const uint8_t *glyph = FONT5X7[(uint8_t)(c - 32)];

        for (int i = 0; i < 5; i++) {
            uint8_t col_bits = glyph[i];
            /* Expand each bit vertically: 1 bit → 2 bits in upper page,
             * bits 4–6 overflow into the lower page. */
            uint8_t lo = 0, hi = 0;
            for (int bit = 0; bit < 4; bit++) {
                if (col_bits & (1 << bit)) {
                    lo |= (3 << (bit * 2));
                }
            }
            for (int bit = 4; bit < 8; bit++) {
                if (col_bits & (1 << bit)) {
                    hi |= (3 << ((bit - 4) * 2));
                }
            }
            uint8_t px0 = col + (uint8_t)(i * 2);
            uint8_t px1 = px0 + 1;
            if (px0 < SSD1306_WIDTH) {
                s_fb[page][px0]     = lo;
                s_fb[page + 1][px0] = hi;
            }
            if (px1 < SSD1306_WIDTH) {
                s_fb[page][px1]     = lo;
                s_fb[page + 1][px1] = hi;
            }
        }
        /* 2-pixel gap */
        uint8_t gap = col + 10;
        if (gap < SSD1306_WIDTH) {
            s_fb[page][gap]     = 0;
            s_fb[page + 1][gap] = 0;
        }
        gap++;
        if (gap < SSD1306_WIDTH) {
            s_fb[page][gap]     = 0;
            s_fb[page + 1][gap] = 0;
        }
        col += 12;
    }
}

void ssd1306_hline(uint8_t y)
{
    if (!s_available || y >= SSD1306_HEIGHT) return;
    uint8_t page = y / 8;
    uint8_t bit  = 1 << (y % 8);
    for (int c = 0; c < SSD1306_WIDTH; c++) {
        s_fb[page][c] |= bit;
    }
}
