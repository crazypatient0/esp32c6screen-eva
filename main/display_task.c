#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "ST7789.h"
#include "RGB.h"
#include "LVGL_Driver.h"
#include "display_task.h"

// Frame counter for web UI FPS display
static uint32_t s_frame_count = 0;
static uint32_t s_fps_avg = 0;
static int64_t s_fps_last_update = 0;

uint32_t lvgl_fps_avg_get(void) {
    return s_fps_avg;
}

// Expression state
static int current_expr = EXPR_NEUTRAL;
static int target_expr = EXPR_NEUTRAL;

// Extern for LVGL display
extern lv_disp_t *disp;

// Expression data: w,h,x,y,r ×2
static const int16_t ex[11][10] = {
    {90,50,35,61,15, 90,50,195,61,15},     // 0 neutral
    {0,0,0,0,0, 0,0,0,0,0},                // 1 ∩∩ happy
    {90,50,35,61,15, 90,6,195,83,3},       // 2 wink
    {120,75,20,49,25, 120,75,180,49,25},   // 3 surprised
    {80,10,40,81,5, 80,10,200,81,5},       // 4 sleepy
    {75,45,15,64,15, 75,45,175,64,15},     // 5 left
    {75,45,55,64,15, 75,45,215,64,15},     // 6 right
    {70,35,35,69,12, 95,50,195,61,15},     // 7 suspicious
    {0,0,0,0,0, 0,0,0,0,0},                // 8 T_T cry
    {0,0,0,0,0, 0,0,0,0,0},                // 9 >.< oops
    {0,0,0,0,0, 0,0,0,0,0},                //10 ∪∪ sad
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
    lv_anim_t a; lv_anim_init(&a);
    lv_anim_set_var(&a,o); lv_anim_set_exec_cb(&a,cb);
    lv_anim_set_values(&a,f,t); lv_anim_set_time(&a,300);
    lv_anim_set_path_cb(&a,lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void apply(int a){
    int16_t *d=ex[a];
    int is_line=(a==1||a==8||a==9||a==10);
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
        else if(a==8) for(int i=0;i<4;i++) lv_obj_clear_flag((lv_obj_t*[]){c1,c2,c3,c4}[i],LV_OBJ_FLAG_HIDDEN);
        else if(a==9){for(int i=0;i<4;i++) lv_obj_clear_flag((lv_obj_t*[]){g1,g2,g3,g4}[i],LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(gdot,LV_OBJ_FLAG_HIDDEN);}
        else if(a==10) for(int i=0;i<8;i++) lv_obj_clear_flag((lv_obj_t*[]){s1,s2,s3,s4,s5,s6,s7,s8}[i],LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(l_eye,LV_OBJ_FLAG_HIDDEN); lv_obj_clear_flag(r_eye,LV_OBJ_FLAG_HIDDEN);
    }
}

void display_set_expr(int id){
    if(id>=0 && id<=10) target_expr=id;
}

int display_get_expr(void){ return current_expr; }

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
        if(target_expr != current_expr){
            current_expr = target_expr;
            apply(current_expr);
        }
        lv_timer_handler();
        s_frame_count++;
        // Update FPS every second
        int64_t now_us = esp_timer_get_time();
        if (now_us - s_fps_last_update >= 1000000) {
            s_fps_avg = s_frame_count;
            s_frame_count = 0;
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
