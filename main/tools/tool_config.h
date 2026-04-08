#pragma once

#include "esp_err.h"
#include <stddef.h>

/**
 * Execute the set_config tool.
 * Supported keys: model, provider, api_key, tavily_key, search_key
 */
esp_err_t tool_config_set_execute(const char *input_json, char *output, size_t output_size);
