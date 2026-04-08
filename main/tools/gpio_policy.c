#include "tools/gpio_policy.h"

#include "driver/gpio.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* ── Display pin reservation ────────────────────────────────────────
 * Block whichever GPIOs are wired to the active LCD backend so the
 * AI cannot accidentally corrupt the display by toggling them.
 *
 * Use CONFIG_* values directly (from sdkconfig.h via compiler -include).
 * #ifndef fallbacks match the Kconfig.projbuild defaults so the code
 * compiles even when sdkconfig hasn't been regenerated yet.
 * ──────────────────────────────────────────────────────────────────*/

#if defined(CONFIG_MIMI_DISPLAY_RM68140)
#  ifndef CONFIG_MIMI_RM_CS_PIN
#    define CONFIG_MIMI_RM_CS_PIN   6
#  endif
#  ifndef CONFIG_MIMI_RM_DC_PIN
#    define CONFIG_MIMI_RM_DC_PIN   7
#  endif
#  ifndef CONFIG_MIMI_RM_RST_PIN
#    define CONFIG_MIMI_RM_RST_PIN  5
#  endif
#  ifndef CONFIG_MIMI_RM_WR_PIN
#    define CONFIG_MIMI_RM_WR_PIN   1
#  endif
#  ifndef CONFIG_MIMI_RM_RD_PIN
#    define CONFIG_MIMI_RM_RD_PIN   2
#  endif
#  ifndef CONFIG_MIMI_RM_BL_PIN
#    define CONFIG_MIMI_RM_BL_PIN  -1
#  endif
#  ifndef CONFIG_MIMI_RM_D0
#    define CONFIG_MIMI_RM_D0      21
#  endif
#  ifndef CONFIG_MIMI_RM_D1
#    define CONFIG_MIMI_RM_D1      46
#  endif
#  ifndef CONFIG_MIMI_RM_D2
#    define CONFIG_MIMI_RM_D2      18
#  endif
#  ifndef CONFIG_MIMI_RM_D3
#    define CONFIG_MIMI_RM_D3      17
#  endif
#  ifndef CONFIG_MIMI_RM_D4
#    define CONFIG_MIMI_RM_D4      19
#  endif
#  ifndef CONFIG_MIMI_RM_D5
#    define CONFIG_MIMI_RM_D5      20
#  endif
#  ifndef CONFIG_MIMI_RM_D6
#    define CONFIG_MIMI_RM_D6       3
#  endif
#  ifndef CONFIG_MIMI_RM_D7
#    define CONFIG_MIMI_RM_D7      14
#  endif
#elif defined(CONFIG_MIMI_DISPLAY_ILI9341)
#  ifndef CONFIG_MIMI_ILI_CS_PIN
#    define CONFIG_MIMI_ILI_CS_PIN   12
#  endif
#  ifndef CONFIG_MIMI_ILI_DC_PIN
#    define CONFIG_MIMI_ILI_DC_PIN   11
#  endif
#  ifndef CONFIG_MIMI_ILI_RST_PIN
#    define CONFIG_MIMI_ILI_RST_PIN  13
#  endif
#  ifndef CONFIG_MIMI_ILI_MOSI_PIN
#    define CONFIG_MIMI_ILI_MOSI_PIN 10
#  endif
#  ifndef CONFIG_MIMI_ILI_SCLK_PIN
#    define CONFIG_MIMI_ILI_SCLK_PIN 46
#  endif
#  ifndef CONFIG_MIMI_ILI_BL_PIN
#    define CONFIG_MIMI_ILI_BL_PIN   -1
#  endif
#elif defined(CONFIG_MIMI_DISPLAY_SSD1306)
#  ifndef CONFIG_MIMI_DISPLAY_SDA_PIN
#    define CONFIG_MIMI_DISPLAY_SDA_PIN 41
#  endif
#  ifndef CONFIG_MIMI_DISPLAY_SCL_PIN
#    define CONFIG_MIMI_DISPLAY_SCL_PIN 42
#  endif
#endif

static bool display_pin_is_reserved(int pin)
{
#if defined(CONFIG_MIMI_DISPLAY_RM68140)
    if (pin == CONFIG_MIMI_RM_CS_PIN  || pin == CONFIG_MIMI_RM_DC_PIN  ||
        pin == CONFIG_MIMI_RM_RST_PIN || pin == CONFIG_MIMI_RM_WR_PIN  ||
        pin == CONFIG_MIMI_RM_RD_PIN  ||
        pin == CONFIG_MIMI_RM_D0      || pin == CONFIG_MIMI_RM_D1      ||
        pin == CONFIG_MIMI_RM_D2      || pin == CONFIG_MIMI_RM_D3      ||
        pin == CONFIG_MIMI_RM_D4      || pin == CONFIG_MIMI_RM_D5      ||
        pin == CONFIG_MIMI_RM_D6      || pin == CONFIG_MIMI_RM_D7)
        return true;
#  if CONFIG_MIMI_RM_BL_PIN >= 0
    if (pin == CONFIG_MIMI_RM_BL_PIN) return true;
#  endif
#elif defined(CONFIG_MIMI_DISPLAY_ILI9341)
    if (pin == CONFIG_MIMI_ILI_CS_PIN   || pin == CONFIG_MIMI_ILI_DC_PIN  ||
        pin == CONFIG_MIMI_ILI_RST_PIN  || pin == CONFIG_MIMI_ILI_MOSI_PIN ||
        pin == CONFIG_MIMI_ILI_SCLK_PIN)
        return true;
#  if CONFIG_MIMI_ILI_BL_PIN >= 0
    if (pin == CONFIG_MIMI_ILI_BL_PIN) return true;
#  endif
#elif defined(CONFIG_MIMI_DISPLAY_SSD1306)
    if (pin == CONFIG_MIMI_DISPLAY_SDA_PIN || pin == CONFIG_MIMI_DISPLAY_SCL_PIN)
        return true;
#else
    (void)pin;
#endif
    return false;
}

#ifndef GPIO_IS_VALID_GPIO
#define GPIO_IS_VALID_GPIO(pin) ((pin) >= 0)
#endif

static bool pin_in_allowlist(int pin, const char *csv)
{
    const char *cursor;

    if (!csv || csv[0] == '\0') {
        return false;
    }

    cursor = csv;
    while (*cursor != '\0') {
        char *endptr = NULL;
        long value;

        while (*cursor == ' ' || *cursor == '\t' || *cursor == ',') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }

        value = strtol(cursor, &endptr, 10);
        if (endptr == cursor) {
            while (*cursor != '\0' && *cursor != ',') {
                cursor++;
            }
            continue;
        }

        if ((int)value == pin) {
            return true;
        }
        cursor = endptr;
    }

    return false;
}

static bool pin_is_allowed_impl(int pin,
                                const char *allowlist_csv,
                                int min_pin,
                                int max_pin,
                                bool block_esp32_flash_pins,
                                bool block_esp32s3_usb_pins)
{
    bool in_policy;

    if (pin < 0) {
        return false;
    }

    /* Block ESP32 flash/PSRAM pins (GPIO 6-11) */
    if (block_esp32_flash_pins && pin >= 6 && pin <= 11) {
        return false;
    }

    /* USB Serial/JTAG uses GPIO19/20 on ESP32-S3 */
    if (block_esp32s3_usb_pins && (pin == 19 || pin == 20)) {
        return false;
    }

    if (allowlist_csv && allowlist_csv[0] != '\0') {
        in_policy = pin_in_allowlist(pin, allowlist_csv);
    } else {
        in_policy = pin >= min_pin && pin <= max_pin;
    }

    if (!in_policy) {
        return false;
    }

    return GPIO_IS_VALID_GPIO((gpio_num_t)pin);
}

bool gpio_policy_pin_is_allowed(int pin)
{
    if (display_pin_is_reserved(pin)) return false;

#if defined(CONFIG_IDF_TARGET_ESP32)
    return pin_is_allowed_impl(pin, MIMI_GPIO_ALLOWED_CSV,
                               MIMI_GPIO_MIN_PIN, MIMI_GPIO_MAX_PIN, true, false);
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    return pin_is_allowed_impl(pin, MIMI_GPIO_ALLOWED_CSV,
                               MIMI_GPIO_MIN_PIN, MIMI_GPIO_MAX_PIN, false, true);
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
    /* GPIO14 = RF antenna switch, must not be used as general GPIO */
    if (pin == 14) return false;
    return pin_is_allowed_impl(pin, MIMI_GPIO_ALLOWED_CSV,
                               MIMI_GPIO_MIN_PIN, MIMI_GPIO_MAX_PIN, false, false);
#else
    return pin_is_allowed_impl(pin, MIMI_GPIO_ALLOWED_CSV,
                               MIMI_GPIO_MIN_PIN, MIMI_GPIO_MAX_PIN, false, false);
#endif
}

bool gpio_policy_pin_forbidden_hint(int pin, char *result, size_t result_len)
{
    if (display_pin_is_reserved(pin)) {
        snprintf(result, result_len,
                 "Error: pin %d is reserved for the display (LCD); it cannot be used as a general GPIO",
                 pin);
        return true;
    }

#if defined(CONFIG_IDF_TARGET_ESP32)
    if (pin >= 6 && pin <= 11) {
        snprintf(result, result_len,
                 "Error: pin %d is reserved for ESP32 flash/PSRAM (GPIO6-11); choose a different pin",
                 pin);
        return true;
    }
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    if (pin == 19 || pin == 20) {
        snprintf(result, result_len,
                 "Error: pin %d is reserved for ESP32-S3 USB Serial/JTAG (GPIO19/20); choose a different pin",
                 pin);
        return true;
    }
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
    if (pin == 14) {
        snprintf(result, result_len,
                 "Error: pin 14 is reserved for RF antenna switch on XIAO ESP32-C6; choose a different pin");
        return true;
    }
#else
    (void)pin;
    (void)result;
    (void)result_len;
#endif

    return false;
}
