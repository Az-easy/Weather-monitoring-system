/**
 * @file control_UI.c
 * @brief 控制界面:LED 开关 / 蜂鸣器开关
 *
 * 按钮点击后切换 iot_state 中的对应状态,并通过 iot_ui_set_* 触发界面刷新;
 * 实际 GPIO 控制(置高/置低引脚)在 iot_ui_set_led() / iot_ui_set_buzzer() 中对接。
 */
#include "iot_ui.h"
#include "lv_font_source_han_sans_bold.h"
#include <stdio.h>

static void led_btn_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        iot_ui_set_led(!iot_state.led_on);
    }
}

static void buzzer_btn_cb(lv_event_t *e)
{
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        iot_ui_set_buzzer(!iot_state.buzzer_on);
    }
}

/* 在屏幕上创建一个整行的开关控制按钮(含标题 + 状态灯 + 状态文字) */
static lv_obj_t *make_switch_btn(lv_obj_t *scr, lv_coord_t y, const char *title,
                                 lv_event_cb_t cb, lv_obj_t **dot_ref, lv_obj_t **text_ref)
{
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_size(btn, 340, 120);
    lv_obj_align(btn, LV_ALIGN_TOP_MID, 0, y);
    lv_obj_set_style_radius(btn, 16, 0);
    lv_obj_set_style_bg_color(btn, IOT_COL_OFF_BG, 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *t = lv_label_create(btn);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_font(t, &chinese_ziku, 0);
    lv_obj_set_style_text_color(t, IOT_COL_ACC, 0);  /* 标题用青色强调字,和天气卡片按键同款 */
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 16);

    *text_ref = lv_label_create(btn);
    lv_label_set_text(*text_ref, "当前:已关闭");
    lv_obj_set_style_text_font(*text_ref, &chinese_ziku, 0);
    lv_obj_set_style_text_color(*text_ref, IOT_COL_TXT2, 0);
    lv_obj_align(*text_ref, LV_ALIGN_BOTTOM_MID, 0, -14);

    *dot_ref = iot_ui_make_dot(btn, 0, 0, IOT_COL_OFF);
    lv_obj_align_to(*dot_ref, *text_ref, LV_ALIGN_OUT_LEFT_MID, -10, 0);

    return btn;
}

void control_UI(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    g_scr_control = scr;
    iot_ui_screen_bg(scr);
    iot_ui_make_header(scr, "设备控制", &iot_refs_control);

    /* -------- LED 控制 -------- */
    iot_refs_control.led_btn = make_switch_btn(scr, 90, "LED 控制",
                                               led_btn_cb,
                                               &iot_refs_control.led_btn_dot,
                                               &iot_refs_control.led_btn_text);

    /* -------- 蜂鸣器控制 -------- */
    iot_refs_control.buzzer_btn = make_switch_btn(scr, 226, "蜂鸣器控制",
                                                  buzzer_btn_cb,
                                                  &iot_refs_control.buzzer_btn_dot,
                                                  &iot_refs_control.buzzer_btn_text);

    /* -------- 返回主界面 -------- */
    iot_ui_make_back_btn(scr, 300, 380);
}
