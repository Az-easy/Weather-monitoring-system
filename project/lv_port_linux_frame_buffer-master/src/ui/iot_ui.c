/**
 * @file iot_ui.c
 * @brief IOT 数据监控中心 —— 共享状态 / 通用控件 / 界面注册与切换
 *
 * 说明:
 *   1. 工程共 4 个界面,各自独立创建,通过 lv_disp_load_scr() 切换:
 *        主界面 mian_UI.c / 控制界面 control_UI.c
 *        状态界面 state_UI.c / 天气查询界面 weather_UI.c
 *   2. "注册界面" = 在 iot_ui_init() 里调用各界面的构建函数
 *      (mian_UI()/control_UI()/state_UI()/weather_UI()),
 *      它们会创建屏幕对象存入 g_scr_* 全局变量、把动态控件存入 iot_refs_*。
 *      之后切换界面只需调用 iot_ui_show_*( ) 用 lv_disp_load_scr() 显示。
 *   3. 界面显示的数据都从 iot_state 读取,外部模块(HTTP 天气、TCP 云通信、GPIO)
 *      调用 iot_ui_set_* 系列接口即可更新数据并自动刷新界面。
 */
#include "iot_ui.h"
#include "lv_font_source_han_sans_bold.h"
#include "led_beep.h"
#include "forecast.h"
#include "future_UI.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/*----------------------- 全局状态 -----------------------*/
/* 注意:默认值只放"真实状态",不放编造的数据。
 * 温度/天气在开机后由后台线程自动查询广州的实时天气(见下方"开机自动查询"一节)填上;
 * 在此之前 temperature 是 IOT_TEMP_UNKNOWN(界面显示 "--"),weather_desc 显示"获取中"。 */
iot_ui_state_t iot_state = {
    .led_on          = false,
    .buzzer_on       = false,
    .cloud_connected = false,
    .temperature     = IOT_TEMP_UNKNOWN, /* 未知 → 界面显示 "--°C" */
    .weather_desc    = "获取中",          /* 真实状态,不是编造的天气 */
    .city            = "广州",            /* 开机默认自动查询的城市 */
    .feels_like      = IOT_TEMP_UNKNOWN, /* 未知 → "--°C" */
    .humidity        = IOT_VAL_UNKNOWN,  /* 未知 → "--" */
    .visibility      = IOT_VAL_UNKNOWN,  /* 未知 → "--" */
    .wind_direction  = "",               /* 未知 → 只显示角度 */
    .wind_degree     = IOT_VAL_UNKNOWN,  /* 未知 → "--" */
    .last_cmd        = "无",
    .last_upload     = "--:--:--",
    .upload_interval = 5,
};

/*----------------------- 各界面全局对象(登记表) -----------------------*/
/* 每个界面对应两个"登记"用的全局变量:
 *   g_scr_xxx   —— 界面屏幕对象。由构建函数(weather_UI()/mian_UI()...)创建并赋值,
 *                  界面切换时 lv_disp_load_scr(g_scr_xxx) 把它显示出来。
 *   iot_refs_xxx —— 界面上需要随 iot_state 刷新的控件指针集合。
 *                   构建函数把要更新的 label/按钮 指针填进来,
 *                   iot_ui_refresh() 遍历它统一刷新。
 * 这些变量"定义"在这里(占内存),"声明"在 iot_ui.h(供其他文件使用)。 */
lv_obj_t *g_scr_home    = NULL;   /* 主界面 */
lv_obj_t *g_scr_control = NULL;   /* 控制界面 */
lv_obj_t *g_scr_state   = NULL;   /* 状态界面 */
lv_obj_t *g_scr_weather = NULL;   /* 天气查询界面(weather_UI.c 创建) */
lv_obj_t *g_scr_future  = NULL;   /* 未来10天天气界面(future_UI.c 创建) */

iot_ui_refs_t iot_refs_home;      /* 主界面的动态控件 */
iot_ui_refs_t iot_refs_control;   /* 控制界面的动态控件 */
iot_ui_refs_t iot_refs_state;     /* 状态界面的动态控件 */
iot_ui_refs_t iot_refs_weather;   /* 天气查询界面的动态控件 */
iot_ui_refs_t iot_refs_future;    /* 未来10天天气界面的动态控件 */

/*----------------------- 通用控件助手 -----------------------*/
void iot_ui_screen_bg(lv_obj_t *scr)
{
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0a1430), 0);
    lv_obj_set_style_bg_grad_color(scr, lv_color_hex(0x16305c), 0);
    lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_main_stop(scr, 0, 0);
    lv_obj_set_style_bg_grad_stop(scr, 255, 0);
}

lv_obj_t *iot_ui_make_header(lv_obj_t *parent, const char *title, iot_ui_refs_t *refs)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_set_size(bar, 800, 64);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x101c33), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 2, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_color(bar, lv_color_hex(0x2a3f6b), 0);

    lv_obj_t *ti = lv_label_create(bar);
    lv_label_set_text(ti, title);
    lv_obj_set_style_text_font(ti, &chinese_ziku, 0);
    lv_obj_set_style_text_color(ti, lv_color_hex(0xffffff), 0);
    lv_obj_align(ti, LV_ALIGN_LEFT_MID, 20, 0);

    if(refs != NULL) {
        refs->cloud_text = lv_label_create(bar);
        lv_label_set_text(refs->cloud_text, "云端离线");
        lv_obj_set_style_text_font(refs->cloud_text, &chinese_ziku, 0);
        lv_obj_set_style_text_color(refs->cloud_text, IOT_COL_TXT2, 0);
        lv_obj_align(refs->cloud_text, LV_ALIGN_RIGHT_MID, -20, 0);

        refs->cloud_dot = lv_obj_create(bar);
        lv_obj_set_size(refs->cloud_dot, 14, 14);
        lv_obj_clear_flag(refs->cloud_dot, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_radius(refs->cloud_dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(refs->cloud_dot, IOT_COL_RED, 0);
        lv_obj_set_style_bg_opa(refs->cloud_dot, LV_OPA_COVER, 0);
        lv_obj_align_to(refs->cloud_dot, refs->cloud_text, LV_ALIGN_OUT_LEFT_MID, -10, 0);
    }
    return bar;
}

lv_obj_t *iot_ui_make_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_20, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x2a3f6b), 0);
    return card;
}

void iot_ui_card_title(lv_obj_t *card, const char *title)
{
    lv_obj_t *t = lv_label_create(card);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_font(t, &chinese_ziku, 0);
    lv_obj_set_style_text_color(t, IOT_COL_ACC, 0);
    lv_obj_set_pos(t, 16, 14);
}

lv_obj_t *iot_ui_make_dot(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_color_t color)
{
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_set_size(dot, 16, 16);
    lv_obj_set_pos(dot, x, y);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 2, 0);
    lv_obj_set_style_border_color(dot, lv_color_hex(0x0a1430), 0);
    return dot;
}

void iot_ui_make_status_row(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                            const char *name, lv_obj_t **dot_ref, lv_obj_t **text_ref)
{
    lv_obj_t *name_lbl = lv_label_create(parent);
    lv_label_set_text(name_lbl, name);
    lv_obj_set_style_text_font(name_lbl, &chinese_ziku, 0);
    lv_obj_set_style_text_color(name_lbl, IOT_COL_TXT2, 0);
    lv_obj_set_pos(name_lbl, x, y);

    lv_obj_t *val = lv_label_create(parent);
    lv_obj_set_style_text_font(val, &chinese_ziku, 0);
    lv_obj_set_style_text_color(val, IOT_COL_TXT, 0);
    /* 值标签在 x+170:右侧留出空间,避免较长的值(如 "30 km")被卡片右边缘裁掉。
     * 原来 x+180 时 "30 km" 的 "m" 会被裁一半;也比状态灯(dot 在 x+150,宽 16)靠右 4px。 */
    lv_obj_set_pos(val, x + 170, y);

    if(text_ref) *text_ref = val;

    /* 只有调用方需要状态灯时才创建 */
    if(dot_ref) {
        lv_obj_t *dot = iot_ui_make_dot(parent, x + 150, y + 2, IOT_COL_OFF);
        *dot_ref = dot;
    }
}

static void back_btn_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        iot_ui_show_home();
    }
}

lv_obj_t *iot_ui_make_back_btn(lv_obj_t *parent, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 200, 56);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_radius(btn, 28, 0);
    lv_obj_set_style_bg_color(btn, IOT_COL_OFF_BG, 0);  /* 深色底 + 青色字,和天气卡片按键同款 */
    lv_obj_add_event_cb(btn, back_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, "返回首页");
    lv_obj_set_style_text_font(lbl, &chinese_ziku, 0);
    lv_obj_set_style_text_color(lbl, IOT_COL_ACC, 0);
    lv_obj_center(lbl);
    return btn;
}

/*----------------------- 状态刷新 -----------------------*/
static void iot_ui_apply_refs(iot_ui_refs_t *r)
{
    if(r == NULL) return;

    /* 云端(顶栏 + 状态界面连接行) */
    if(r->cloud_dot)  lv_obj_set_style_bg_color(r->cloud_dot,  iot_state.cloud_connected ? IOT_COL_ON : IOT_COL_RED, 0);
    if(r->cloud_text) lv_label_set_text(r->cloud_text, iot_state.cloud_connected ? "云端在线" : "云端离线");
    if(r->conn_dot)   lv_obj_set_style_bg_color(r->conn_dot,   iot_state.cloud_connected ? IOT_COL_ON : IOT_COL_RED, 0);
    if(r->conn_text)  lv_label_set_text(r->conn_text, iot_state.cloud_connected ? "在线" : "离线");

    /* LED / 蜂鸣器 */
    if(r->led_dot)    lv_obj_set_style_bg_color(r->led_dot,    iot_state.led_on    ? IOT_COL_ON : IOT_COL_OFF, 0);
    if(r->led_text)   lv_label_set_text(r->led_text,   iot_state.led_on    ? "开" : "关");
    if(r->buzzer_dot) lv_obj_set_style_bg_color(r->buzzer_dot, iot_state.buzzer_on ? IOT_COL_ON : IOT_COL_OFF, 0);
    if(r->buzzer_text)lv_label_set_text(r->buzzer_text, iot_state.buzzer_on ? "开" : "关");

    /* 天气 */
    if(r->city_text)    lv_label_set_text(r->city_text, iot_state.city);
    if(r->weather_text) lv_label_set_text(r->weather_text, iot_state.weather_desc);
    if(r->temp_text) {
        char buf[16];
        if(iot_state.temperature == IOT_TEMP_UNKNOWN) {
            lv_label_set_text(r->temp_text, "--°C");  /* 还没查到真实温度,不显示编造的值 */
        } else {
            snprintf(buf, sizeof(buf), "%d°C", iot_state.temperature);
            lv_label_set_text(r->temp_text, buf);
        }
    }

    /* 天气详细(主界面"天气详细"卡片,<0 = 未知 → "--") */
    if(r->feels_like_text) {
        char buf[16];
        if(iot_state.feels_like < 0) lv_label_set_text(r->feels_like_text, "--°C");
        else { snprintf(buf, sizeof(buf), "%d°C", iot_state.feels_like); lv_label_set_text(r->feels_like_text, buf); }
    }
    if(r->humidity_text) {
        char buf[16];
        if(iot_state.humidity < 0) lv_label_set_text(r->humidity_text, "--");
        else { snprintf(buf, sizeof(buf), "%d%%", iot_state.humidity); lv_label_set_text(r->humidity_text, buf); }
    }
    if(r->visibility_text) {
        char buf[16];
        if(iot_state.visibility < 0) lv_label_set_text(r->visibility_text, "--");
        else { snprintf(buf, sizeof(buf), "%d km", iot_state.visibility); lv_label_set_text(r->visibility_text, buf); }
    }
    if(r->wind_text) {
        if(iot_state.wind_direction[0] != '\0') {
            /* 有方向文字(如 "东南风")优先显示,读起来直观。
             * 该标签用中文字库 chinese_ziku 渲染,所以能显示中文。 */
            lv_label_set_text(r->wind_text, iot_state.wind_direction);
        } else if(iot_state.wind_degree >= 0) {
            /* 只有角度:显示数字(中文字库没有 "°",只显示角度值)。
             * 接口一般都会给方向文字,这一分支很少走到。 */
            char buf[16];
            snprintf(buf, sizeof(buf), "%d", iot_state.wind_degree);
            lv_label_set_text(r->wind_text, buf);
        } else {
            lv_label_set_text(r->wind_text, "--");
        }
    }

    /* 数据上报 */
    if(r->upload_text) lv_label_set_text(r->upload_text, iot_state.last_upload[0] == '-' ? "未上报" : "已上报");
    if(r->upload_time) lv_label_set_text(r->upload_time, iot_state.last_upload);
    if(r->upload_interval) {
        char buf[32];
        snprintf(buf, sizeof(buf), "每 %u 秒", iot_state.upload_interval);
        lv_label_set_text(r->upload_interval, buf);
    }
    if(r->cmd_text) lv_label_set_text(r->cmd_text, iot_state.last_cmd);

    /* 控制界面按钮 */
    if(r->led_btn)     lv_obj_set_style_bg_color(r->led_btn,     iot_state.led_on    ? IOT_COL_ON_BG : IOT_COL_OFF_BG, 0);
    if(r->led_btn_dot) lv_obj_set_style_bg_color(r->led_btn_dot, iot_state.led_on    ? IOT_COL_ON : IOT_COL_OFF, 0);
    if(r->led_btn_text)   lv_label_set_text(r->led_btn_text,   iot_state.led_on    ? "当前:已开启" : "当前:已关闭");
    if(r->buzzer_btn)     lv_obj_set_style_bg_color(r->buzzer_btn,     iot_state.buzzer_on ? IOT_COL_ON_BG : IOT_COL_OFF_BG, 0);
    if(r->buzzer_btn_dot) lv_obj_set_style_bg_color(r->buzzer_btn_dot, iot_state.buzzer_on ? IOT_COL_ON : IOT_COL_OFF, 0);
    if(r->buzzer_btn_text)lv_label_set_text(r->buzzer_btn_text, iot_state.buzzer_on ? "当前:已开启" : "当前:已关闭");
}

/* 用 iot_state 当前值刷新全部 4 个界面的动态控件。
 * 哪个界面当前可见就更新哪个;不可见的也一起同步,保证切过去时状态是新的。 */
void iot_ui_refresh(void)
{
    iot_ui_apply_refs(&iot_refs_home);
    iot_ui_apply_refs(&iot_refs_control);
    iot_ui_apply_refs(&iot_refs_state);
    iot_ui_apply_refs(&iot_refs_weather);   /* 天气查询界面 */
    iot_ui_apply_refs(&iot_refs_future);    /* 未来10天天气界面 */
}

/*----------------------- 数据更新入口(外部模块对接点) -----------------------*/
void iot_ui_set_led(bool on)
{
    iot_state.led_on = on;
    printf("[IOT] LED: %s\n", on ? "ON" : "OFF");
    if(led_set(on) != 0) {
        printf("[IOT] LED 控制失败(请确认 /dev/Led 驱动已加载)\n");
    }
    iot_ui_refresh();
}

void iot_ui_set_buzzer(bool on)
{
    iot_state.buzzer_on = on;
    printf("[IOT] BUZZER: %s\n", on ? "ON" : "OFF");
    if(beep_set(on) != 0) {
        printf("[IOT] 蜂鸣器控制失败(请确认 /dev/beep 驱动已加载)\n");
    }
    iot_ui_refresh();
}

void iot_ui_set_cloud(bool connected)
{
    iot_state.cloud_connected = connected;
    printf("[IOT] CLOUD: %s\n", connected ? "ONLINE" : "OFFLINE");
    iot_ui_refresh();
}

void iot_ui_set_weather(const char *city, int temp, const char *desc,
                        int feels_like, int humidity, int visibility,
                        const char *wind_direction, int wind_degree)
{
    if(city) {
        strncpy(iot_state.city, city, sizeof(iot_state.city) - 1);
        iot_state.city[sizeof(iot_state.city) - 1] = '\0';
    }
    if(desc) {
        strncpy(iot_state.weather_desc, desc, sizeof(iot_state.weather_desc) - 1);
        iot_state.weather_desc[sizeof(iot_state.weather_desc) - 1] = '\0';
    }
    iot_state.temperature = (int16_t)temp;

    /* 天气详细(负值 = 未知,界面显示 "--") */
    iot_state.feels_like  = (int16_t)feels_like;
    iot_state.humidity    = (int16_t)humidity;
    iot_state.visibility  = (int16_t)visibility;
    iot_state.wind_degree = (int16_t)wind_degree;
    iot_state.wind_direction[0] = '\0';
    if(wind_direction) {
        strncpy(iot_state.wind_direction, wind_direction, sizeof(iot_state.wind_direction) - 1);
        iot_state.wind_direction[sizeof(iot_state.wind_direction) - 1] = '\0';
    }

    iot_ui_refresh();
}

void iot_ui_on_cmd(const char *cmd)
{
    if(cmd) {
        strncpy(iot_state.last_cmd, cmd, sizeof(iot_state.last_cmd) - 1);
        iot_state.last_cmd[sizeof(iot_state.last_cmd) - 1] = '\0';
    }
    iot_ui_refresh();
}

void iot_ui_note_upload(void)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if(t) {
        snprintf(iot_state.last_upload, sizeof(iot_state.last_upload),
                 "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    }
    iot_ui_refresh();
}

/*----------------------- 界面切换 / 初始化 -----------------------*/
void iot_ui_show_home(void)
{
    lv_disp_load_scr(g_scr_home);
    iot_ui_refresh();
}

void iot_ui_show_control(void)
{
    lv_disp_load_scr(g_scr_control);
    iot_ui_refresh();
}

void iot_ui_show_state(void)
{
    lv_disp_load_scr(g_scr_state);
    iot_ui_refresh();
}

/* 切换到天气查询界面。
 * 屏幕对象早就由 weather_UI() 建好并存入 g_scr_weather,
 * 这里只用 lv_disp_load_scr() 把它设成当前显示屏幕即可。 */
void iot_ui_show_weather(void)
{
    lv_disp_load_scr(g_scr_weather);
    iot_ui_refresh();
}

void iot_ui_show_future(void)
{
    lv_disp_load_scr(g_scr_future);
    iot_ui_refresh();
}

/* ------------------- 界面注册(初始化)入口 -------------------
 * main() 里调用一次,做四件事:
 *   1) led_beep_init()      打开 LED / 蜂鸣器设备
 *   2) mian_UI()/control_UI()/state_UI()/weather_UI()
 *      创建 4 个界面并把屏幕对象存进 g_scr_*、动态控件存进 iot_refs_*
 *      —— 这就是"注册界面":界面被创建、被登记,之后可以随时切换
 *   3) forecast("广州")     开机自动查一遍广州实时天气,填进 iot_state
 *      (同步阻塞,网络没就绪时会卡几秒;失败就显示"获取失败",不显示编造的值)
 *   4) iot_ui_refresh() + lv_disp_load_scr(g_scr_home)
 *      用真实数据刷新一遍,再显示主界面 */
void iot_ui_init(void)
{
    /* ① 打开 LED / 蜂鸣器设备(驱动未加载时打印错误,不影响界面运行) */
    led_beep_init();

    /* ② 注册(创建)4 个界面 */
    mian_UI();        /* 主界面      -> g_scr_home    (mian_UI.c)    */
    control_UI();     /* 控制界面    -> g_scr_control (control_UI.c) */
    state_UI();       /* 状态界面    -> g_scr_state   (state_UI.c)   */
    weather_UI();     /* 天气查询界面 -> g_scr_weather (weather_UI.c) ← 天气界面在这里注册 */
    future_UI();      /* 未来10天天气界面 -> g_scr_future (future_UI.c) */

    /* ③ 开机自动查询一遍广州的实时天气。
     * 用可变缓冲区 buf(forecast 内部用 strtok 会改字符串,不能传字符串常量)。
     * 成功就把真实值填进 iot_state;失败保持温度 "--"、天气显示"获取失败"。 */
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "%s", "广州");
        if(forecast(buf) == 0) {
            /* 查询成功:把天气结构体里的真实数据填进 iot_state */
            iot_ui_set_weather(g_weather.city, g_weather.temp, g_weather.text,
                               g_weather.feels_like, g_weather.humidity,
                               g_weather.visibility,
                               g_weather.wind_direction, g_weather.wind_degree);
        } else {
            iot_ui_set_weather(NULL, IOT_TEMP_UNKNOWN, "获取失败",
                               IOT_TEMP_UNKNOWN, IOT_VAL_UNKNOWN,
                               IOT_VAL_UNKNOWN, NULL, IOT_VAL_UNKNOWN);
        }
    }

    /* ④ 用 iot_state 当前值刷新一遍所有界面,再显示主界面 */
    iot_ui_refresh();
    lv_disp_load_scr(g_scr_home);
}
