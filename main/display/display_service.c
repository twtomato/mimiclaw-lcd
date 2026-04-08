#include "display_service.h"
#include "mimi_config.h"

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "display";

/* ── Shared state ───────────────────────────────────────────────── */

typedef enum { STATE_NONE, STATE_THINKING } disp_state_t;

static SemaphoreHandle_t     s_mutex;
static TaskHandle_t          s_task;
static volatile disp_state_t s_state = STATE_NONE;
static char                  s_channel[16];
static char                  s_ip[16];   /* "255.255.255.255\0" fits in 16 */

/* ══════════════════════════════════════════════════════════════════
 *  LVGL TFT backend — shared by RM68140 (320×480) and ILI9341 (240×320)
 *
 *  Screen layout:
 *    y=0..59    Banner (logo image or text header)
 *    y=60..119  Status area — provider / model / IP + thinking
 *    y=120..131 Gradient divider (12px)
 *    y=132..end Chat area — scrollable WeChat-style bubbles
 * ══════════════════════════════════════════════════════════════════ */
#if defined(CONFIG_MIMI_DISPLAY_RM68140) || defined(CONFIG_MIMI_DISPLAY_ILI9341)

/* ── Display-specific includes and dimension defines ───────────── */
#if defined(CONFIG_MIMI_DISPLAY_RM68140)
#  include "rm68140.h"
#  include "display/img_banner.h"
#  define DISP_WIDTH   RM68140_WIDTH
#  define DISP_HEIGHT  RM68140_HEIGHT
#  define DISP_INIT()  rm68140_init()
#  define DISP_SET_WINDOW  rm68140_set_window
#  define DISP_WRITE_PIXELS rm68140_write_pixels
#else  /* ILI9341 */
#  include "ili9341.h"
#  define DISP_WIDTH   ILI9341_WIDTH
#  define DISP_HEIGHT  ILI9341_HEIGHT
#  define DISP_INIT()  ili9341_init()
#  define DISP_SET_WINDOW  ili9341_set_window
#  define DISP_WRITE_PIXELS ili9341_write_pixels
#endif

#include "lvgl.h"
#ifdef CONFIG_MIMI_DISPLAY_FONT_CJK
#  include "display/source-han-sans16.h"
#endif

/* ── Layout constants ───────────────────────────────────────────── */
#if defined(CONFIG_MIMI_DISPLAY_RM68140)
#  define BANNER_HEIGHT   60   /* image banner */
#  define STATUS_HEIGHT   120  /* banner(60) + status(60) */
#else
#  define BANNER_HEIGHT   24   /* text banner — smaller to maximise chat area */
#  define STATUS_HEIGHT   84   /* banner(24) + status(60) */
#endif
#define DIVIDER_HEIGHT  12                            /* gradient divider */
#define CHAT_Y          (STATUS_HEIGHT + DIVIDER_HEIGHT)  /* chat starts here */
#define CHAT_HEIGHT     (DISP_HEIGHT - CHAT_Y)
#define MAX_BUBBLES     12                            /* max chat entries */
#define BUBBLE_MAX_W    (DISP_WIDTH * 92 / 100)    /* 92% screen width */
#define BUBBLE_PAD      6                             /* inner padding    */
#define BUBBLE_MARGIN   4                             /* between bubbles  */

/* ── LVGL state ─────────────────────────────────────────────────── */
#define LVGL_BUF_LINES 20
static lv_disp_draw_buf_t  s_draw_buf;
static lv_color_t          s_lvgl_buf[DISP_WIDTH * LVGL_BUF_LINES];
static lv_disp_drv_t       s_disp_drv;
static SemaphoreHandle_t   s_lvgl_mutex;

static void lvgl_flush_cb(lv_disp_drv_t *drv,
                          const lv_area_t *area,
                          lv_color_t *color_p)
{
    uint32_t pixels = (area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1);
    DISP_SET_WINDOW(area->x1, area->y1, area->x2, area->y2);
#if defined(CONFIG_MIMI_DISPLAY_ILI9341)
    /* SPI is MSB-first; LVGL stores RGB565 little-endian → byte-swap in place */
    uint16_t *buf = (uint16_t *)color_p;
    for (uint32_t i = 0; i < pixels; i++) {
        buf[i] = (uint16_t)((buf[i] >> 8) | (buf[i] << 8));
    }
#endif
    DISP_WRITE_PIXELS((const uint16_t *)color_p, pixels);
    lv_disp_flush_ready(drv);
}

static void lvgl_task(void *arg)
{
    TickType_t last = xTaskGetTickCount();
    while (1) {
        TickType_t now = xTaskGetTickCount();
        xSemaphoreTake(s_lvgl_mutex, portMAX_DELAY);
        lv_tick_inc((now - last) * portTICK_PERIOD_MS);
        last = now;
        lv_timer_handler();
        xSemaphoreGive(s_lvgl_mutex);
        vTaskDelay(2);
    }
}

static bool lvgl_lock(uint32_t timeout_ms)
{
    TickType_t ticks = timeout_ms ? pdMS_TO_TICKS(timeout_ms) : portMAX_DELAY;
    return xSemaphoreTake(s_lvgl_mutex, ticks) == pdTRUE;
}

static void lvgl_unlock(void) { xSemaphoreGive(s_lvgl_mutex); }

/* ── Status area labels ─────────────────────────────────────────── */
static lv_obj_t *s_lbl_title;   /* "MimiClaw"          */
static lv_obj_t *s_lbl_a;       /* provider / status   */
static lv_obj_t *s_lbl_b;       /* model               */
static lv_obj_t *s_lbl_c;       /* thinking anim / msg */
static lv_obj_t *s_lbl_ip;      /* IP (bottom of status) */

/* ── Chat area ──────────────────────────────────────────────────── */
static lv_obj_t *s_chat_cont;   /* scrollable container */

/* ── Fonts ──────────────────────────────────────────────────────── */
static const lv_font_t *s_font_cn;   /* Source Han Sans 16 (CJK) */
static const lv_font_t *s_font_en;   /* Montserrat (ASCII fallback) */
#ifdef CONFIG_MIMI_DISPLAY_FONT_CJK
static lv_font_t        s_cjk_font;  /* RAM copy of font descriptor (allows fallback) */
#endif

/* ── UI construction ────────────────────────────────────────────── */

static void create_ui(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_make(0x1A, 0x1A, 0x2E), 0);  /* dark navy */
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    /* fonts */
#ifdef CONFIG_LV_FONT_MONTSERRAT_14
    s_font_en = &lv_font_montserrat_14;
#else
    s_font_en = LV_FONT_DEFAULT;
#endif
#ifdef CONFIG_MIMI_DISPLAY_FONT_CJK
    /* Copy the const font descriptor to RAM so we can set .fallback.
     * Glyph bitmap data remains in flash — only the ~48-byte struct moves. */
    s_cjk_font = source_han_sans16;
    s_cjk_font.fallback = s_font_en;
    s_font_cn = &s_cjk_font;
#else
    s_font_cn = s_font_en;   /* no CJK font — fall back to Montserrat */
#endif

    /* ── Banner (y 0..59) ── */
#if defined(CONFIG_MIMI_DISPLAY_RM68140)
    lv_obj_t *banner = lv_img_create(scr);
    lv_img_set_src(banner, &img_banner);
    lv_obj_set_pos(banner, 0, 0);
#else  /* ILI9341 — text banner */
    lv_obj_t *banner_bg = lv_obj_create(scr);
    lv_obj_set_size(banner_bg, DISP_WIDTH, BANNER_HEIGHT);
    lv_obj_set_pos(banner_bg, 0, 0);
    lv_obj_set_style_bg_color(banner_bg, lv_color_make(0x0D, 0x1B, 0x2E), 0);
    lv_obj_set_style_border_width(banner_bg, 0, 0);
    lv_obj_set_style_radius(banner_bg, 0, 0);
    lv_obj_set_style_pad_all(banner_bg, 0, 0);
    lv_obj_clear_flag(banner_bg, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *banner_lbl = lv_label_create(banner_bg);
    lv_obj_set_style_text_color(banner_lbl, lv_color_make(0xFF, 0xFF, 0xFF), 0);
    lv_obj_set_style_text_font(banner_lbl, s_font_cn, 0);
    lv_label_set_text(banner_lbl, "MimiClaw");
    lv_obj_align(banner_lbl, LV_ALIGN_CENTER, 0, 0);
#endif

    /* ── Status area (y 60..119) ── */
    lv_obj_t *status_bg = lv_obj_create(scr);
    lv_obj_set_size(status_bg, DISP_WIDTH, STATUS_HEIGHT - BANNER_HEIGHT);
    lv_obj_set_pos(status_bg, 0, BANNER_HEIGHT);
    lv_obj_set_style_bg_color(status_bg, lv_color_make(0x1A, 0x1A, 0x2E), 0);
    lv_obj_set_style_border_width(status_bg, 0, 0);
    lv_obj_set_style_radius(status_bg, 0, 0);
    lv_obj_set_style_pad_all(status_bg, 0, 0);
    lv_obj_clear_flag(status_bg, LV_OBJ_FLAG_SCROLLABLE);

    /* s_lbl_title unused but kept for API compatibility */
    s_lbl_title = lv_label_create(status_bg);
    lv_obj_add_flag(s_lbl_title, LV_OBJ_FLAG_HIDDEN);

    lv_color_t c_normal = lv_color_make(0xE8, 0xE8, 0xE8);
    lv_color_t c_accent = lv_color_make(0x5B, 0x9B, 0xD5);
    lv_color_t c_dim    = lv_color_make(0x7F, 0x8C, 0x9A);

    s_lbl_a = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl_a, c_normal, 0);
    lv_obj_set_style_text_font(s_lbl_a, s_font_en, 0);
    lv_label_set_long_mode(s_lbl_a, LV_LABEL_LONG_DOT);
    lv_label_set_recolor(s_lbl_a, true);
    lv_obj_set_width(s_lbl_a, DISP_WIDTH - 10);
    lv_obj_set_pos(s_lbl_a, 5, BANNER_HEIGHT + 4);
    lv_label_set_text(s_lbl_a, "");

    s_lbl_b = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl_b, c_normal, 0);
    lv_obj_set_style_text_font(s_lbl_b, s_font_en, 0);
    lv_label_set_long_mode(s_lbl_b, LV_LABEL_LONG_DOT);
    lv_label_set_recolor(s_lbl_b, true);
    lv_obj_set_width(s_lbl_b, DISP_WIDTH - 10);
    lv_obj_set_pos(s_lbl_b, 5, BANNER_HEIGHT + 22);
    lv_label_set_text(s_lbl_b, "");

    /* IP: below Model, full width */
    s_lbl_ip = lv_label_create(scr);
    lv_obj_set_style_text_color(s_lbl_ip, c_dim, 0);
    lv_obj_set_style_text_font(s_lbl_ip, s_font_en, 0);
    lv_label_set_long_mode(s_lbl_ip, LV_LABEL_LONG_DOT);
    lv_label_set_recolor(s_lbl_ip, true);
    lv_obj_set_size(s_lbl_ip, DISP_WIDTH - 10, 18);
    lv_obj_set_pos(s_lbl_ip, 5, BANNER_HEIGHT + 40);
    lv_label_set_text(s_lbl_ip, "");

    /* s_lbl_c: hidden — no thinking indicator in status bar */
    s_lbl_c = lv_label_create(scr);
    lv_obj_add_flag(s_lbl_c, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_lbl_c, "");
    (void)c_accent;

    /* ── Gradient divider (y 120..131) ── */
    /* Top half: darker, bottom half: chat bg colour — simulates gradient */
    lv_obj_t *div1 = lv_obj_create(scr);
    lv_obj_set_size(div1, DISP_WIDTH, DIVIDER_HEIGHT / 2);
    lv_obj_set_pos(div1, 0, STATUS_HEIGHT);
    lv_obj_set_style_bg_color(div1, lv_color_make(0x10, 0x18, 0x28), 0);
    lv_obj_set_style_border_width(div1, 0, 0);
    lv_obj_set_style_radius(div1, 0, 0);
    lv_obj_set_style_pad_all(div1, 0, 0);

    lv_obj_t *div2 = lv_obj_create(scr);
    lv_obj_set_size(div2, DISP_WIDTH, DIVIDER_HEIGHT / 2);
    lv_obj_set_pos(div2, 0, STATUS_HEIGHT + DIVIDER_HEIGHT / 2);
    lv_obj_set_style_bg_color(div2, lv_color_make(0x08, 0x10, 0x18), 0);
    lv_obj_set_style_border_width(div2, 0, 0);
    lv_obj_set_style_radius(div2, 0, 0);
    lv_obj_set_style_pad_all(div2, 0, 0);

    /* ── Chat area (y 160..479) — Telegram dark background ── */
    s_chat_cont = lv_obj_create(scr);
    lv_obj_set_size(s_chat_cont, DISP_WIDTH, CHAT_HEIGHT);
    lv_obj_set_pos(s_chat_cont, 0, CHAT_Y);
    lv_obj_set_style_bg_color(s_chat_cont, lv_color_make(0x0F, 0x14, 0x21), 0);  /* deep dark */
    lv_obj_set_style_bg_opa(s_chat_cont, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_chat_cont, 0, 0);
    lv_obj_set_style_radius(s_chat_cont, 0, 0);
    lv_obj_set_style_pad_all(s_chat_cont, BUBBLE_MARGIN, 0);
    lv_obj_set_style_pad_row(s_chat_cont, BUBBLE_MARGIN, 0);
    lv_obj_set_flex_flow(s_chat_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(s_chat_cont,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_scrollbar_mode(s_chat_cont, LV_SCROLLBAR_MODE_OFF);
}

/* Clear status labels. Must be called with LVGL lock held. */
static void lvgl_clear_status(void)
{
    lv_label_set_text(s_lbl_a, "");
    lv_label_set_text(s_lbl_b, "");
    lv_label_set_text(s_lbl_c, "");
}

/* ── Display task — drives the "Thinking…" animation ────────────── */

static void display_task(void *arg)
{
    static const char *dots[] = {"Thinking.  ", "Thinking.. ", "Thinking..."};
    int tick = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(500));
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        if (s_state == STATE_THINKING) {
            if (lvgl_lock(50)) {
                lv_label_set_text(s_lbl_c, dots[tick % 3]);
                lvgl_unlock();
            }
            tick++;
        }
        xSemaphoreGive(s_mutex);
    }
}

/* ── Public API ─────────────────────────────────────────────────── */

esp_err_t display_service_init(void)
{
    esp_err_t err = DISP_INIT();
    if (err != ESP_OK) return err;

    /* Init LVGL and register display driver */
    lv_init();
    lv_disp_draw_buf_init(&s_draw_buf, s_lvgl_buf, NULL,
                          DISP_WIDTH * LVGL_BUF_LINES);
    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.draw_buf = &s_draw_buf;
    s_disp_drv.flush_cb = lvgl_flush_cb;
    s_disp_drv.hor_res  = DISP_WIDTH;
    s_disp_drv.ver_res  = DISP_HEIGHT;
    lv_disp_drv_register(&s_disp_drv);

    s_mutex      = xSemaphoreCreateMutex();
    s_lvgl_mutex = xSemaphoreCreateMutex();
    if (!s_mutex || !s_lvgl_mutex) return ESP_ERR_NO_MEM;

    /* Build persistent UI (must hold LVGL lock) */
    if (lvgl_lock(0)) {
        create_ui();
        lv_label_set_text(s_lbl_a, "Booting...");
        lvgl_unlock();
    }

    /* LVGL tick + render task on CPU1 (avoids WDT contention) */
    xTaskCreatePinnedToCore(lvgl_task, "lvgl",
                            8192, NULL, 5, NULL, 1);

    /* Thinking-animation task on CPU0 */
    xTaskCreatePinnedToCore(display_task, "display",
                            MIMI_DISPLAY_TASK_STACK, NULL,
                            MIMI_DISPLAY_TASK_PRIO, &s_task,
                            MIMI_DISPLAY_TASK_CORE);

#if defined(CONFIG_MIMI_DISPLAY_RM68140)
    rm68140_set_backlight(true);
    ESP_LOGI(TAG, "RM68140 display service started (%dx%d)", DISP_WIDTH, DISP_HEIGHT);
#else
    ili9341_set_backlight(true);
    ESP_LOGI(TAG, "ILI9341 display service started (%dx%d)", DISP_WIDTH, DISP_HEIGHT);
#endif
    return ESP_OK;
}

void display_service_show_wifi_connecting(const char *ssid)
{
    char line[80];
    snprintf(line, sizeof(line), "#808080 SSID:# %s", ssid ? ssid : "?");

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state = STATE_NONE;
    if (lvgl_lock(0)) {
        lvgl_clear_status();
        lv_label_set_text(s_lbl_a, "WiFi connecting...");
        lv_label_set_text(s_lbl_b, line);
        lvgl_unlock();
    }
    xSemaphoreGive(s_mutex);
}

void display_service_show_wifi_ok(const char *ip)
{
    if (ip) strncpy(s_ip, ip, sizeof(s_ip) - 1);
    char line[48];
    snprintf(line, sizeof(line), "#808080 IP:# %s", s_ip);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state = STATE_NONE;
    if (lvgl_lock(0)) {
        lvgl_clear_status();
        lv_label_set_text(s_lbl_a, "WiFi connected!");
        lv_label_set_text(s_lbl_b, line);
        lvgl_unlock();
    }
    xSemaphoreGive(s_mutex);
}

void display_service_show_wifi_fail(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state = STATE_NONE;
    if (lvgl_lock(0)) {
        lvgl_clear_status();
        lv_label_set_text(s_lbl_a, "WiFi failed!");
        lv_label_set_text(s_lbl_b, "Captive portal: 192.168.4.1");
        lvgl_unlock();
    }
    xSemaphoreGive(s_mutex);
}

void display_service_show_ready(const char *ip, const char *provider, const char *model)
{
    if (ip) strncpy(s_ip, ip, sizeof(s_ip) - 1);

    char prov[96], mod[96], ip_line[48];
    snprintf(prov,    sizeof(prov),    "#808080 Provider:# %s", provider ? provider : "");
    snprintf(mod,     sizeof(mod),     "#808080 Model:#    %s", model    ? model    : "");
    snprintf(ip_line, sizeof(ip_line), "#808080 IP:# %s", s_ip);

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state = STATE_NONE;
    if (lvgl_lock(0)) {
        lvgl_clear_status();
        lv_label_set_text(s_lbl_a,  prov);
        lv_label_set_text(s_lbl_b,  mod);
        lv_label_set_text(s_lbl_ip, ip_line);
        lvgl_unlock();
    }
    xSemaphoreGive(s_mutex);
}

void display_service_show_thinking(const char *channel)
{
    strncpy(s_channel, channel ? channel : "", sizeof(s_channel) - 1);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state = STATE_THINKING;
    xSemaphoreGive(s_mutex);
}

void display_service_show_message(const char *channel, const char *preview)
{
    /* On RM68140 the chat area handles messages; just stop the thinking anim */
    (void)channel;
    (void)preview;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state = STATE_NONE;
    /* nothing to update on display — chat bubble already shown */
    xSemaphoreGive(s_mutex);
}

/* ── Chat bubble ────────────────────────────────────────────────── */

/* Strip characters known to cause rendering problems (blacklist approach).
 * Removes: 4-byte emoji (U+10000+), variation selectors (U+FE00-U+FE0F),
 *          ZWJ (U+200D), and Misc Symbols/Dingbats (U+2600-U+27BF).
 * Everything else is passed through — LVGL font fallback handles the rest.
 * Writes into dst (must be at least strlen(src)+1 bytes). */
static void strip_unsupported(const char *src, char *dst, size_t dst_size)
{
    size_t wi = 0, ri = 0;
    while (src[ri]) {
        unsigned char c = (unsigned char)src[ri];
        uint32_t cp;
        int len;

        /* Decode UTF-8 codepoint */
        if      (c < 0x80) { cp = c;        len = 1; }
        else if (c < 0xE0) { cp = c & 0x1F; len = 2; }
        else if (c < 0xF0) { cp = c & 0x0F; len = 3; }
        else               { cp = c & 0x07; len = 4; }
        for (int j = 1; j < len; j++) {
            unsigned char b = (unsigned char)src[ri + j];
            if (!b) { len = j; break; }
            cp = (cp << 6) | (b & 0x3F);
        }

        /* Blacklist: drop characters known to cause boxes */
        bool drop =
            len == 4 ||                               /* emoji U+10000+        */
            (cp >= 0xFE00 && cp <= 0xFE0F) ||         /* variation selectors   */
            cp == 0x200D;                             /* zero width joiner     */

        if (!drop) {
            if (wi + len >= dst_size) break;   /* no room — stop cleanly */
            for (int j = 0; j < len; j++)
                dst[wi++] = src[ri + j];
        }
        ri += len;
    }
    dst[wi] = '\0';
}

/* Strip common Markdown symbols, keeping plain text.
 * Output is always <= input length, so src==dst (in-place) is safe.
 * Handles: **bold**, *italic*, __bold__, _italic_, `code`, ```block```,
 *          # headings, > blockquote, ~~strike~~, [text](url). */
static void strip_markdown(const char *src, char *dst, size_t dst_size)
{
    size_t ri = 0, wi = 0;
    while (src[ri] && wi < dst_size - 1) {
        char c = src[ri];
        bool bol = (ri == 0 || src[ri - 1] == '\n'); /* beginning of line */

        /* ── Fenced code block ```...``` ── */
        if (c == '`' && src[ri+1] == '`' && src[ri+2] == '`') {
            ri += 3;
            while (src[ri] && src[ri] != '\n') ri++;   /* skip language tag */
            if (src[ri] == '\n') ri++;
            while (src[ri]) {
                if (src[ri] == '`' && src[ri+1] == '`' && src[ri+2] == '`') { ri += 3; break; }
                if (wi < dst_size - 1) dst[wi++] = src[ri];
                ri++;
            }
            continue;
        }
        /* ── Inline code `...` ── */
        if (c == '`') {
            ri++;
            while (src[ri] && src[ri] != '`') { if (wi < dst_size-1) dst[wi++] = src[ri]; ri++; }
            if (src[ri] == '`') ri++;
            continue;
        }
        /* ── Bold ** / italic * ── */
        if (c == '*' && src[ri+1] == '*') { ri += 2; continue; }
        if (c == '*')                      { ri++;    continue; }
        /* ── Bold __ / italic _ ── */
        if (c == '_' && src[ri+1] == '_') { ri += 2; continue; }
        if (c == '_')                      { ri++;    continue; }
        /* ── Strikethrough ~~ ── */
        if (c == '~' && src[ri+1] == '~') { ri += 2; continue; }
        /* ── Heading # at line start ── */
        if (bol && c == '#') {
            while (src[ri] == '#') ri++;
            while (src[ri] == ' ') ri++;
            continue;
        }
        /* ── Blockquote > at line start ── */
        if (bol && c == '>') { ri++; if (src[ri] == ' ') ri++; continue; }
        /* ── Link [text](url) → text ── */
        if (c == '[') {
            ri++;
            while (src[ri] && src[ri] != ']') { if (wi < dst_size-1) dst[wi++] = src[ri]; ri++; }
            if (src[ri] == ']') ri++;
            if (src[ri] == '(') { ri++; while (src[ri] && src[ri] != ')') ri++; if (src[ri] == ')') ri++; }
            continue;
        }
        dst[wi++] = src[ri++];
    }
    dst[wi] = '\0';
}

void display_service_push_chat(const char *role, const char *content)
{
    if (!role || !content || content[0] == '\0') return;

    /* Filter emoji before rendering */
    size_t clen = strlen(content);
    char *filtered = malloc(clen + 1);
    if (!filtered) return;
    strip_unsupported(content, filtered, clen + 1);
    if (filtered[0] == '\0') { free(filtered); return; }
    strip_markdown(filtered, filtered, clen + 1);  /* in-place: output <= input */
    if (filtered[0] == '\0') { free(filtered); return; }

/* Collapse consecutive blank lines (\n\n+) into a single \n */
    char *p = filtered, *w = filtered;
    while (*p) {
        *w++ = *p;
        if (*p == '\n') {
            while (*p == '\n') p++;
        } else {
            p++;
        }
    }
    *w = '\0';

    content = filtered;

    bool is_user      = (strcmp(role, "user")      == 0);
    bool is_assistant = (strcmp(role, "assistant")  == 0);
    bool is_system    = (strcmp(role, "system")     == 0);

    if (!is_user && !is_assistant && !is_system) return;

    if (!lvgl_lock(200)) { free(filtered); return; }

    /* Remove oldest bubble if at limit */
    uint32_t cnt = lv_obj_get_child_cnt(s_chat_cont);
    if (cnt >= MAX_BUBBLES) {
        lv_obj_t *oldest = lv_obj_get_child(s_chat_cont, 0);
        if (oldest) lv_obj_del(oldest);
    }

    /* --- Bubble container (full-width row for alignment) --- */
    lv_obj_t *row = lv_obj_create(s_chat_cont);
    lv_obj_set_width(row, DISP_WIDTH - BUBBLE_MARGIN * 2);
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_radius(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    /* --- Bubble background --- */
    lv_obj_t *bubble = lv_obj_create(row);
    lv_obj_set_style_border_width(bubble, 0, 0);
    lv_obj_set_style_radius(bubble, 8, 0);
    lv_obj_set_style_pad_all(bubble, BUBBLE_PAD, 0);
    lv_obj_set_height(bubble, LV_SIZE_CONTENT);
    lv_obj_clear_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);

    /* --- Text label (Chinese font) --- */
    lv_obj_t *lbl = lv_label_create(bubble);
    lv_obj_set_style_text_font(lbl, s_font_cn, 0);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_label_set_text(lbl, content);

    /* Measure natural text width, cap at BUBBLE_MAX_W */
    lv_obj_set_width(lbl, LV_SIZE_CONTENT);
    lv_obj_update_layout(lbl);
    lv_coord_t tw = lv_obj_get_width(lbl);
    lv_coord_t bw = (tw < BUBBLE_MAX_W) ? tw : BUBBLE_MAX_W;
    lv_obj_set_width(lbl, bw);
    lv_obj_set_width(bubble, bw + BUBBLE_PAD * 2);

    /* Role-specific colours and alignment */
    if (is_user) {
        /* User: right-aligned, Telegram blue-purple bubble, white text */
        lv_obj_set_style_bg_color(bubble, lv_color_make(0x5B, 0x6B, 0xE8), 0);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(lbl, lv_color_make(0xFF, 0xFF, 0xFF), 0);
        lv_obj_align(bubble, LV_ALIGN_RIGHT_MID, -4, 0);

    } else if (is_assistant) {
        /* Assistant: left-aligned, dark card, light text (no bubble style) */
        lv_obj_set_style_bg_color(bubble, lv_color_make(0x1E, 0x2C, 0x3A), 0);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_COVER, 0);
        lv_obj_set_style_text_color(lbl, lv_color_make(0xE8, 0xE8, 0xE8), 0);
        lv_obj_align(bubble, LV_ALIGN_LEFT_MID, 4, 0);

    } else {
        /* System: centred, semi-transparent dark, dim text */
        lv_obj_set_style_bg_color(bubble, lv_color_make(0x2A, 0x3A, 0x4A), 0);
        lv_obj_set_style_bg_opa(bubble, LV_OPA_70, 0);
        lv_obj_set_style_text_color(lbl, lv_color_make(0xAA, 0xBB, 0xCC), 0);
        lv_obj_align(bubble, LV_ALIGN_CENTER, 0, 0);
    }

    /* Scroll to latest message */
    lv_obj_scroll_to_view_recursive(row, LV_ANIM_ON);

    lvgl_unlock();
    free(filtered);
}

/* ══════════════════════════════════════════════════════════════════
 *  SSD1306 I2C OLED backend
 * ══════════════════════════════════════════════════════════════════ */
#elif defined(CONFIG_MIMI_DISPLAY_SSD1306)

#include "ssd1306.h"

#define PAGE_TITLE   0
#define PAGE_SEP_Y   17
#define PAGE_LINE_A  3
#define PAGE_LINE_B  4
#define PAGE_LINE_C  5
#define PAGE_LINE_D  6
#define PAGE_LINE_E  7
#define CHARS_PER_LINE  21

static void wrap_str(const char *str, char *line1, char *line2, size_t len)
{
    size_t slen = strlen(str);
    if (slen <= CHARS_PER_LINE) {
        strncpy(line1, str, len - 1);
        line1[len - 1] = '\0';
        line2[0] = '\0';
        return;
    }
    int brk = CHARS_PER_LINE;
    for (int i = CHARS_PER_LINE - 1; i >= 0; i--) {
        if (str[i] == ' ') { brk = i; break; }
    }
    int l1 = brk < (int)(len - 1) ? brk : (int)(len - 1);
    strncpy(line1, str, l1);
    line1[l1] = '\0';
    const char *rest = str + brk + (str[brk] == ' ' ? 1 : 0);
    strncpy(line2, rest, len - 1);
    line2[len - 1] = '\0';
}

static void redraw_header(const char *title)
{
    ssd1306_put_str_large(0, PAGE_TITLE, title);
    ssd1306_hline(PAGE_SEP_Y);
}

static void render_static(const char *title,
                           const char *lineA, const char *lineB,
                           const char *lineC, const char *lineD)
{
    ssd1306_clear();
    redraw_header(title);
    if (lineA) ssd1306_put_str(0, PAGE_LINE_A, lineA);
    if (lineB) ssd1306_put_str(0, PAGE_LINE_B, lineB);
    if (lineC) ssd1306_put_str(0, PAGE_LINE_C, lineC);
    if (lineD) ssd1306_put_str(0, PAGE_LINE_D, lineD);
    ssd1306_flush();
}

esp_err_t display_service_init(void)
{
    esp_err_t err = ssd1306_init();
    if (err == ESP_ERR_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;

    s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return ESP_ERR_NO_MEM;

    render_static("MimiClaw", "Booting...", NULL, NULL, NULL);
    ESP_LOGI(TAG, "SSD1306 display service started");
    return ESP_OK;
}

void display_service_show_wifi_connecting(const char *ssid)
{
    if (!ssd1306_is_available()) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state = STATE_NONE;
    char line[CHARS_PER_LINE + 1];
    snprintf(line, sizeof(line), "SSID: %s", ssid ? ssid : "?");
    render_static("MimiClaw", "WiFi connecting", line, NULL, NULL);
    xSemaphoreGive(s_mutex);
}

void display_service_show_wifi_ok(const char *ip)
{
    if (!ssd1306_is_available()) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state = STATE_NONE;
    if (ip) strncpy(s_ip, ip, sizeof(s_ip) - 1);
    char line[CHARS_PER_LINE + 1];
    snprintf(line, sizeof(line), "IP: %s", s_ip);
    render_static("MimiClaw", "WiFi connected", line, NULL, NULL);
    xSemaphoreGive(s_mutex);
}

void display_service_show_wifi_fail(void)
{
    if (!ssd1306_is_available()) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state = STATE_NONE;
    render_static("MimiClaw", "WiFi failed", "Captive portal:", "192.168.4.1", NULL);
    xSemaphoreGive(s_mutex);
}

void display_service_show_ready(const char *ip, const char *provider, const char *model)
{
    if (!ssd1306_is_available()) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state = STATE_NONE;
    if (ip) strncpy(s_ip, ip, sizeof(s_ip) - 1);

    char prov_full[CHARS_PER_LINE+1], model_full[CHARS_PER_LINE+1], ip_line[CHARS_PER_LINE+1];
    snprintf(prov_full,  sizeof(prov_full),  "Provider: %s", provider ? provider : "");
    snprintf(model_full, sizeof(model_full), "Model: %s",    model    ? model    : "");
    snprintf(ip_line,    sizeof(ip_line),    "IP: %s",       s_ip);

    char prov1[CHARS_PER_LINE+1], prov2[CHARS_PER_LINE+1];
    char model1[CHARS_PER_LINE+1], model2[CHARS_PER_LINE+1];
    wrap_str(prov_full,  prov1,  prov2,  sizeof(prov1));
    wrap_str(model_full, model1, model2, sizeof(model1));

    ssd1306_clear();
    redraw_header("MimiClaw");
    ssd1306_put_str(0, PAGE_LINE_A, prov1);
    if (prov2[0])  ssd1306_put_str(0, PAGE_LINE_B, prov2);
    ssd1306_put_str(0, PAGE_LINE_C, model1);
    if (model2[0]) ssd1306_put_str(0, PAGE_LINE_D, model2);
    ssd1306_put_str(0, PAGE_LINE_E, ip_line);
    ssd1306_flush();
    xSemaphoreGive(s_mutex);
}

void display_service_show_thinking(const char *channel)
{
    (void)channel; /* SSD1306: no thinking indicator */
}

void display_service_show_message(const char *channel, const char *preview)
{
    (void)channel; (void)preview; /* SSD1306: no message display */
}

void display_service_push_chat(const char *role, const char *content)
{
    (void)role; (void)content; /* SSD1306 has no chat bubble support */
}

/* ══════════════════════════════════════════════════════════════════
 *  No display (CONFIG_MIMI_DISPLAY_NONE or backend not set)
 * ══════════════════════════════════════════════════════════════════ */
#else

esp_err_t display_service_init(void)                                              { return ESP_OK; }
void display_service_show_wifi_connecting(const char *ssid)                       { (void)ssid; }
void display_service_show_wifi_ok(const char *ip)                                 { (void)ip; }
void display_service_show_wifi_fail(void)                                         {}
void display_service_show_ready(const char *ip, const char *p, const char *m)    { (void)ip; (void)p; (void)m; }
void display_service_show_thinking(const char *channel)                           { (void)channel; }
void display_service_show_message(const char *channel, const char *preview)       { (void)channel; (void)preview; }
void display_service_push_chat(const char *role, const char *content)             { (void)role; (void)content; }

#endif /* display backend */
