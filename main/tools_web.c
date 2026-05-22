#include "tools_handlers.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "memory.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "tools_web";

#define MAX_RESULT_SIZE 1800

typedef struct {
    char *buf;
    size_t len;
    size_t max;
    bool truncated;
} web_buf_ctx_t;

// Percent-encode non-ASCII characters in a URL path/query string.
// Host part (between :// and first /) is left unchanged.
// Returns encoded URL into dst (must have enough space, worst case = 3x len).
// Returns dst on success, or NULL on buffer overflow.
static char *url_encode_non_ascii(const char *url, char *dst, size_t dst_size)
{
    size_t di = 0;
    const char *p = url;

    // Find host end (first '/' after "://")
    const char *host_end = strstr(url, "://");
    if (host_end) {
        host_end = strchr(host_end + 3, '/');
    }

    while (*p && di < dst_size - 1) {
        if ((unsigned char)*p < 0x80) {
            // ASCII: copy as-is
            dst[di++] = *p++;
        } else {
            // Non-ASCII: percent-encode
            if (di + 3 >= dst_size) return NULL;
            snprintf(&dst[di], 4, "%%%02X", (unsigned char)*p);
            di += 3;
            p++;
        }
    }
    dst[di] = '\0';
    return dst;
}

static esp_err_t web_http_event_handler(esp_http_client_event_t *evt)
{
    web_buf_ctx_t *ctx = (web_buf_ctx_t *)evt->user_data;
    if (!ctx || !ctx->buf) return ESP_OK;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (evt->data_len > 0) {
                for (int i = 0; i < evt->data_len; i++) {
                    if (ctx->len >= ctx->max - 1) {
                        ctx->truncated = true;
                        return ESP_OK;
                    }
                    ctx->buf[ctx->len++] = ((char *)evt->data)[i];
                }
                ctx->buf[ctx->len] = '\0';
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

// Minimal HTML tag stripper: removes <...> tags and decodes &amp; &lt; &gt; &quot; &nbsp;
static void strip_html_tags_inplace(char *dest, const char *src, size_t dest_len)
{
    size_t di = 0;
    bool in_tag = false;
    for (size_t si = 0; src[si] && di < dest_len - 1; si++) {
        if (src[si] == '<') { in_tag = true; continue; }
        if (src[si] == '>') { in_tag = false; continue; }
        if (in_tag) continue;
        // Decode entities
        if (src[si] == '&') {
            if (strncmp(src + si, "&amp;", 5) == 0) { dest[di++] = '&'; si += 4; continue; }
            if (strncmp(src + si, "&lt;", 4) == 0) { dest[di++] = '<'; si += 3; continue; }
            if (strncmp(src + si, "&gt;", 4) == 0) { dest[di++] = '>'; si += 3; continue; }
            if (strncmp(src + si, "&quot;", 6) == 0) { dest[di++] = '"'; si += 5; continue; }
            if (strncmp(src + si, "&nbsp;", 6) == 0) { dest[di++] = ' '; si += 5; continue; }
        }
        dest[di++] = src[si];
    }
    dest[di] = '\0';
}

bool tools_web_fetch_handler(const cJSON *input, char *result, size_t result_len)
{
    cJSON *url_json = cJSON_GetObjectItem(input, "url");
    if (!url_json || !cJSON_IsString(url_json) || !url_json->valuestring || !url_json->valuestring[0]) {
        snprintf(result, result_len, "Error: 'url' required (string)");
        return false;
    }

    const char *url = url_json->valuestring;
    ESP_LOGI(TAG, "web_fetch: %s", url);

    // Use static buffers to avoid stack overflow on ESP32-C6's small task stack
    static char s_response[MAX_RESULT_SIZE];
    static char s_clean[MAX_RESULT_SIZE];
    static char s_out[MAX_RESULT_SIZE];
    static char s_url_encoded[MAX_RESULT_SIZE * 3];  // worst case: every char becomes %XX
    s_response[0] = '\0';
    s_clean[0] = '\0';
    s_out[0] = '\0';
    s_url_encoded[0] = '\0';
    web_buf_ctx_t ctx = {.buf = s_response, .len = 0, .max = MAX_RESULT_SIZE, .truncated = false};

    // Encode non-ASCII characters in URL path/query (esp_http_client can't handle them)
    const char *url_to_use = url;
    if (url_encode_non_ascii(url, s_url_encoded, sizeof(s_url_encoded)) != NULL) {
        url_to_use = s_url_encoded;
        ESP_LOGI(TAG, "web_fetch: encoded URL = %s", url_to_use);
    }

    esp_http_client_config_t config = {
        .url = url_to_use,
        .event_handler = web_http_event_handler,
        .user_data = &ctx,
        .timeout_ms = 3000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        snprintf(result, result_len, "Error: failed to init HTTP client");
        return true;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "web_fetch HTTP error: %s", esp_err_to_name(err));
        snprintf(result, result_len, "Error: HTTP error %s", esp_err_to_name(err));
        return true;
    }

    if (status != 200) {
        snprintf(result, result_len, "Error: HTTP status %d", status);
        return true;
    }

    // Strip HTML tags for a cleaner text output
    strip_html_tags_inplace(s_clean, s_response, sizeof(s_clean));

    // Collapse whitespace
    size_t di = 0;
    bool last_was_space = false;
    for (size_t si = 0; s_clean[si] && di < sizeof(s_out) - 1; si++) {
        if (s_clean[si] == ' ' || s_clean[si] == '\t' || s_clean[si] == '\n' || s_clean[si] == '\r') {
            if (!last_was_space && di > 0) {
                s_out[di++] = ' ';
                last_was_space = true;
            }
        } else {
            s_out[di++] = s_clean[si];
            last_was_space = false;
        }
    }
    s_out[di] = '\0';

    if (ctx.truncated) {
        snprintf(result, result_len, "[truncated] %s [truncated]", s_out);
    } else {
        snprintf(result, result_len, "%s", s_out);
    }
    return true;
}

// Minimal JSON parser for Exa search results
// Extracts title and url from each result object
// Uses static buffers to avoid stack overflow on ESP32-C6's small task stack
static void parse_exa_results(const char *json, char *result, size_t result_len)
{
    // Check for error responses first
    if (strstr(json, "\"error\"") != NULL) {
        // Try to extract error message
        const char *msg = strstr(json, "\"error\"");
        const char *colon = strchr(msg, ':');
        if (colon) {
            const char *quote = strchr(colon + 1, '"');
            if (quote) {
                const char *end = strchr(quote + 1, '"');
                if (end && end - quote - 1 < (int)result_len - 20) {
                    int len = end - quote - 1;
                    snprintf(result, result_len, "Error from Exa: %.*s", len, quote + 1);
                    return;
                }
            }
        }
        snprintf(result, result_len, "Error: Exa API returned an error");
        return;
    }

    const char *results_start = strstr(json, "\"results\"");
    if (!results_start) {
        snprintf(result, result_len, "Error: no results field in Exa response (malformed JSON)");
        return;
    }

    // Find opening bracket of results array
    const char *arr = strchr(results_start, '[');
    if (!arr) {
        snprintf(result, result_len, "Error: results is not an array");
        return;
    }

    int count = 0;
    // Use static buffer to avoid stack overflow
    static char s_output[2048];
    s_output[0] = '\0';
    size_t oi = 0;

    // Walk through array elements
    const char *p = arr + 1;
    while (*p && count < 8) {
        // Skip whitespace
        while (*p && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t' || *p == ',')) p++;

        if (*p != '{') break;

        // Find title
        const char *title = NULL;
        size_t title_len = 0;
        const char *tp = strstr(p, "\"title\"");
        if (tp && tp < p + 4096) {
            const char *colon = strchr(tp, ':');
            if (colon && colon - tp < 20) {
                const char *quote = strchr(colon + 1, '"');
                if (quote) {
                    title = quote + 1;
                    const char *end = strchr(title, '"');
                    if (end) {
                        title_len = end - title;
                        if (title_len > 200) title_len = 200;
                    }
                }
            }
        }

        // Find url
        const char *url = NULL;
        size_t url_len = 0;
        const char *up = strstr(p, "\"url\"");
        if (up && up < p + 4096) {
            const char *colon = strchr(up, ':');
            if (colon && colon - up < 20) {
                const char *quote = strchr(colon + 1, '"');
                if (quote) {
                    url = quote + 1;
                    const char *end = strchr(url, '"');
                    if (end) {
                        url_len = end - url;
                        if (url_len > 500) url_len = 500;
                    }
                }
            }
        }

        if (title && url && title_len > 0 && url_len > 0) {
            count++;
            int written = snprintf(s_output + oi, sizeof(s_output) - oi - 1, "%d. %.*s\n   %.*s\n\n",
                count, (int)title_len, title, (int)url_len, url);
            if (written > 0) oi += written;
            if (oi >= sizeof(s_output) - 100) break;
        }

        // Move past this object
        const char *obj_end = strchr(p + 1, '}');
        if (!obj_end) break;
        p = obj_end + 1;
    }

    if (count == 0) {
        snprintf(result, result_len, "No results found");
    } else {
        // result_len may be only 512 bytes, truncate if needed
        size_t header_len = snprintf(NULL, 0, "Search results (%d):\n\n", count);
        size_t avail = (result_len > header_len + 1) ? (result_len - header_len - 1) : 0;
        size_t copy_len = (avail < strlen(s_output)) ? avail : strlen(s_output);
        snprintf(result, result_len, "Search results (%d):\n\n%.*s", count, (int)copy_len, s_output);
    }
}

bool tools_web_search_handler(const cJSON *input, char *result, size_t result_len)
{
    cJSON *query_json = cJSON_GetObjectItem(input, "query");
    if (!query_json || !cJSON_IsString(query_json) || !query_json->valuestring || !query_json->valuestring[0]) {
        snprintf(result, result_len, "Error: 'query' required (string)");
        return false;
    }

    const char *query = query_json->valuestring;
    ESP_LOGI(TAG, "web_search: %s", query);

    // Read Exa API key from NVS
    char exa_key[128] = {0};
    if (!memory_get("exa_api_key", exa_key, sizeof(exa_key)) || exa_key[0] == '\0') {
        ESP_LOGW(TAG, "web_search: exa_api_key not found in NVS");
        snprintf(result, result_len, "Error: Exa API key not configured. Set 'Search API Key' in the web UI config panel.");
        return true;
    }
    ESP_LOGI(TAG, "web_search: exa_api_key found, len=%d", (int)strlen(exa_key));

    // Build Exa API request body
    char body[512];
    snprintf(body, sizeof(body),
        "{\"query\":\"%s\",\"num_results\":5,\"contents\":{\"text\":true,\"highlights\":true}}",
        query);

    // Use static buffers to avoid stack overflow on ESP32-C6's small task stack
    static char s_response[3200];
    static char s_parsed[2048];
    s_response[0] = '\0';
    s_parsed[0] = '\0';
    web_buf_ctx_t ctx = {.buf = s_response, .len = 0, .max = sizeof(s_response), .truncated = false};

    esp_http_client_config_t config = {
        .url = "https://api.exa.ai/search",
        .event_handler = web_http_event_handler,
        .user_data = &ctx,
        .timeout_ms = 8000,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        snprintf(result, result_len, "Error: failed to init HTTP client");
        return true;
    }

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "x-api-key", exa_key);
    esp_http_client_set_post_field(client, body, strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "web_search HTTP error: %s (query: %s)", esp_err_to_name(err), query);
        snprintf(result, result_len, "Error: HTTP error %s", esp_err_to_name(err));
        return true;
    }

    if (status != 200) {
        ESP_LOGW(TAG, "web_search Exa API status %d (query: %s), body: %s",
                 status, query, s_response);
        snprintf(result, result_len, "Error: Exa API status %d", status);
        return true;
    }

    ESP_LOGI(TAG, "web_search Exa response: %.*s", (int)(strlen(s_response) < 200 ? strlen(s_response) : 200), s_response);

    // Parse Exa JSON response
    parse_exa_results(s_response, s_parsed, sizeof(s_parsed));
    ESP_LOGI(TAG, "web_search parsed result: %.*s", (int)(strlen(s_parsed) < 200 ? strlen(s_parsed) : 200), s_parsed);
    snprintf(result, result_len, "%s", s_parsed);
    return true;
}
