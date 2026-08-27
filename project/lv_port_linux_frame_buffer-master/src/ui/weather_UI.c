/**
 * @file weather_UI.c
 * @brief 天气查询界面:选择城市 → forecast() 查询 → 更新主界面实时天气
 *
 * 从主界面天气卡片「查天气」按钮进入。
 * 点击城市按钮后调用 forecast(city) 同步查询(期间界面短暂阻塞),
 * 查询成功则把结果显示到主界面实时天气卡片并返回主界面。
 */
#include "iot_ui.h"
#include "lv_font_source_han_sans_bold.h"
#include "forecast.h"
#include <stdio.h>
#include <string.h>

static lv_obj_t *g_status_label;   /* 查询状态提示 */

/* 可查询的城市列表 */
static const char *city_list[] = {
    "上饶", "广州", "北京", "上海",
    "成都", "杭州", "武汉", "西安",
};

static void city_btn_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    const char *city = (const char *)lv_event_get_user_data(e);

    if(g_status_label) {
        lv_label_set_text_fmt(g_status_label, "正在查询 %s 天气...", city);
        lv_refr_now(NULL);   /* 立即重绘,先显示"查询中",再执行阻塞的网络查询 */
    }

    /* forecast() 内部用 strtok 修改入参,必须传可变缓冲区 */
    char buf[32];
    snprintf(buf, sizeof(buf), "%s", city);

    if(forecast(buf) == 0) {
        /* 查询成功:把天气结构体 g_weather 的数据(含体感/湿度/能见度/风向)更新到主界面 */
        iot_ui_set_weather(g_weather.city, g_weather.temp, g_weather.text,
                           g_weather.feels_like, g_weather.humidity,
                           g_weather.visibility,
                           g_weather.wind_direction, g_weather.wind_degree);
        iot_ui_show_home();
    } else {
        if(g_status_label) lv_label_set_text(g_status_label, "查询失败,请检查网络后重试");
    }
}

/* weather_UI():天气查询界面的"注册(创建)"入口。
 * 由 iot_ui_init() 在启动时调用一次,作用是:
 *   1. lv_obj_create(NULL) 建一个新的屏幕对象,存入全局 g_scr_weather
 *      (界面切换时 lv_disp_load_scr(g_scr_weather) 就能显示它);
 *   2. 在屏幕上摆放控件,并把需要动态刷新的控件指针填进全局 iot_refs_weather;
 *   3. 之后从主界面点「查天气」按钮 → iot_ui_show_weather() 即可切到这个界面。 */
void weather_UI(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);   /* 创建独立屏幕(不挂到任何父对象) */
    g_scr_weather = scr;                   /* 登记到全局,供切换使用 */
    iot_ui_screen_bg(scr);
    iot_ui_make_header(scr, "天气查询", &iot_refs_weather);

    /* 城市选择卡片 */
    lv_obj_t *card = iot_ui_make_card(scr, 20, 80, 760, 290);
    iot_ui_card_title(card, "选择城市,查询实时天气");

    const lv_coord_t bw = 165, bh = 78, gap = 18;
    const lv_coord_t x0 = 25, y0 = 58;
    int i;
    for(i = 0; i < 8; i++) {
        int col = i % 4;
        int row = i / 4;

        lv_obj_t *btn = lv_btn_create(card);
        lv_obj_set_size(btn, bw, bh);
        lv_obj_set_pos(btn, x0 + col * (bw + gap), y0 + row * (bh + gap));
        lv_obj_set_style_radius(btn, 12, 0);
        lv_obj_set_style_bg_color(btn, IOT_COL_ON_BG, 0);
        lv_obj_add_event_cb(btn, city_btn_cb, LV_EVENT_CLICKED, (void *)city_list[i]);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, city_list[i]);
        lv_obj_set_style_text_font(lbl, &chinese_ziku, 0);
        lv_obj_set_style_text_color(lbl, IOT_COL_TXT, 0);
        lv_obj_center(lbl);
    }

    /* 查询状态提示 */
    g_status_label = lv_label_create(card);
    lv_label_set_text(g_status_label, "请选择要查询的城市");
    lv_obj_set_style_text_font(g_status_label, &chinese_ziku, 0);
    lv_obj_set_style_text_color(g_status_label, IOT_COL_TXT2, 0);
    lv_obj_set_pos(g_status_label, 25, 252);

    /* 返回主界面 */
    iot_ui_make_back_btn(scr, 300, 392);
}
