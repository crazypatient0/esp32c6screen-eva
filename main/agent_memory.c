#include "agent_memory.h"
#include "memory.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "agent_mem";

// RAM-resident memory buffer, survives across calls
static char s_memory[AGENT_MEMORY_MAX_SIZE + 1] = {0};

// Load memory from NVS into s_memory on startup
void agent_memory_init(void)
{
    s_memory[0] = '\0';
    if (!memory_get(AGENT_MEMORY_NVS_KEY, s_memory, sizeof(s_memory))) {
        ESP_LOGI(TAG, "No prior memory found, starting fresh");
    } else {
        ESP_LOGI(TAG, "Loaded memory (%d bytes): %.80s",
                 (int)strlen(s_memory), s_memory);
    }
}

// Get current memory text
void agent_memory_get(char *buf, size_t buf_size)
{
    if (!buf || buf_size == 0) return;
    strncpy(buf, s_memory, buf_size - 1);
    buf[buf_size - 1] = '\0';
}

// Append a new exchange, truncate oldest content if over 1KB, persist to NVS
void agent_memory_append(const char *user_msg, const char *assistant_response)
{
    if (!user_msg || !assistant_response) return;

    // Format the new entry
    char entry[512];
    int entry_len = snprintf(entry, sizeof(entry),
        "\nUser: %.*s\nAssistant: %.*s",
        200, user_msg,
        200, assistant_response);

    if (entry_len <= 0) return;

    size_t current_len = strlen(s_memory);
    size_t total = current_len + entry_len;

    if (total >= AGENT_MEMORY_MAX_SIZE) {
        // Need to make room: drop oldest entries from front
        // Entries start with "\nUser:", find the second occurrence (keep at least one)
        const char *first_user = strstr(s_memory, "\nUser:");
        if (first_user) {
            const char *second_user = strstr(first_user + 1, "\nUser:");
            if (second_user) {
                // Truncate from second entry onward, then append new entry
                size_t kept_len = strlen(second_user);
                // Verify we actually have room
                if (kept_len + entry_len < AGENT_MEMORY_MAX_SIZE) {
                    memmove(s_memory, second_user, kept_len + 1);
                    current_len = kept_len;
                } else {
                    // Even after dropping all, can't fit: just keep last entry
                    s_memory[0] = '\0';
                    current_len = 0;
                }
            } else {
                // Only one entry in memory and it's full: can't fit, skip update
                ESP_LOGW(TAG, "Memory entry too large to fit, skipping");
                return;
            }
        } else {
            s_memory[0] = '\0';
            current_len = 0;
        }
    }

    // Append new entry
    int remaining = AGENT_MEMORY_MAX_SIZE - current_len;
    if (remaining > 1) {
        snprintf(s_memory + current_len, (size_t)remaining, "%s", entry);
    }

    // Persist to NVS
    esp_err_t err = memory_set(AGENT_MEMORY_NVS_KEY, s_memory);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Memory updated (%d bytes)", (int)strlen(s_memory));
    } else {
        ESP_LOGE(TAG, "Failed to persist memory: %s", esp_err_to_name(err));
    }
}

// Clear all memory
void agent_memory_reset(void)
{
    s_memory[0] = '\0';
    memory_delete(AGENT_MEMORY_NVS_KEY);
    ESP_LOGI(TAG, "Memory cleared");
}
