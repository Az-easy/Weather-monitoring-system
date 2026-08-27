/**
 * @file mian_UI.c
 * @brief 主界面:时间显示 / 天气显示 / 设备状态 / 界面导航
 *
 * 布局(800x480):
 *   - 顶部栏:标题 + 云端在线状态
 *   - 时间(HH:MM:SS,每秒刷新)+ 日期(星期)
 *   - 左侧:实时天气卡片(城市 / 温度 / 天气描述)
 *   - 中间:天气详细卡片(体感温度 / 相对湿度 / 能见度 / 风向)
 *   - 右侧:功能导航卡片(控制界面 / 状态界面)
 */
#include "iot_ui.h"
#include "lv_font_source_han_sans_bold.h"
#include "future_UI.h"   /* 未来10天天气界面:open_future_weather() */
#include <stdio.h>
#include <time.h>

/* ============ 底部滚动公告内容:想改内容直接改这一行 ============ */
#define TICKER_TEXT "热烈欢迎光临知贤天气安全指挥中心!本系统实时采集各地天气数据并上报华为云平台,支持远程控制LED灯与蜂鸣器,发现异常自动告警"

static lv_obj_t *g_time_label;    /* HH:MM:SS */
static lv_obj_t *g_date_label;    /* 2026年08月15日 星期六 */
static lv_timer_t *g_clock_timer;

/* 每秒刷新一次时间 / 日期 */
static void clock_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if(t == NULL) return;

    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
    lv_label_set_text(g_time_label, buf);

    static const char *wk[] = {"日", "一", "二", "三", "四", "五", "六"};
    snprintf(buf, sizeof(buf), "%04d年%02d月%02d日 星期%s",
             t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, wk[t->tm_wday]);
    lv_label_set_text(g_date_label, buf);
}

/* 导航按钮回调 */
static void nav_btn_control_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) iot_ui_show_control();
}

static void nav_btn_state_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) iot_ui_show_state();
}

/* 天气卡片「查天气」按钮:进入城市选择界面 */
static void weather_btn_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) iot_ui_show_weather();
}

/* 实时天气卡片右上角「未来10天天气」按钮:查询未来10天并切换到该界面(future_UI.c) */
static void future_btn_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) open_future_weather();
}

/* 在卡片内创建一个整行的导航按钮(样式和「查天气」按键一致:深色底 + 青色字) */
static lv_obj_t *make_nav_btn(lv_obj_t *parent, lv_coord_t y, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 208, 76);
    lv_obj_set_pos(btn, 16, y);
    lv_obj_set_style_radius(btn, 22, 0);                /* 和「查天气」按键同款圆角 */
    lv_obj_set_style_bg_color(btn, IOT_COL_OFF_BG, 0);  /* 深色底 */
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &chinese_ziku, 0);
    lv_obj_set_style_text_color(lbl, IOT_COL_ACC, 0);   /* 青色强调字 */
    lv_obj_center(lbl);
    return btn;
}

/* 底部滚动公告栏:文字宽度超过标签宽度就自动循环滚动。
 * 速度由 LV_STYLE_ANIM_SPEED 控制(px/秒,越大越快),内容在顶部 TICKER_TEXT 改。 */
static void make_ticker(lv_obj_t *scr)
{
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_size(bar, 800, 44);
    lv_obj_set_pos(bar, 0, 436);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x101c33), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 2, 0);
    lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
    lv_obj_set_style_border_color(bar, lv_color_hex(0x2a3f6b), 0);

    lv_obj_t *lbl = lv_label_create(bar);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(lbl, TICKER_TEXT);
    lv_obj_set_style_text_font(lbl, &chinese_ziku, 0);
    lv_obj_set_style_text_color(lbl, IOT_COL_ACC, 0);
    lv_obj_set_width(lbl, 760);              /* 宽度小于文字实际宽度才滚动 */
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 12, 0);
    lv_obj_set_style_anim_speed(lbl, 45, 0); /* 滚动速度(px/秒),数字越大越快 */
}

void mian_UI(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    g_scr_home = scr;
    iot_ui_screen_bg(scr);
    iot_ui_make_header(scr, "知贤天气安全指挥中心", &iot_refs_home);

    /* -------- 时间 / 日期 -------- */
    g_time_label = lv_label_create(scr);
    lv_label_set_text(g_time_label, "--:--:--");
    lv_obj_set_style_text_font(g_time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(g_time_label, lv_color_hex(0xffffff), 0);
    lv_obj_align(g_time_label, LV_ALIGN_TOP_MID, 0, 78);

    g_date_label = lv_label_create(scr);
    lv_label_set_text(g_date_label, "----年--月--日 星期-");
    lv_obj_set_style_text_font(g_date_label, &chinese_ziku, 0);
    lv_obj_set_style_text_color(g_date_label, IOT_COL_TXT2, 0);
    lv_obj_align(g_date_label, LV_ALIGN_TOP_MID, 0, 166);

    /* -------- 天气卡片 -------- */
    lv_obj_t *w_card = iot_ui_make_card(scr, 20, 210, 240, 220);
    iot_ui_card_title(w_card, "实时天气");

    iot_refs_home.city_text = lv_label_create(w_card);
    lv_label_set_text(iot_refs_home.city_text, "--");
    lv_obj_set_style_text_font(iot_refs_home.city_text, &chinese_ziku, 0);
    lv_obj_set_style_text_color(iot_refs_home.city_text, IOT_COL_TXT, 0);
    lv_obj_set_pos(iot_refs_home.city_text, 16, 50);

    iot_refs_home.temp_text = lv_label_create(w_card);
    lv_label_set_text(iot_refs_home.temp_text, "--°C");
    lv_obj_set_style_text_font(iot_refs_home.temp_text, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(iot_refs_home.temp_text, IOT_COL_ACC, 0);
    lv_obj_set_pos(iot_refs_home.temp_text, 16, 80);

    iot_refs_home.weather_text = lv_label_create(w_card);
    lv_label_set_text(iot_refs_home.weather_text, "--");
    lv_obj_set_style_text_font(iot_refs_home.weather_text, &chinese_ziku, 0);
    lv_obj_set_style_text_color(iot_refs_home.weather_text, IOT_COL_TXT2, 0);
    lv_obj_set_pos(iot_refs_home.weather_text, 16, 148);

    /* 「查天气」按钮:进入城市选择界面查询其他地点天气 */
    lv_obj_t *w_btn = lv_btn_create(w_card);
    lv_obj_set_size(w_btn, 208, 44);
    lv_obj_set_pos(w_btn, 16, 172);
    lv_obj_set_style_radius(w_btn, 22, 0);
    lv_obj_set_style_bg_color(w_btn, IOT_COL_OFF_BG, 0);
    lv_obj_add_event_cb(w_btn, weather_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *w_lbl = lv_label_create(w_btn);
    lv_label_set_text(w_lbl, "查天气");
    lv_obj_set_style_text_font(w_lbl, &chinese_ziku, 0);
    lv_obj_set_style_text_color(w_lbl, IOT_COL_ACC, 0);
    lv_obj_center(w_lbl);

    /* 「未来10天天气」按钮:实时天气卡片右上角(70x70 方形,文字分两行) */
    lv_obj_t *future_btn = lv_btn_create(w_card);
    lv_obj_set_size(future_btn, 70, 70);
    lv_obj_set_style_radius(future_btn, 8, 0);
    lv_obj_set_style_bg_color(future_btn, IOT_COL_OFF_BG, 0);
    lv_obj_set_style_pad_all(future_btn, 0, 0);
    lv_obj_add_event_cb(future_btn, future_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(future_btn, LV_ALIGN_TOP_RIGHT, -8, 8);

    /* 70px 宽放不下"未来10天天气"(约96px),拆成两行"10天 / 天气" */
    lv_obj_t *future_lbl = lv_label_create(future_btn);
    lv_label_set_text(future_lbl, "10天\n天气");
    lv_label_set_long_mode(future_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(future_lbl, &chinese_ziku, 0);
    lv_obj_set_style_text_color(future_lbl, IOT_COL_ACC, 0);
    lv_obj_set_style_text_align(future_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(future_lbl, 70);
    lv_obj_center(future_lbl);

    /* -------- 天气详细卡片(体感温度 / 湿度 / 能见度 / 风向) -------- */
    /* 数据来源:forecast.c 查完填进 g_weather → iot_ui_set_weather() → iot_state。
     * 查询前都是未知值(<0),界面显示 "--"。 */
    lv_obj_t *s_card = iot_ui_make_card(scr, 280, 210, 240, 220);
    iot_ui_card_title(s_card, "天气详细");
    iot_ui_make_status_row(s_card, 16, 52,  "体感温度", NULL, &iot_refs_home.feels_like_text);
    iot_ui_make_status_row(s_card, 16, 92,  "相对湿度", NULL, &iot_refs_home.humidity_text);
    iot_ui_make_status_row(s_card, 16, 132, "能见度",   NULL, &iot_refs_home.visibility_text);
    iot_ui_make_status_row(s_card, 16, 172, "风向",     NULL, &iot_refs_home.wind_text);

    /* 前三个数值标签改用数字字体 montserrat_16:中文字库 chinese_ziku 不含 "°" 符号,
     * 体感温度后面的 "°C" 会显示成空白。montserrat_16 自带 °/%/km/数字。
     * 风向标签保持中文字库 chinese_ziku:它的值是中文方向(如"东南风")。 */
    lv_obj_set_style_text_font(iot_refs_home.feels_like_text,   &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_font(iot_refs_home.humidity_text,     &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_font(iot_refs_home.visibility_text,   &lv_font_montserrat_16, 0);

    /* -------- 功能导航卡片 -------- */
    lv_obj_t *nav = iot_ui_make_card(scr, 540, 210, 240, 220);
    iot_ui_card_title(nav, "功能导航");
    make_nav_btn(nav, 50, "控制界面", nav_btn_control_cb);
    make_nav_btn(nav, 132, "状态界面", nav_btn_state_cb);

    /* -------- 底部滚动公告(内容在 TICKER_TEXT 改) -------- */
    make_ticker(scr);

    /* -------- 时钟定时器(每秒刷新一次) -------- */
    g_clock_timer = lv_timer_create(clock_timer_cb, 1000, NULL);
}
