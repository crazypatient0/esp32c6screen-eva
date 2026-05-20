#include "tools_handlers.h"
#include "display_task.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include <stdio.h>

static const char *TAG = "tools_display";

bool tools_set_expression_handler(const cJSON *input, char *result, size_t result_len)
{
    const cJSON *id_item = cJSON_GetObjectItem(input, "id");
    if (!id_item || !cJSON_IsNumber(id_item)) {
        snprintf(result, result_len, "Error: missing or invalid 'id' field");
        return false;
    }

    int id = id_item->valueint;
    if (id < 0 || id > 9) {
        snprintf(result, result_len, "Error: expression id must be 0-9");
        return false;
    }

    display_set_expr(id);

    static const char *names[] = {
        "neutral", "happy ∩∩", "wink", "surprised O_O", "sleepy __",
        "thinking ↔", "suspicious",
        "cry T_T", "oops >.<", "sad ∪∪"
    };
    snprintf(result, result_len, "Expression changed to %s (%d)", names[id], id);
    ESP_LOGI(TAG, "set_expression: %d", id);
    return true;
}

bool tools_play_expression_handler(const cJSON *input, char *result, size_t result_len)
{
    const cJSON *id_item = cJSON_GetObjectItem(input, "id");
    if (!id_item || !cJSON_IsNumber(id_item)) {
        snprintf(result, result_len, "Error: missing or invalid 'id' field");
        return false;
    }

    int id = id_item->valueint;
    if (id < 0 || id > 9) {
        snprintf(result, result_len, "Error: expression id must be 0-9");
        return false;
    }

    display_play_expr(id);

    static const char *names[] = {
        "neutral", "happy ∩∩", "wink", "surprised O_O", "sleepy __",
        "thinking ↔", "suspicious",
        "cry T_T", "oops >.<", "sad ∪∪"
    };
    snprintf(result, result_len, "Playing expression: %s (%d) — 2-second animation, then returns to previous", names[id], id);
    ESP_LOGI(TAG, "play_expression: %d", id);
    return true;
}

bool tools_servo_set_handler(const cJSON *input, char *result, size_t result_len)
{
    const cJSON *id_item = cJSON_GetObjectItem(input, "id");
    const cJSON *angle_item = cJSON_GetObjectItem(input, "angle");

    if (!id_item || !cJSON_IsNumber(id_item) || !angle_item || !cJSON_IsNumber(angle_item)) {
        snprintf(result, result_len, "Error: missing or invalid 'id' or 'angle'");
        return false;
    }

    int id = id_item->valueint;
    int angle = angle_item->valueint;

    if (id < 0 || id > 1) {
        snprintf(result, result_len, "Error: servo id must be 0 or 1");
        return false;
    }
    if (angle < 0 || angle > 180) {
        snprintf(result, result_len, "Error: angle must be 0-180");
        return false;
    }

    // GPIOs available for servos: 0, 1, 9, 10, 12, 13, 23
    const int servo_pins[] = {12, 13};
    int pin = servo_pins[id];

    // Configure LEDC PWM for servo (50Hz, 0-180°)
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_14_BIT,
        .timer_num = LEDC_TIMER_0 + id,
        .freq_hz = 50,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer);

    ledc_channel_config_t channel = {
        .gpio_num = pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0 + id,
        .timer_sel = LEDC_TIMER_0 + id,
        .duty = 0,
        .hpoint = 0
    };
    ledc_channel_config(&channel);

    // Convert 0-180 to duty (50Hz, 14-bit: 0-16383)
    // SG90: 0° = 500us, 90° = 1500us, 180° = 2500us
    // At 50Hz (20ms period), 14-bit resolution:
    // duty = (pulse_us / 20000) * 16384
    uint32_t pulse_us = 500 + (angle * 2000 / 180);
    uint32_t duty = (pulse_us * 16384) / 20000;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0 + id, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0 + id);

    snprintf(result, result_len, "Servo %d (GPIO%d) set to %d°", id, pin, angle);
    ESP_LOGI(TAG, "servo_set: id=%d pin=%d angle=%d", id, pin, angle);
    return true;
}
