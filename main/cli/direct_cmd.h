#pragma once

#include <stddef.h>
#include <stdbool.h>

/**
 * Check if a message is a direct command (starts with '!').
 */
static inline bool direct_cmd_is(const char *msg)
{
    return msg && msg[0] == '/';
}

/**
 * Execute a direct command and write the result to output.
 * @param msg        Full message string starting with '!'
 * @param output     Output buffer
 * @param output_size Size of output buffer
 */
void direct_cmd_execute(const char *msg, char *output, size_t output_size);
