#pragma once

/* MimiClaw Global Configuration */

/* Build-time secrets (highest priority, override NVS) */
#if __has_include("mimi_secrets.h")
#include "mimi_secrets.h"
#endif

#ifndef MIMI_SECRET_WIFI_SSID
#define MIMI_SECRET_WIFI_SSID       ""
#endif
#ifndef MIMI_SECRET_WIFI_PASS
#define MIMI_SECRET_WIFI_PASS       ""
#endif
#ifndef MIMI_SECRET_TG_TOKEN
#define MIMI_SECRET_TG_TOKEN        ""
#endif
#ifndef MIMI_SECRET_API_KEY
#define MIMI_SECRET_API_KEY         ""
#endif
#ifndef MIMI_SECRET_MODEL
#define MIMI_SECRET_MODEL           ""
#endif
#ifndef MIMI_SECRET_MODEL_PROVIDER
#define MIMI_SECRET_MODEL_PROVIDER  "anthropic"
#endif
#ifndef MIMI_SECRET_PROXY_HOST
#define MIMI_SECRET_PROXY_HOST      ""
#endif
#ifndef MIMI_SECRET_PROXY_PORT
#define MIMI_SECRET_PROXY_PORT      ""
#endif
#ifndef MIMI_SECRET_PROXY_TYPE
#define MIMI_SECRET_PROXY_TYPE      ""
#endif
#ifndef MIMI_SECRET_SEARCH_KEY
#define MIMI_SECRET_SEARCH_KEY      ""
#endif
#ifndef MIMI_SECRET_FEISHU_APP_ID
#define MIMI_SECRET_FEISHU_APP_ID   ""
#endif
#ifndef MIMI_SECRET_FEISHU_APP_SECRET
#define MIMI_SECRET_FEISHU_APP_SECRET ""
#endif
#ifndef MIMI_SECRET_TAVILY_KEY
#define MIMI_SECRET_TAVILY_KEY      ""
#endif

/* WiFi */
#define MIMI_WIFI_MAX_RETRY          10
#define MIMI_WIFI_RETRY_BASE_MS      1000
#define MIMI_WIFI_RETRY_MAX_MS       30000

/* Telegram Bot */
#define MIMI_TG_POLL_TIMEOUT_S       30
#define MIMI_TG_MAX_MSG_LEN          4096
#define MIMI_TG_POLL_STACK           (12 * 1024)
#define MIMI_TG_POLL_PRIO            5
#define MIMI_TG_POLL_CORE            0
#define MIMI_TG_CARD_SHOW_MS         3000
#define MIMI_TG_CARD_BODY_SCALE      3

/* Feishu Bot */
#define MIMI_FEISHU_MAX_MSG_LEN          4096
#define MIMI_FEISHU_POLL_STACK           (12 * 1024)
#define MIMI_FEISHU_POLL_PRIO            5
#define MIMI_FEISHU_POLL_CORE            0
#define MIMI_FEISHU_WEBHOOK_PORT         18790
#define MIMI_FEISHU_WEBHOOK_PATH         "/feishu/events"
#define MIMI_FEISHU_WEBHOOK_MAX_BODY     (16 * 1024)

/* Agent Loop */
#define MIMI_AGENT_STACK             (24 * 1024)
#define MIMI_AGENT_PRIO              6
#ifdef CONFIG_FREERTOS_UNICORE
#define MIMI_AGENT_CORE              0
#else
#define MIMI_AGENT_CORE              1
#endif
#ifdef CONFIG_SPIRAM
#define MIMI_AGENT_MAX_HISTORY       20
#define MIMI_AGENT_MAX_TOOL_ITER     10
#else
/* Single-core, no PSRAM: limit history and tool iterations to reduce memory pressure */
#define MIMI_AGENT_MAX_HISTORY       10
#define MIMI_AGENT_MAX_TOOL_ITER     7
#endif
#define MIMI_MAX_TOOL_CALLS          4
#define MIMI_AGENT_SEND_WORKING_STATUS 1

/* Timezone (POSIX TZ format) */
#define MIMI_TIMEZONE                "CTS-8"

/* Large-buffer allocator: use PSRAM when available, internal RAM otherwise */
#ifdef CONFIG_SPIRAM
#define MIMI_MALLOC_LARGE  MALLOC_CAP_SPIRAM
#else
#define MIMI_MALLOC_LARGE  MALLOC_CAP_DEFAULT
#endif

/* LLM */
#define MIMI_LLM_DEFAULT_MODEL       "minimax/minimax-m2.5"    //"openrouter/free" //"claude-opus-4-5"
#define MIMI_LLM_PROVIDER_DEFAULT    "openrouter" //"openai"   //"anthropic"
#define MIMI_LLM_MAX_TOKENS          4096
#define MIMI_LLM_API_URL             "https://api.anthropic.com/v1/messages"
#define MIMI_OPENAI_API_URL          "https://api.openai.com/v1/chat/completions"
#define MIMI_OPENROUTER_API_URL      "https://openrouter.ai/api/v1/chat/completions"
#define MIMI_OPENROUTER_APP_NAME     "MimiClaw"
#define MIMI_LLM_API_VERSION         "2023-06-01"
#ifdef CONFIG_SPIRAM
#define MIMI_LLM_STREAM_BUF_SIZE     (32 * 1024)
#else
#define MIMI_LLM_STREAM_BUF_SIZE     (8 * 1024)
#endif
#define MIMI_LLM_LOG_VERBOSE_PAYLOAD 0
#define MIMI_LLM_LOG_PREVIEW_BYTES   160

/* Message Bus */
#define MIMI_BUS_QUEUE_LEN           16
#define MIMI_OUTBOUND_STACK          (12 * 1024)
#define MIMI_OUTBOUND_PRIO           5
#define MIMI_OUTBOUND_CORE           0

/* Memory / SPIFFS */
#define MIMI_SPIFFS_BASE             "/spiffs"
#define MIMI_SPIFFS_CONFIG_DIR       MIMI_SPIFFS_BASE "/config"
#define MIMI_SPIFFS_MEMORY_DIR       MIMI_SPIFFS_BASE "/memory"
#define MIMI_SPIFFS_SESSION_DIR      MIMI_SPIFFS_BASE "/sessions"
#define MIMI_MEMORY_FILE             MIMI_SPIFFS_MEMORY_DIR "/MEMORY.md"
#define MIMI_SOUL_FILE               MIMI_SPIFFS_CONFIG_DIR "/SOUL.md"
#define MIMI_USER_FILE               MIMI_SPIFFS_CONFIG_DIR "/USER.md"
#ifdef CONFIG_SPIRAM
#define MIMI_CONTEXT_BUF_SIZE        (16 * 1024)
#else
#define MIMI_CONTEXT_BUF_SIZE        (6 * 1024)
#endif
#define MIMI_SESSION_MAX_MSGS        20

/* Cron / Heartbeat */
#define MIMI_CRON_FILE               MIMI_SPIFFS_BASE "/cron.json"
#define MIMI_CRON_MAX_JOBS           16
#define MIMI_CRON_CHECK_INTERVAL_MS  (60 * 1000)
#define MIMI_HEARTBEAT_FILE          MIMI_SPIFFS_BASE "/HEARTBEAT.md"
#define MIMI_HEARTBEAT_INTERVAL_MS   (30 * 60 * 1000)

/* GPIO */
#define MIMI_GPIO_CONFIG_SECTION     1   /* enable GPIO tools */

/* Skills */
#define MIMI_SKILLS_PREFIX           MIMI_SPIFFS_BASE "/skills/"

/* WebSocket Gateway */
#define MIMI_WS_PORT                 18789
#define MIMI_WS_MAX_CLIENTS          4

/* Serial CLI */
#define MIMI_CLI_STACK               (4 * 1024)
#define MIMI_CLI_PRIO                3
#define MIMI_CLI_CORE                0

/* NVS Namespaces */
#define MIMI_NVS_WIFI                "wifi_config"
#define MIMI_NVS_TG                  "tg_config"
#define MIMI_NVS_FEISHU              "feishu_config"
#define MIMI_NVS_LLM                 "llm_config"
#define MIMI_NVS_PROXY               "proxy_config"
#define MIMI_NVS_SEARCH              "search_config"

/* NVS Keys */
#define MIMI_NVS_KEY_SSID            "ssid"
#define MIMI_NVS_KEY_PASS            "password"
#define MIMI_NVS_KEY_TG_TOKEN        "bot_token"
#define MIMI_NVS_KEY_FEISHU_APP_ID   "app_id"
#define MIMI_NVS_KEY_FEISHU_APP_SECRET "app_secret"
#define MIMI_NVS_KEY_API_KEY         "api_key"
#define MIMI_NVS_KEY_TAVILY_KEY      "tavily_key"
#define MIMI_NVS_KEY_MODEL           "model"
#define MIMI_NVS_KEY_PROVIDER        "provider"
#define MIMI_NVS_KEY_PROXY_HOST      "host"
#define MIMI_NVS_KEY_PROXY_PORT      "port"
#define MIMI_NVS_KEY_PROXY_TYPE      "proxy_type"

/* ── Display backend ──────────────────────────────────────────────── */
/* Selected via:  idf.py menuconfig  →  MimiClaw Display              */
/* CONFIG_MIMI_DISPLAY_RM68140 / _SSD1306 / _NONE are set by Kconfig. */

/* Display task (shared by backends that have one) */
#define MIMI_DISPLAY_TASK_STACK  (6 * 1024)
#define MIMI_DISPLAY_TASK_PRIO   3
#define MIMI_DISPLAY_TASK_CORE   0

/* ── Kconfig → short-name adapters (used by driver source files) ──── */

#ifdef CONFIG_MIMI_DISPLAY_SSD1306
#  define MIMI_DISPLAY_I2C_PORT    CONFIG_MIMI_DISPLAY_I2C_PORT
#  define MIMI_DISPLAY_SDA_PIN     CONFIG_MIMI_DISPLAY_SDA_PIN
#  define MIMI_DISPLAY_SCL_PIN     CONFIG_MIMI_DISPLAY_SCL_PIN
#  define MIMI_DISPLAY_I2C_ADDR    CONFIG_MIMI_DISPLAY_I2C_ADDR
#endif

#ifdef CONFIG_MIMI_DISPLAY_ILI9341
#  define MIMI_ILI_CS_PIN    CONFIG_MIMI_ILI_CS_PIN
#  define MIMI_ILI_DC_PIN    CONFIG_MIMI_ILI_DC_PIN
#  define MIMI_ILI_RST_PIN   CONFIG_MIMI_ILI_RST_PIN
#  define MIMI_ILI_MOSI_PIN  CONFIG_MIMI_ILI_MOSI_PIN
#  define MIMI_ILI_SCLK_PIN  CONFIG_MIMI_ILI_SCLK_PIN
#  define MIMI_ILI_BL_PIN    CONFIG_MIMI_ILI_BL_PIN
#  define MIMI_ILI_SPI_HZ    CONFIG_MIMI_ILI_SPI_HZ
#endif

#ifdef CONFIG_MIMI_DISPLAY_RM68140
#  define MIMI_RM_CS_PIN    CONFIG_MIMI_RM_CS_PIN
#  define MIMI_RM_DC_PIN    CONFIG_MIMI_RM_DC_PIN
#  define MIMI_RM_RST_PIN   CONFIG_MIMI_RM_RST_PIN
#  define MIMI_RM_WR_PIN    CONFIG_MIMI_RM_WR_PIN
#  define MIMI_RM_RD_PIN    CONFIG_MIMI_RM_RD_PIN
#  define MIMI_RM_BL_PIN    CONFIG_MIMI_RM_BL_PIN
#  define MIMI_RM_D0        CONFIG_MIMI_RM_D0
#  define MIMI_RM_D1        CONFIG_MIMI_RM_D1
#  define MIMI_RM_D2        CONFIG_MIMI_RM_D2
#  define MIMI_RM_D3        CONFIG_MIMI_RM_D3
#  define MIMI_RM_D4        CONFIG_MIMI_RM_D4
#  define MIMI_RM_D5        CONFIG_MIMI_RM_D5
#  define MIMI_RM_D6        CONFIG_MIMI_RM_D6
#  define MIMI_RM_D7        CONFIG_MIMI_RM_D7
#endif

/* WiFi Onboarding (Captive Portal) */
#define MIMI_ONBOARD_AP_PREFIX    "MimiClaw-"
#define MIMI_ONBOARD_AP_PASS      ""
#define MIMI_ONBOARD_HTTP_PORT    80
#define MIMI_ONBOARD_DNS_STACK    (4 * 1024)
#define MIMI_ONBOARD_MAX_SCAN     20
