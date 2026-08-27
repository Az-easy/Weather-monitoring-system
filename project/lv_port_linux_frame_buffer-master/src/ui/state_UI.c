/**
 * @file state_UI.c
 * @brief 状态界面:设备状态(LED / 蜂鸣器 / 云端连接)+ 云端通信(上报间隔 / 最近上报 / 云端指令)
 */
#include "iot_ui.h"
#include "lv_font_source_han_sans_bold.h"

void state_UI(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    g_scr_state = scr;
    iot_ui_screen_bg(scr);
    iot_ui_make_header(scr, "设备状态", &iot_refs_state);

    /* -------- 设备状态卡片 -------- */
    lv_obj_t *dev = iot_ui_make_card(scr, 20, 90, 370, 190);
    iot_ui_card_title(dev, "设备状态");
    iot_ui_make_status_row(dev, 20, 56,  "LED 状态",  &iot_refs_state.led_dot,    &iot_refs_state.led_text);
    iot_ui_make_status_row(dev, 20, 96,  "蜂鸣器",    &iot_refs_state.buzzer_dot, &iot_refs_state.buzzer_text);
    iot_ui_make_status_row(dev, 20, 136, "云端连接",  &iot_refs_state.conn_dot,   &iot_refs_state.conn_text);

    /* -------- 云端通信卡片 -------- */
    lv_obj_t *net = iot_ui_make_card(scr, 410, 90, 370, 190);
    iot_ui_card_title(net, "云端通信");
    iot_ui_make_status_row(net, 20, 56,  "上报间隔",  NULL, &iot_refs_state.upload_interval);
    iot_ui_make_status_row(net, 20, 96,  "最近上报",  NULL, &iot_refs_state.upload_time);
    iot_ui_make_status_row(net, 20, 136, "云端指令",  NULL, &iot_refs_state.cmd_text);

    /* 云端指令可能较长,超出卡片时省略号截断 */
    lv_label_set_long_mode(iot_refs_state.cmd_text, LV_LABEL_LONG_DOT);
    lv_obj_set_width(iot_refs_state.cmd_text, 150);

    /* -------- 返回主界面 -------- */
    iot_ui_make_back_btn(scr, 300, 350);
}
