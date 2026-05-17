#pragma once

#include <stdint.h>

// Initialize display and start LVGL task
void display_init(void);

// Set expression (thread-safe, called from tools)
void display_set_expr(int id);

// Get current expression
int display_get_expr(void);

// Check if thinking animation is in progress
bool display_is_thinking(void);

// Get LVGL FPS average (frames per second, updated once per second)
uint32_t lvgl_fps_avg_get(void);

// Get display task CPU usage (0-100%)
uint8_t display_cpu_usage_get(void);

// List of expressions (public IDs)
#define EXPR_NEUTRAL    0
#define EXPR_HAPPY      1
#define EXPR_WINK       2
#define EXPR_SURPRISED  3
#define EXPR_SLEEPY     4
#define EXPR_THINKING   5
#define EXPR_SUSPICIOUS 6
#define EXPR_CRY        7
#define EXPR_OOPS       8
#define EXPR_SAD        9

// Internal only - used by thinking animation, not exposed in UI
// Stored at ex[] indices 10 and 11
#define EXPR_LEFT_IDX   10
#define EXPR_RIGHT_IDX  11
