# Display backend sources — only compiled when the matching backend is selected
set(DISPLAY_SRCS "display/display_service.c")
if(CONFIG_MIMI_DISPLAY_SSD1306)
    list(APPEND DISPLAY_SRCS "display/ssd1306.c")
endif()
if(CONFIG_MIMI_DISPLAY_RM68140)
    list(APPEND DISPLAY_SRCS "display/rm68140.c")
    list(APPEND DISPLAY_SRCS "display/img_banner.c")
endif()
if(CONFIG_MIMI_DISPLAY_ILI9341)
    list(APPEND DISPLAY_SRCS "display/ili9341.c")
endif()
if(CONFIG_MIMI_DISPLAY_FONT_CJK)
    list(APPEND DISPLAY_SRCS "display/source-han-sans16.c")
endif()

# Display backend component requirements
set(DISPLAY_REQUIRES "")
if(CONFIG_MIMI_DISPLAY_SSD1306)
    list(APPEND DISPLAY_REQUIRES esp_driver_i2c)
endif()
if(CONFIG_MIMI_DISPLAY_RM68140)
    list(APPEND DISPLAY_REQUIRES lvgl__lvgl)
endif()
if(CONFIG_MIMI_DISPLAY_ILI9341)
    list(APPEND DISPLAY_REQUIRES lvgl__lvgl esp_driver_spi)
endif()

idf_component_register(
    SRCS
        "mimi.c"
        "bus/message_bus.c"
        "wifi/wifi_manager.c"
        "channels/telegram/telegram_bot.c"
        "channels/feishu/feishu_bot.c"
        "llm/llm_proxy.c"
        "agent/agent_loop.c"
        "agent/context_builder.c"
        "memory/memory_store.c"
        "memory/session_mgr.c"
        "gateway/ws_server.c"
        "cli/serial_cli.c"
        "proxy/http_proxy.c"
        "cron/cron_service.c"
        "heartbeat/heartbeat.c"
        "tools/tool_registry.c"
        "tools/tool_cron.c"
        "tools/tool_config.c"
        "cli/direct_cmd.c"
        "tools/tool_web_search.c"
        "tools/tool_get_time.c"
        "tools/tool_files.c"
        "tools/tool_gpio.c"
        "tools/gpio_policy.c"
        "skills/skill_loader.c"
        "onboard/wifi_onboard.c"
        ${DISPLAY_SRCS}
    INCLUDE_DIRS
        "."
    REQUIRES
        nvs_flash esp_wifi esp_netif esp_http_client esp_http_server
        esp_https_ota esp_event json spiffs console vfs app_update esp-tls
        esp_timer esp_websocket_client esp_driver_gpio esp_driver_spi esp_driver_i2c
        ${DISPLAY_REQUIRES}
)
