#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

esp_err_t http_channel_start(QueueHandle_t input_queue);
