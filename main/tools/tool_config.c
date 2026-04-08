#include "tool_config.h"
#include "mimi_config.h"
#include "llm/llm_proxy.h"
#include "tools/tool_web_search.h"

#include <string.h>
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "tool_config";

esp_err_t tool_config_set_execute(const char *input_json, char *output, size_t output_size)
{
    cJSON *root = cJSON_Parse(input_json ? input_json : "{}");
    if (!root) {
        snprintf(output, output_size, "Error: invalid JSON input");
        return ESP_ERR_INVALID_ARG;
    }

    cJSON *key_item = cJSON_GetObjectItem(root, "key");
    cJSON *val_item = cJSON_GetObjectItem(root, "value");

    if (!cJSON_IsString(key_item) || !cJSON_IsString(val_item)) {
        cJSON_Delete(root);
        snprintf(output, output_size, "Error: 'key' and 'value' are required strings");
        return ESP_ERR_INVALID_ARG;
    }

    const char *key   = key_item->valuestring;
    const char *value = val_item->valuestring;

    esp_err_t err = ESP_ERR_NOT_FOUND;

    if (strcmp(key, "model") == 0) {
        err = llm_set_model(value);
        if (err == ESP_OK) {
            /* Reload LLM config so the new model is used immediately */
            llm_proxy_init();
            snprintf(output, output_size,
                     "OK: model set to '%s' (takes effect on next LLM call)", value);
        }
    } else if (strcmp(key, "provider") == 0) {
        err = llm_set_provider(value);
        if (err == ESP_OK) {
            llm_proxy_init();
            snprintf(output, output_size,
                     "OK: provider set to '%s' (takes effect on next LLM call)", value);
        }
    } else if (strcmp(key, "api_key") == 0) {
        err = llm_set_api_key(value);
        if (err == ESP_OK) {
            llm_proxy_init();
            snprintf(output, output_size, "OK: LLM api_key updated");
        }
    } else if (strcmp(key, "tavily_key") == 0) {
        err = tool_web_search_set_tavily_key(value);
        if (err == ESP_OK) {
            snprintf(output, output_size, "OK: tavily_key updated");
        }
    } else if (strcmp(key, "search_key") == 0) {
        err = tool_web_search_set_key(value);
        if (err == ESP_OK) {
            snprintf(output, output_size, "OK: search_key (Brave) updated");
        }
    } else {
        cJSON_Delete(root);
        snprintf(output, output_size,
                 "Error: unknown key '%s'. Valid keys: model, provider, api_key, tavily_key, search_key",
                 key);
        return ESP_ERR_INVALID_ARG;
    }

    if (err != ESP_OK) {
        snprintf(output, output_size, "Error: failed to save '%s' (%s)", key, esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "set_config: %s = %s", key,
                 (strcmp(key, "api_key") == 0 || strcmp(key, "tavily_key") == 0 ||
                  strcmp(key, "search_key") == 0) ? "***" : value);
    }

    cJSON_Delete(root);
    return err;
}
