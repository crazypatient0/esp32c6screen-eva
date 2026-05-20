#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "ST7789.h"
#include "RGB.h"
#include "LVGL_Driver.h"
#include "display_task.h"

// Frame counter for web UI FPS display (actual flushes, not loop iterations)
static const char *TAG_DISP = "display";

// Frame counter for web UI FPS display (actual flushes, not loop iterations)
static uint32_t s_fps_avg = 0;
static int64_t s_fps_last_update = 0;

// CPU measurement: track task active time
static int64_t s_cpu_last_update = 0;
static int64_t s_cpu_active_us = 0;
static uint8_t s_cpu_usage = 0;

// External flush counter from LVGL driver (volatile, defined in LVGL_Driver.c)
extern volatile uint32_t s_flush_count;

uint32_t lvgl_fps_avg_get(void) {
    return s_fps_avg;
}

uint8_t display_cpu_usage_get(void) {
    return s_cpu_usage;
}

// Expression state
static int current_expr = EXPR_NEUTRAL;
static int target_expr = EXPR_NEUTRAL;

// Thinking animation state (5-phase, loops while s_think_mode is set)
static bool s_thinking = false;
static int64_t s_thinking_start;
static int s_thinking_base;
static int s_thinking_phase = 0;
// s_think_mode: when true, the thinking animation loops continuously
// until display_think_stop() is called (e.g., when LLM response arrives)
static bool s_think_mode = false;

// Suspicious animation state (5-phase, unchanged)
static bool s_suspicious = false;
static int64_t s_suspicious_start;
static int s_suspicious_base;
static int s_suspicious_phase = 0;

// Unified expression play animation for non-thinking/non-suspicious expressions (3-phase)
static bool s_playing = false;
static int64_t s_playing_start;
static int s_playing_base;
static int s_playing_target;
static int s_playing_phase = 0;

// Extern for LVGL display
extern lv_disp_t *disp;

// Public IDs: 0-9. Internal (thinking animation): indices 10-11. Internal (suspicious animation): 12-13.
// is_line checks: a==1||a==8||a==9 (happy,cry,oops). sad(10) uses lines too.
static const int16_t ex[14][10] = {
    {90,50,35,61,15, 90,50,195,61,15},     // 0 neutral
    {0,0,0,0,0, 0,0,0,0,0},                // 1 ∩∩ happy
    {90,50,35,61,15, 90,6,195,83,3},       // 2 wink
    {120,75,20,49,25, 120,75,180,49,25},   // 3 surprised
    {80,10,40,81,5, 80,10,200,81,5},       // 4 sleepy
    {0,0,0,0,0, 0,0,0,0,0},                // 5 thinking (placeholder, not rendered directly)
    {70,35,35,69,12, 95,50,195,61,15},     // 6 suspicious (left-heavy)
    {0,0,0,0,0, 0,0,0,0,0},                // 7 T_T cry
    {0,0,0,0,0, 0,0,0,0,0},                // 8 >.< oops
    {0,0,0,0,0, 0,0,0,0,0},                // 9 ∪∪ sad (uses s1-s8 lines)
    {75,45,15,64,15, 75,45,175,64,15},     // 10 left (thinking internal)
    {75,45,55,64,15, 75,45,215,64,15},     // 11 right (thinking internal)
    {70,35,35,69,12, 95,50,195,61,15},     // 12 susp-left (left eye small)
    {95,50,23,61,15, 70,35,207,69,12},     // 13 susp-right (left eye big, right eye small)
};

// Line objects for expressions
static lv_obj_t *l_eye, *r_eye;
// ∩∩ lines (9 lines)
static lv_obj_t *h1,*h2,*h3,*h4,*h5,*h6,*h7,*h8,*h9; lv_point_t hp[9][2];
// T_T lines (4 lines)
static lv_obj_t *c1,*c2,*c3,*c4; lv_point_t cp[4][2];
// >.< lines (4 + 1 dot)
static lv_obj_t *g1,*g2,*g3,*g4,*gdot; lv_point_t gp[4][2];
// ∪∪ lines (8 lines)
static lv_obj_t *s1,*s2,*s3,*s4,*s5,*s6,*s7,*s8; lv_point_t sp[8][2];

// Animation helpers
static void aw(lv_obj_t *o,int32_t v){lv_obj_set_width(o,v);}
static void ah(lv_obj_t *o,int32_t v){lv_obj_set_height(o,v);}
static void ax(lv_obj_t *o,int32_t v){lv_obj_set_x(o,v);}
static void ay(lv_obj_t *o,int32_t v){lv_obj_set_y(o,v);}
static void ar(lv_obj_t *o,int32_t v){lv_obj_set_style_radius(o,v,0);}

static void sa(lv_obj_t *o,lv_anim_exec_xcb_t cb,int32_t f,int32_t t){
    static lv_anim_t a_pool[32];
    static int pool_idx = 0;
    lv_anim_t *a = &a_pool[pool_idx++];
    if(pool_idx >= 32) pool_idx = 0;
    lv_anim_init(a);
    lv_anim_set_var(a,o); lv_anim_set_exec_cb(a,cb);
    lv_anim_set_values(a,f,t); lv_anim_set_time(a,300);
    lv_anim_set_path_cb(a,lv_anim_path_ease_out);
    lv_anim_start(a);
}

static void apply(int a){
    int16_t *d=ex[a];
    int is_line=(a==1||a==7||a==8||a==9);
    int tw=is_line?0:d[0],th=is_line?0:d[1],tx=is_line?80:d[2],ty=is_line?86:d[3],tr=is_line?0:d[4];
    int tw2=is_line?0:d[5],th2=is_line?0:d[6],tx2=is_line?80:d[7],ty2=is_line?86:d[8],tr2=is_line?0:d[9];

    sa(l_eye,(lv_anim_exec_xcb_t)aw,lv_obj_get_width(l_eye),tw);
    sa(l_eye,(lv_anim_exec_xcb_t)ah,lv_obj_get_height(l_eye),th);
    sa(l_eye,(lv_anim_exec_xcb_t)ax,lv_obj_get_x(l_eye),tx);
    sa(l_eye,(lv_anim_exec_xcb_t)ay,lv_obj_get_y(l_eye),ty);
    sa(l_eye,(lv_anim_exec_xcb_t)ar,lv_obj_get_style_radius(l_eye,0),tr);
    sa(r_eye,(lv_anim_exec_xcb_t)aw,lv_obj_get_width(r_eye),tw2);
    sa(r_eye,(lv_anim_exec_xcb_t)ah,lv_obj_get_height(r_eye),th2);
    sa(r_eye,(lv_anim_exec_xcb_t)ax,lv_obj_get_x(r_eye),tx2);
    sa(r_eye,(lv_anim_exec_xcb_t)ay,lv_obj_get_y(r_eye),ty2);
    sa(r_eye,(lv_anim_exec_xcb_t)ar,lv_obj_get_style_radius(r_eye,0),tr2);

    // Hide all 26 line objects
    lv_obj_t *all[]={h1,h2,h3,h4,h5,h6,h7,h8,h9,c1,c2,c3,c4,g1,g2,g3,g4,gdot,s1,s2,s3,s4,s5,s6,s7,s8};
    for(int i=0;i<26;i++) lv_obj_add_flag(all[i],LV_OBJ_FLAG_HIDDEN);

    if(is_line){
        lv_obj_add_flag(l_eye,LV_OBJ_FLAG_HIDDEN); lv_obj_add_flag(r_eye,LV_OBJ_FLAG_HIDDEN);
        if(a==1) for(int i=1;i<=9;i++) lv_obj_clear_flag((lv_obj_t*[]){0,h1,h2,h3,h4,h5,h6,h7,h8,h9}[i],LV_OBJ_FLAG_HIDDEN);
        else if(a==7) for(int i=0;i<4;i++) lv_obj_clear_flag((lv_obj_t*[]){c1,c2,c3,c4}[i],LV_OBJ_FLAG_HIDDEN);
        else if(a==8){for(int i=0;i<4;i++) lv_obj_clear_flag((lv_obj_t*[]){g1,g2,g3,g4}[i],LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(gdot,LV_OBJ_FLAG_HIDDEN);}
        else if(a==9) for(int i=0;i<8;i++) lv_obj_clear_flag((lv_obj_t*[]){s1,s2,s3,s4,s5,s6,s7,s8}[i],LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(l_eye,LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(r_eye,LV_OBJ_FLAG_HIDDEN);
    }
}

void display_set_expr(int id){
    if(id < 0 || id > 9) return;  // public IDs 0-9 only

    ESP_LOGI(TAG_DISP, "display_set_expr called: id=%d, current_expr=%d, target_expr=%d, s_thinking=%d, s_suspicious=%d",
             id, current_expr, target_expr, s_thinking, s_suspicious);

    if(id == EXPR_THINKING){
        // Start thinking animation from current expression
        // If already thinking, ignore re-trigger to prevent restart loops
        if (s_thinking) {
            ESP_LOGI(TAG_DISP, "thinking already in progress, ignoring re-trigger");
            return;
        }
        s_thinking_base = current_expr;
        s_thinking = true;
        s_thinking_phase = 0;
        s_thinking_start = esp_timer_get_time();
        target_expr = EXPR_LEFT_IDX;
        ESP_LOGI(TAG_DISP, "thinking started from expr=%d", s_thinking_base);
    } else if(id == EXPR_SUSPICIOUS){
        // Start suspicious animation from current expression
        s_suspicious_base = current_expr;
        s_suspicious = true;
        s_suspicious_phase = 0;
        s_suspicious_start = esp_timer_get_time();
        target_expr = EXPR_SUSP_LEFT_IDX;
        ESP_LOGI(TAG_DISP, "suspicious started from expr=%d", s_suspicious_base);
    } else {
        // Immediate set: cancel any in-progress animations and apply expression directly
        s_thinking = false;
        s_suspicious = false;
        s_playing = false;
        current_expr = id;
        target_expr = id;
        apply(current_expr);
        ESP_LOGI(TAG_DISP, "set_expr id=%d (immediate)", id);
    }
}

void display_play_expr(int id){
    if(id < 0 || id > 9) return;  // public IDs 0-9 only

    // Cancel any in-progress animations
    s_thinking = false;
    s_suspicious = false;

    // thinking/suspicious have their own 5-phase animation triggered via display_set_expr
    if(id == EXPR_THINKING || id == EXPR_SUSPICIOUS){
        display_set_expr(id);
        return;
    }

    // Non-thinking/non-suspicious: play unified 3-phase animation
    // Phase 0 (0-0.5s): base → target (transition)
    // Phase 1 (0.5-1.5s): hold target
    // Phase 2 (1.5-2.0s): target → base (return)
    s_playing_base = current_expr;
    s_playing_target = id;
    s_playing = true;
    s_playing_phase = 0;
    s_playing_start = esp_timer_get_time();
    target_expr = id;
    ESP_LOGI(TAG_DISP, "play_expr id=%d: %d → %d → %d", id, s_playing_base, s_playing_target, s_playing_base);
}

int display_get_expr(void){ return current_expr; }

bool display_is_thinking(void){ return s_thinking || s_suspicious || s_think_mode; }

// Start continuous thinking mode: loops the thinking animation until display_think_stop().
// Called when agent starts processing a user message.
void display_think_start(void){
    if (s_think_mode) return;  // already in think mode
    ESP_LOGI(TAG_DISP, "display_think_start: entering think mode");
    s_think_mode = true;
    // Trigger the thinking animation if not already running
    if (!s_thinking) {
        s_thinking_base = current_expr;
        s_thinking = true;
        s_thinking_phase = 0;
        s_thinking_start = esp_timer_get_time();
        target_expr = EXPR_LEFT_IDX;
    }
}

// Stop continuous thinking mode: ends the looping animation and restores base expression.
// Called when agent sends the final response.
void display_think_stop(void){
    if (!s_think_mode && !s_thinking) return;  // not in think mode
    ESP_LOGI(TAG_DISP, "display_think_stop: exiting think mode");
    s_think_mode = false;
    s_thinking = false;
    current_expr = s_thinking_base;
    target_expr = s_thinking_base;
    apply(current_expr);
}

static void display_task(void *arg){
    // Initialize LVGL
    LVGL_Init();
    lv_disp_set_rotation(disp, LV_DISP_ROT_90);

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);

    // Create eye objects
    l_eye = lv_obj_create(scr); r_eye = lv_obj_create(scr);
    lv_obj_set_style_bg_color(l_eye, lv_color_make(0x87,0xCE,0xEB),0);
    lv_obj_set_style_bg_color(r_eye, lv_color_make(0x87,0xCE,0xEB),0);
    lv_obj_set_style_border_width(l_eye,0,0); lv_obj_set_style_border_width(r_eye,0,0);

    // ∩∩ happy lines
    hp[0][0]=(lv_point_t){55,94};hp[0][1]=(lv_point_t){67,80};
    hp[1][0]=(lv_point_t){67,80};hp[1][1]=(lv_point_t){75,77};
    hp[2][0]=(lv_point_t){75,77};hp[2][1]=(lv_point_t){83,80};
    hp[3][0]=(lv_point_t){83,80};hp[3][1]=(lv_point_t){95,94};
    hp[4][0]=(lv_point_t){140,97};hp[4][1]=(lv_point_t){180,97};
    hp[5][0]=(lv_point_t){225,94};hp[5][1]=(lv_point_t){237,80};
    hp[6][0]=(lv_point_t){237,80};hp[6][1]=(lv_point_t){245,77};
    hp[7][0]=(lv_point_t){245,77};hp[7][1]=(lv_point_t){253,80};
    hp[8][0]=(lv_point_t){253,80};hp[8][1]=(lv_point_t){265,94};
    h1=lv_line_create(scr);lv_line_set_points(h1,hp[0],2);
    h2=lv_line_create(scr);lv_line_set_points(h2,hp[1],2);
    h3=lv_line_create(scr);lv_line_set_points(h3,hp[2],2);
    h4=lv_line_create(scr);lv_line_set_points(h4,hp[3],2);
    h5=lv_line_create(scr);lv_line_set_points(h5,hp[4],2);
    h6=lv_line_create(scr);lv_line_set_points(h6,hp[5],2);
    h7=lv_line_create(scr);lv_line_set_points(h7,hp[6],2);
    h8=lv_line_create(scr);lv_line_set_points(h8,hp[7],2);
    h9=lv_line_create(scr);lv_line_set_points(h9,hp[8],2);

    // T_T cry lines
    cp[0][0]=(lv_point_t){60,60};cp[0][1]=(lv_point_t){120,60};
    cp[1][0]=(lv_point_t){90,60};cp[1][1]=(lv_point_t){90,110};
    cp[2][0]=(lv_point_t){200,60};cp[2][1]=(lv_point_t){260,60};
    cp[3][0]=(lv_point_t){230,60};cp[3][1]=(lv_point_t){230,110};
    c1=lv_line_create(scr);lv_line_set_points(c1,cp[0],2);
    c2=lv_line_create(scr);lv_line_set_points(c2,cp[1],2);
    c3=lv_line_create(scr);lv_line_set_points(c3,cp[2],2);
    c4=lv_line_create(scr);lv_line_set_points(c4,cp[3],2);

    // >.< oops lines
    gp[0][0]=(lv_point_t){55,70};gp[0][1]=(lv_point_t){82,86};
    gp[1][0]=(lv_point_t){55,102};gp[1][1]=(lv_point_t){82,86};
    gp[2][0]=(lv_point_t){265,70};gp[2][1]=(lv_point_t){238,86};
    gp[3][0]=(lv_point_t){265,102};gp[3][1]=(lv_point_t){238,86};
    g1=lv_line_create(scr);lv_line_set_points(g1,gp[0],2);
    g2=lv_line_create(scr);lv_line_set_points(g2,gp[1],2);
    g3=lv_line_create(scr);lv_line_set_points(g3,gp[2],2);
    g4=lv_line_create(scr);lv_line_set_points(g4,gp[3],2);
    gdot=lv_obj_create(scr);
    lv_obj_set_size(gdot,12,12);lv_obj_set_pos(gdot,154,80);
    lv_obj_set_style_radius(gdot,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_bg_color(gdot,lv_color_make(0x87,0xCE,0xEB),0);lv_obj_set_style_border_width(gdot,0,0);

    // ∪∪ sad lines
    sp[0][0]=(lv_point_t){55,75};sp[0][1]=(lv_point_t){67,90};
    sp[1][0]=(lv_point_t){67,90};sp[1][1]=(lv_point_t){75,93};
    sp[2][0]=(lv_point_t){75,93};sp[2][1]=(lv_point_t){83,90};
    sp[3][0]=(lv_point_t){83,90};sp[3][1]=(lv_point_t){95,75};
    sp[4][0]=(lv_point_t){225,75};sp[4][1]=(lv_point_t){237,90};
    sp[5][0]=(lv_point_t){237,90};sp[5][1]=(lv_point_t){245,93};
    sp[6][0]=(lv_point_t){245,93};sp[6][1]=(lv_point_t){253,90};
    sp[7][0]=(lv_point_t){253,90};sp[7][1]=(lv_point_t){265,75};
    s1=lv_line_create(scr);lv_line_set_points(s1,sp[0],2);
    s2=lv_line_create(scr);lv_line_set_points(s2,sp[1],2);
    s3=lv_line_create(scr);lv_line_set_points(s3,sp[2],2);
    s4=lv_line_create(scr);lv_line_set_points(s4,sp[3],2);
    s5=lv_line_create(scr);lv_line_set_points(s5,sp[4],2);
    s6=lv_line_create(scr);lv_line_set_points(s6,sp[5],2);
    s7=lv_line_create(scr);lv_line_set_points(s7,sp[6],2);
    s8=lv_line_create(scr);lv_line_set_points(s8,sp[7],2);

    // Style all 25 line objects
    lv_obj_t *ls[]={h1,h2,h3,h4,h5,h6,h7,h8,h9,c1,c2,c3,c4,g1,g2,g3,g4,s1,s2,s3,s4,s5,s6,s7,s8};
    for(int i=0;i<25;i++){
        lv_obj_set_style_line_width(ls[i],12,0);
        lv_obj_set_style_line_color(ls[i],lv_color_make(0x87,0xCE,0xEB),0);
        lv_obj_set_style_line_rounded(ls[i],true,0);
        lv_obj_add_flag(ls[i],LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_add_flag(gdot,LV_OBJ_FLAG_HIDDEN);

    apply(EXPR_NEUTRAL);

    // Main display loop
    while(1){
        // Handle thinking animation state machine
        if(s_thinking){
            int64_t elapsed_ms = (esp_timer_get_time() - s_thinking_start) / 1000;

            if(s_thinking_phase == 0 && elapsed_ms >= 500){
                // original → LEFT (0.5s transition)
                if (!s_thinking) break;
                s_thinking_phase = 1;
                current_expr = EXPR_LEFT_IDX;
                apply(current_expr);
            } else if(s_thinking_phase == 1 && elapsed_ms >= 1500){
                // hold LEFT for 1s, then transition to RIGHT
                if (!s_thinking) break;
                s_thinking_phase = 2;
                current_expr = EXPR_RIGHT_IDX;
                apply(current_expr);
            } else if(s_thinking_phase == 2 && elapsed_ms >= 2000){
                // hold RIGHT for 0.5s
                if (!s_thinking) break;
                s_thinking_phase = 3;
            } else if(s_thinking_phase == 3 && elapsed_ms >= 3000){
                // hold RIGHT for 1s, then begin return to base
                if (!s_thinking) break;
                s_thinking_phase = 4;
            } else if(s_thinking_phase == 4 && elapsed_ms >= 3500){
                // thinking animation complete (3.5s total)
                // If s_think_mode is set (LLM still processing), loop the animation
                int elapsed_s = (int)((esp_timer_get_time() - s_thinking_start) / 1000000);
                ESP_LOGI(TAG_DISP, "thinking phase4: s_think_mode=%d, s_thinking=%d, elapsed=%d s",
                         s_think_mode, s_thinking, elapsed_s);
                if (s_think_mode && s_thinking) {
                    ESP_LOGI(TAG_DISP, "thinking loop: restarting (s_think_mode=1, s_thinking=1)");
                    s_thinking_phase = 0;
                    s_thinking_start = esp_timer_get_time();
                    continue;
                }
                if (!s_thinking) break;
                s_thinking_phase = 0;
                s_thinking = false;
                ESP_LOGI(TAG_DISP, "thinking done, returning to expr=%d", s_thinking_base);
                current_expr = s_thinking_base;
                target_expr = s_thinking_base;
                apply(current_expr);
            }
        } else if(s_suspicious){
            // Handle suspicious animation state machine
            // 0-0.5s: base → susp-left (transition)
            // 0.5-1.5s: hold susp-left
            // 1.5-2s: susp-left → susp-right (transition)
            // 2-3s: hold susp-right
            // 3-3.5s: susp-right → base (transition)
            int64_t elapsed_ms = (esp_timer_get_time() - s_suspicious_start) / 1000;

            if(s_suspicious_phase == 0 && elapsed_ms >= 500){
                // transition: base → susp-left complete, enter hold
                if (!s_suspicious) break;
                s_suspicious_phase = 1;
                current_expr = EXPR_SUSP_LEFT_IDX;
                apply(current_expr);
            } else if(s_suspicious_phase == 1 && elapsed_ms >= 1500){
                // transition: susp-left → susp-right complete, enter hold
                if (!s_suspicious) break;
                s_suspicious_phase = 2;
                current_expr = EXPR_SUSP_RIGHT_IDX;
                apply(current_expr);
            } else if(s_suspicious_phase == 2 && elapsed_ms >= 2000){
                // hold susp-right, begin return to base
                if (!s_suspicious) break;
                s_suspicious_phase = 3;
            } else if(s_suspicious_phase == 3 && elapsed_ms >= 3000){
                // transition: return to base complete, begin final hold
                if (!s_suspicious) break;
                s_suspicious_phase = 4;
            } else if(s_suspicious_phase == 4 && elapsed_ms >= 3500){
                // suspicious animation complete
                if (!s_suspicious) break;
                s_suspicious_phase = 0;
                s_suspicious = false;
                ESP_LOGI(TAG_DISP, "suspicious done, returning to expr=%d", s_suspicious_base);
                current_expr = s_suspicious_base;
                target_expr = s_suspicious_base;
                apply(current_expr);
            }
        } else if(s_playing){
            // Handle unified 3-phase animation for non-thinking/non-suspicious expressions
            // Phase 0 (0-0.5s): base → target (transition)
            // Phase 1 (0.5-1.5s): hold target
            // Phase 2 (1.5-2.0s): target → base (return)
            int64_t elapsed_ms = (esp_timer_get_time() - s_playing_start) / 1000;

            if(s_playing_phase == 0 && elapsed_ms >= 500){
                // transition: base → target complete, enter hold
                if (!s_playing) break;
                s_playing_phase = 1;
                current_expr = s_playing_target;
                apply(current_expr);
            } else if(s_playing_phase == 1 && elapsed_ms >= 1500){
                // transition: target → base complete, animation done
                if (!s_playing) break;
                s_playing_phase = 2;
            } else if(s_playing_phase == 2 && elapsed_ms >= 2000){
                // playing animation complete
                if (!s_playing) break;
                s_playing_phase = 0;
                s_playing = false;
                ESP_LOGI(TAG_DISP, "playing done, returning to expr=%d", s_playing_base);
                current_expr = s_playing_base;
                target_expr = s_playing_base;
                apply(current_expr);
            }
        } else if(target_expr != current_expr){
            current_expr = target_expr;
            apply(current_expr);
        }

        int64_t loop_start = esp_timer_get_time();

        lv_timer_handler();

        // Measure active time spent in lv_timer_handler
        int64_t loop_end = esp_timer_get_time();
        s_cpu_active_us += (loop_end - loop_start);

        // Update FPS and CPU every second
        int64_t now_us = esp_timer_get_time();
        if (now_us - s_cpu_last_update >= 1000000) {
            // CPU: active time / total time in last second
            s_cpu_usage = (uint8_t)((s_cpu_active_us * 100) / 1000000);
            s_cpu_active_us = 0;
            s_cpu_last_update = now_us;
        }
        if (now_us - s_fps_last_update >= 1000000) {
            s_fps_avg = s_flush_count;
            s_flush_count = 0;
            s_fps_last_update = now_us;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void display_init(void){
    LCD_Init();
    BK_Light(50);
    RGB_Init();

    xTaskCreatePinnedToCore(display_task, "display", 8192, NULL, 5, NULL, 0);
}
