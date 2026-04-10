#include "direct_cmd.h"
#include "mimi_config.h"
#include "llm/llm_proxy.h"
#include "tools/tool_web_search.h"
#include "proxy/http_proxy.h"
#include "display/display_service.h"
#include "wifi/wifi_manager.h"

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "direct_cmd";

/* ── Helpers ──────────────────────────────────────────────────── */

static size_t buf_append(char *buf, size_t size, size_t off, const char *fmt, ...)
    __attribute__((format(printf, 4, 5)));

static size_t buf_append(char *buf, size_t size, size_t off, const char *fmt, ...)
{
    if (off >= size - 1) return off;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + off, size - off, fmt, ap);
    va_end(ap);
    if (n > 0) off += (size_t)n < (size - off) ? (size_t)n : (size - off - 1);
    return off;
}

/* Read one NVS string key; returns source "NVS" or "build" or "not set" */
static const char *nvs_read_str(const char *ns, const char *key,
                                char *out, size_t out_size)
{
    nvs_handle_t nvs;
    if (nvs_open(ns, NVS_READONLY, &nvs) == ESP_OK) {
        size_t len = out_size;
        if (nvs_get_str(nvs, key, out, &len) == ESP_OK && out[0]) {
            nvs_close(nvs);
            return "NVS";
        }
        nvs_close(nvs);
    }
    return NULL;
}

/* Append one config line with optional masking of sensitive values */
static size_t append_cfg(char *buf, size_t size, size_t off,
                         const char *label, const char *ns, const char *key,
                         const char *build_val, bool mask)
{
    char nvs_val[128] = {0};
    const char *source  = "not set";
    const char *display = "(empty)";

    const char *src = nvs_read_str(ns, key, nvs_val, sizeof(nvs_val));
    if (src) {
        source  = src;
        display = nvs_val;
    } else if (build_val && build_val[0]) {
        source  = "build";
        display = build_val;
    }

    if (mask && strlen(display) > 6 && strcmp(display, "(empty)") != 0) {
        char masked[12];
        snprintf(masked, sizeof(masked), "%.4s****", display);
        return buf_append(buf, size, off, "%-14s: %s [%s]\n", label, masked, source);
    }
    return buf_append(buf, size, off, "%-14s: %s [%s]\n", label, display, source);
}

/* ── Command handlers ─────────────────────────────────────────── */

static void cmd_help(char *out, size_t sz)
{
    snprintf(out, sz,
        "Direct commands (no LLM):\n"
        "  /help              - this message\n"
        "  /config            - show all config\n"
        "  /model <name>      - set LLM model\n"
        "  /provider <name>   - set LLM provider\n"
        "  /apikey <key>      - set LLM API key\n"
        "  /tavily <key>      - set Tavily search key\n"
        "  /heap              - show memory stats\n"
        "  /restart           - restart device"
    );
}

static void cmd_config(char *out, size_t sz)
{
    size_t off = 0;
    off = buf_append(out, sz, off, "=== Config ===\n");
    off = append_cfg(out, sz, off, "Model",      MIMI_NVS_LLM,    MIMI_NVS_KEY_MODEL,    MIMI_SECRET_MODEL,           false);
    off = append_cfg(out, sz, off, "Provider",   MIMI_NVS_LLM,    MIMI_NVS_KEY_PROVIDER, MIMI_SECRET_MODEL_PROVIDER,  false);
    off = append_cfg(out, sz, off, "API Key",    MIMI_NVS_LLM,    MIMI_NVS_KEY_API_KEY,  MIMI_SECRET_API_KEY,         true);
    off = append_cfg(out, sz, off, "Tavily Key", MIMI_NVS_SEARCH, MIMI_NVS_KEY_TAVILY_KEY, MIMI_SECRET_TAVILY_KEY,    true);
    off = append_cfg(out, sz, off, "Search Key", MIMI_NVS_SEARCH, MIMI_NVS_KEY_API_KEY,  MIMI_SECRET_SEARCH_KEY,      true);
    off = append_cfg(out, sz, off, "WiFi SSID",  MIMI_NVS_WIFI,   MIMI_NVS_KEY_SSID,     MIMI_SECRET_WIFI_SSID,       false);
    off = append_cfg(out, sz, off, "WiFi Pass",  MIMI_NVS_WIFI,   MIMI_NVS_KEY_PASS,     MIMI_SECRET_WIFI_PASS,       true);
    off = append_cfg(out, sz, off, "TG Token",   MIMI_NVS_TG,     MIMI_NVS_KEY_TG_TOKEN, MIMI_SECRET_TG_TOKEN,        true);
    off = append_cfg(out, sz, off, "Proxy Host", MIMI_NVS_PROXY,  MIMI_NVS_KEY_PROXY_HOST, MIMI_SECRET_PROXY_HOST,    false);

    /* Heap */
    off = buf_append(out, sz, off, "==============\n");
    off = buf_append(out, sz, off, "Heap internal: %d B\n",
                     (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
#ifdef CONFIG_SPIRAM
    off = buf_append(out, sz, off, "Heap PSRAM:    %d B\n",
                     (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
#endif
    buf_append(out, sz, off, "Heap total:    %d B",
               (int)esp_get_free_heap_size());
}

static void cmd_heap(char *out, size_t sz)
{
    size_t off = 0;
    off = buf_append(out, sz, off, "Heap internal: %d B\n",
                     (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
#ifdef CONFIG_SPIRAM
    off = buf_append(out, sz, off, "Heap PSRAM:    %d B\n",
                     (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
#endif
    buf_append(out, sz, off, "Heap total:    %d B",
               (int)esp_get_free_heap_size());
}

static void cmd_set_model(const char *arg, char *out, size_t sz)
{
    if (!arg || !arg[0]) { snprintf(out, sz, "Usage: /model <name>"); return; }
    if (llm_set_model(arg) == ESP_OK) {
        llm_proxy_init();
        display_service_show_ready(wifi_manager_get_ip(), llm_get_provider(), llm_get_model());
        snprintf(out, sz, "OK: model = %s", arg);
        ESP_LOGI(TAG, "Model set to: %s", arg);
    } else {
        snprintf(out, sz, "Error: failed to save model");
    }
}

static void cmd_set_provider(const char *arg, char *out, size_t sz)
{
    if (!arg || !arg[0]) { snprintf(out, sz, "Usage: /provider <name>"); return; }
    if (llm_set_provider(arg) == ESP_OK) {
        llm_proxy_init();
        display_service_show_ready(wifi_manager_get_ip(), llm_get_provider(), llm_get_model());
        snprintf(out, sz, "OK: provider = %s", arg);
        ESP_LOGI(TAG, "Provider set to: %s", arg);
    } else {
        snprintf(out, sz, "Error: failed to save provider");
    }
}

static void cmd_set_apikey(const char *arg, char *out, size_t sz)
{
    if (!arg || !arg[0]) { snprintf(out, sz, "Usage: /apikey <key>"); return; }
    if (llm_set_api_key(arg) == ESP_OK) {
        llm_proxy_init();
        snprintf(out, sz, "OK: API key updated (%.4s****)", arg);
        ESP_LOGI(TAG, "API key updated");
    } else {
        snprintf(out, sz, "Error: failed to save API key");
    }
}

static void cmd_set_tavily(const char *arg, char *out, size_t sz)
{
    if (!arg || !arg[0]) { snprintf(out, sz, "Usage: /tavily <key>"); return; }
    if (tool_web_search_set_tavily_key(arg) == ESP_OK) {
        snprintf(out, sz, "OK: Tavily key updated (%.4s****)", arg);
        ESP_LOGI(TAG, "Tavily key updated");
    } else {
        snprintf(out, sz, "Error: failed to save Tavily key");
    }
}

/* ── Public entry point ───────────────────────────────────────── */

void direct_cmd_execute(const char *msg, char *output, size_t output_size)
{
    /* Skip the leading '!' */
    const char *p = msg + 1;

    /* Extract command name (up to first space or end) */
    char cmd[32] = {0};
    const char *space = strchr(p, ' ');
    size_t cmd_len = space ? (size_t)(space - p) : strlen(p);
    if (cmd_len >= sizeof(cmd)) cmd_len = sizeof(cmd) - 1;
    memcpy(cmd, p, cmd_len);

    /* Argument: everything after the first space */
    const char *arg = (space && space[1]) ? space + 1 : NULL;

    /* Trim trailing whitespace from arg */
    char arg_buf[256] = {0};
    if (arg) {
        strncpy(arg_buf, arg, sizeof(arg_buf) - 1);
        int end = (int)strlen(arg_buf) - 1;
        while (end >= 0 && (arg_buf[end] == ' ' || arg_buf[end] == '\n' || arg_buf[end] == '\r')) {
            arg_buf[end--] = '\0';
        }
        arg = arg_buf;
    }

    ESP_LOGI(TAG, "Direct command: '%s' arg='%s'", cmd, arg ? arg : "");

    if (strcmp(cmd, "help") == 0)         { cmd_help(output, output_size); }
    else if (strcmp(cmd, "config") == 0)  { cmd_config(output, output_size); }
    else if (strcmp(cmd, "heap") == 0)    { cmd_heap(output, output_size); }
    else if (strcmp(cmd, "model") == 0)   { cmd_set_model(arg, output, output_size); }
    else if (strcmp(cmd, "provider") == 0){ cmd_set_provider(arg, output, output_size); }
    else if (strcmp(cmd, "apikey") == 0)  { cmd_set_apikey(arg, output, output_size); }
    else if (strcmp(cmd, "tavily") == 0)  { cmd_set_tavily(arg, output, output_size); }
    else if (strcmp(cmd, "restart") == 0) {
        snprintf(output, output_size, "Restarting...");
        /* Push the response first, then restart after a short delay */
        /* Caller is responsible for sending output before restart */
    } else {
        snprintf(output, output_size,
                 "Unknown command: /%s\nSend /help for available commands.", cmd);
    }
}
