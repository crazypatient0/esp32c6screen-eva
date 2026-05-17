#pragma once

#include <stdint.h>

// Initialize display and start LVGL task
void display_init(void);

// Set expression (thread-safe, called from tools)
void display_set_expr(int id);

// Get current expression
int display_get_expr(void);

// Get LVGL FPS average (frames per second, updated once per second)
uint32_t lvgl_fps_avg_get(void);

// List of expressions
#define EXPR_NEUTRAL    0
#define EXPR_HAPPY      1
#define EXPR_WINK       2
#define EXPR_SURPRISED  3
#define EXPR_SLEEPY     4
#define EXPR_LEFT       5
#define EXPR_RIGHT      6
#define EXPR_SUSPICIOUS 7
#define EXPR_CRY        8
#define EXPR_OOPS       9
#define EXPR_SAD        10
