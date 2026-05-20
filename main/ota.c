#include "ota.h"
#include "config.h"
#include "esp_log.h"
#include "esp_ota_ops.h"

static const char *TAG = "ota";
static bool s_pending_verify = false;
static bool s_ota_in_progress = false;
static esp_ota_handle_t s_ota_handle = 0;
static const esp_partition_t *s_ota_partition = NULL;

// Current firmware version (set at compile time)
#ifndef ZCLAW_VERSION
#define ZCLAW_VERSION "dev"
#endif

esp_err_t ota_init(void)
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (!running) {
        ESP_LOGE(TAG, "Failed to get running partition");
        return ESP_FAIL;
    }

    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            s_pending_verify = true;
            ESP_LOGW(TAG, "Image pending verification; will mark valid after stable boot window");
        }
    }

    ESP_LOGI(TAG, "Running from: %s (v%s)", running->label, ZCLAW_VERSION);
    return ESP_OK;
}

const char *ota_get_version(void)
{
    return ZCLAW_VERSION;
}

esp_err_t ota_mark_valid(void)
{
    return esp_ota_mark_app_valid_cancel_rollback();
}

esp_err_t ota_mark_valid_if_pending(void)
{
    if (!s_pending_verify) {
        return ESP_OK;
    }

    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err == ESP_OK) {
        s_pending_verify = false;
    }
    return err;
}

bool ota_is_pending_verify(void)
{
    return s_pending_verify;
}

esp_err_t ota_rollback(void)
{
    esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
    // This should not return if successful
    return err;
}

// --- HTTP OTA update ---

esp_err_t ota_update_begin(void)
{
    if (s_ota_in_progress) {
        ESP_LOGW(TAG, "OTA already in progress");
        return ESP_ERR_INVALID_STATE;
    }

    s_ota_partition = esp_ota_get_next_update_partition(NULL);
    if (!s_ota_partition) {
        ESP_LOGE(TAG, "No OTA partition found");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Starting OTA to partition: %s (size=%d)", s_ota_partition->label, s_ota_partition->size);

    esp_err_t err = esp_ota_begin(s_ota_partition, OTA_SIZE_UNKNOWN, &s_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
        s_ota_partition = NULL;
        return err;
    }

    s_ota_in_progress = true;
    ESP_LOGI(TAG, "OTA begin OK, ready to receive data");
    return ESP_OK;
}

esp_err_t ota_update_write(const void *buf, size_t len)
{
    if (!s_ota_in_progress || !s_ota_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    return esp_ota_write(s_ota_handle, buf, len);
}

esp_err_t ota_update_end(void)
{
    if (!s_ota_in_progress) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t err = esp_ota_end(s_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed: %s", esp_err_to_name(err));
    }

    if (err == ESP_OK) {
        err = esp_ota_set_boot_partition(s_ota_partition);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "OTA update complete, rebooting...");
        }
    }

    s_ota_in_progress = false;
    s_ota_handle = 0;
    s_ota_partition = NULL;
    return err;
}

bool ota_is_in_progress(void)
{
    return s_ota_in_progress;
}
