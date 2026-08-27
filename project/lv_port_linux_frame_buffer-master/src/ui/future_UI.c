/**
 * @file future_UI.c
 * @brief 未来10天天气界面:查询 forecast.c 的 forecast_daily() 并列表显示
 *
 * 从主界面实时天气卡片右上角「未来10天天气」按钮进入。
 * open_future_weather() 用当前城市(iot_state.city,默认广州)同步查询未来10天
 * (最高温 / 最低温 / 降水概率 / 白天天气),刷新 10 行列表后切到本界面。
 */
#include "iot_ui.h"
#include "lv_font_source_han_sans_bold.h"
#include "forecast.h"
#include "future_UI.h"
#include <stdio.h>
#include <string.h>

/* 10 行列表的控件指针(索引 0..9) */
static lv_obj_t *g_day_date[FORECAST_MAX_DAYS];
static lv_obj_t *g_day_text[FORECAST_MAX_DAYS];
static lv_obj_t *g_day_high[FORECAST_MAX_DAYS];
static lv_obj_t *g_day_low[FORECAST_MAX_DAYS];
static lv_obj_t *g_day_precip[FORECAST_MAX_DAYS];

/* 把 g_forecast10 刷到 10 行上;没查到的行 / 未知字段显示 "--" */
static void future_ui_refresh(void)
{
    char buf[32];
    for(int i = 0; i < FORECAST_MAX_DAYS; i++) {
        int have = (i < g_forecast10.count);

        /* 日期:只显示"月-日",截掉开头的 "YYYY-" */
        if(have && g_forecast10.day[i].date[0]) {
            const char *d = g_forecast10.day[i].date;
            if(strlen(d) > 5) d += 5;
            lv_label_set_text(g_day_date[i], d);
        } else {
            lv_label_set_text(g_day_date[i], "--");
        }

        if(have && g_forecast10.day[i].text_day[0])
            lv_label_set_text(g_day_text[i], g_forecast10.day[i].text_day);
        else
            lv_label_set_text(g_day_text[i], "--");

        int high   = have ? g_forecast10.day[i].high   : -1;
        int low    = have ? g_forecast10.day[i].low    : -1;
        int precip = have ? g_forecast10.day[i].precip : -1;

        if(high >= 0)     { snprintf(buf, sizeof(buf), "%d°C", high);   lv_label_set_text(g_day_high[i], buf); }
        else              lv_label_set_text(g_day_high[i], "--");
        if(low >= 0)      { snprintf(buf, sizeof(buf), "%d°C", low);    lv_label_set_text(g_day_low[i], buf); }
        else              lv_label_set_text(g_day_low[i], "--");
        if(precip >= 0)   { snprintf(buf, sizeof(buf), "%d%%", precip); lv_label_set_text(g_day_precip[i], buf); }
        else              lv_label_set_text(g_day_precip[i], "--");
    }
}

void future_UI(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);   /* 创建独立屏幕(不挂到任何父对象) */
    g_scr_future = scr;                    /* 登记到全局,供 iot_ui_show_future() 切换 */
    iot_ui_screen_bg(scr);
    iot_ui_make_header(scr, "未来10天天气", &iot_refs_future);

    /* 列表卡片 */
    lv_obj_t *card = iot_ui_make_card(scr, 20, 76, 760, 336);
    iot_ui_card_title(card, "未来10天天气预报");

    /* 列标题:日期 / 天气 / 最高温 / 最低温 / 降水概率 */
    static const char *col_name[] = { "日期", "天气", "最高温", "最低温", "降水概率" };
    static const lv_coord_t col_x[] = { 24, 140, 300, 430, 570 };
    for(int c = 0; c < 5; c++) {
        lv_obj_t *h = lv_label_create(card);
        lv_label_set_text(h, col_name[c]);
        lv_obj_set_style_text_font(h, &chinese_ziku, 0);
        lv_obj_set_style_text_color(h, IOT_COL_TXT2, 0);
        lv_obj_set_pos(h, col_x[c], 40);
    }

    /* 10 行数据。日期/温度/降水用数字字体 montserrat_16(自带 ° %),
     * 天气现象用中文字库 chinese_ziku(中文)。 */
    const lv_coord_t row_y0 = 70, row_h = 26;
    for(int i = 0; i < FORECAST_MAX_DAYS; i++) {
        lv_coord_t y = row_y0 + i * row_h;

        g_day_date[i] = lv_label_create(card);
        lv_label_set_text(g_day_date[i], "--");
        lv_obj_set_style_text_font(g_day_date[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(g_day_date[i], IOT_COL_ACC, 0);
        lv_obj_set_pos(g_day_date[i], col_x[0], y);

        g_day_text[i] = lv_label_create(card);
        lv_label_set_text(g_day_text[i], "--");
        lv_obj_set_style_text_font(g_day_text[i], &chinese_ziku, 0);
        lv_obj_set_style_text_color(g_day_text[i], IOT_COL_TXT, 0);
        lv_obj_set_pos(g_day_text[i], col_x[1], y - 2);   /* 中文20px比数字高,微调对齐 */

        g_day_high[i] = lv_label_create(card);
        lv_label_set_text(g_day_high[i], "--");
        lv_obj_set_style_text_font(g_day_high[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(g_day_high[i], IOT_COL_TXT, 0);
        lv_obj_set_pos(g_day_high[i], col_x[2], y);

        g_day_low[i] = lv_label_create(card);
        lv_label_set_text(g_day_low[i], "--");
        lv_obj_set_style_text_font(g_day_low[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(g_day_low[i], IOT_COL_TXT2, 0);
        lv_obj_set_pos(g_day_low[i], col_x[3], y);

        g_day_precip[i] = lv_label_create(card);
        lv_label_set_text(g_day_precip[i], "--");
        lv_obj_set_style_text_font(g_day_precip[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(g_day_precip[i], IOT_COL_ACC, 0);
        lv_obj_set_pos(g_day_precip[i], col_x[4], y);
    }

    /* 返回主界面 */
    iot_ui_make_back_btn(scr, 300, 416);
}

/* 主界面按钮入口:用当前城市同步查询未来10天,刷新列表后切到本界面。
 * 查询是阻塞的(和 weather_UI.c 点城市一个套路),期间界面短暂等待。 */
void open_future_weather(void)
{
    char buf[32];
    snprintf(buf, sizeof(buf), "%s", iot_state.city[0] ? iot_state.city : "广州");
    forecast_daily(buf);          /* 失败时 g_forecast10.count=0,列表全显示 "--" */
    future_ui_refresh();          /* 把结果刷到 10 行标签上 */
    iot_ui_show_future();         /* 切换界面(内部会 iot_ui_refresh() 刷云端状态) */
}
