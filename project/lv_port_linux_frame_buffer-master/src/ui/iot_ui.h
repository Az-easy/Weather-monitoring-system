/**
 * @file iot_ui.h
 * @brief IOT 数据监控中心 —— 设备端 LVGL 界面公共接口
 *
 * 界面:主界面(mian_UI.c)、控制界面(control_UI.c)、状态界面(state_UI.c)、
 *       天气查询界面(weather_UI.c)。
 * 界面显示的数据统一放在 iot_state 中,由 iot_ui_set_* 系列更新;
 * 网络(HTTP 天气 / TCP 云通信)和 GPIO(LED、蜂鸣器)模块对接这些接口即可实时刷新界面。
 */
#ifndef __IOT_UI_H__
#define __IOT_UI_H__

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------- 界面配色(供各界面文件复用) -----------------------*/
#define IOT_COL_ON     lv_color_hex(0x22c55e)   /* 开启/在线:绿色 */
#define IOT_COL_RED    lv_color_hex(0xef4444)   /* 异常/离线:红色 */
#define IOT_COL_OFF    lv_color_hex(0x64748b)   /* 关闭:灰色 */
#define IOT_COL_ON_BG  lv_color_hex(0x14532d)   /* 按钮开启底色 */
#define IOT_COL_OFF_BG lv_color_hex(0x1e293b)   /* 按钮关闭底色 */
#define IOT_COL_TXT    lv_color_hex(0xe8eefc)   /* 主文字 */
#define IOT_COL_TXT2   lv_color_hex(0x9fb4e0)   /* 次级文字 */
#define IOT_COL_ACC    lv_color_hex(0x22d3ee)   /* 强调色(青色) */

/* 温度"无数据"标记:temperature 等于它时,界面显示 "--" 而不是编造的温度。
 * 开机天气还没查回来之前,iot_state.temperature 就是这个值。 */
#define IOT_TEMP_UNKNOWN ((int16_t)-32768)

/* 其它数值(体感/湿度/能见度/风向)的"无数据"标记:小于 0 一律按未知处理,显示 "--" */
#define IOT_VAL_UNKNOWN ((int16_t)-1)

/*----------------------- 设备运行状态(共享数据) -----------------------*/
typedef struct {
    bool     led_on;            /* LED 开关 */
    bool     buzzer_on;         /* 蜂鸣器开关 */
    bool     cloud_connected;   /* 云端连接 */
    int16_t  temperature;       /* 温度(摄氏度) */
    char     weather_desc[16];  /* 天气描述,如 "晴" "多云" "小雨" */
    char     city[16];          /* 城市 */
    /* 天气详细(主界面"天气详细"卡片,数据来自 forecast.c 的 g_weather) */
    int16_t  feels_like;        /* 体感温度(摄氏度),<0 未知显示 "--" */
    int16_t  humidity;          /* 相对湿度(%),<0 未知显示 "--" */
    int16_t  visibility;        /* 能见度(km),<0 未知显示 "--" */
    char     wind_direction[16];/* 风向文字,如 "东南风"(来自接口),空串时界面只显示角度 */
    int16_t  wind_degree;       /* 风向角度(度),<0 未知显示 "--" */
    char     last_cmd[64];      /* 云端下发的最后一条命令 */
    char     last_upload[16];   /* 最近一次上报时间 "HH:MM:SS",未上报为 "--:--:--" */
    uint16_t upload_interval;   /* 数据上报间隔(秒) */
} iot_ui_state_t;

extern iot_ui_state_t iot_state;

/*----------------------- 界面动态控件引用 -----------------------*/
/* 各界面需要随 iot_state 刷新/变色的控件指针,由界面构建函数填充 */
typedef struct {
    /* 顶栏:云端状态 */
    lv_obj_t *cloud_dot;
    lv_obj_t *cloud_text;
    /* 设备状态行(主界面状态卡片 / 状态界面) */
    lv_obj_t *led_dot;
    lv_obj_t *led_text;
    lv_obj_t *buzzer_dot;
    lv_obj_t *buzzer_text;
    /* 状态界面:云端连接行 */
    lv_obj_t *conn_dot;
    lv_obj_t *conn_text;
    /* 天气(主界面) */
    lv_obj_t *city_text;
    lv_obj_t *temp_text;
    lv_obj_t *weather_text;
    /* 天气详细(主界面,由 mian_UI() 填充) */
    lv_obj_t *feels_like_text;
    lv_obj_t *humidity_text;
    lv_obj_t *visibility_text;
    lv_obj_t *wind_text;
    /* 数据上报 */
    lv_obj_t *upload_text;      /* "已上报/未上报" */
    lv_obj_t *upload_time;      /* 最近上报时间 */
    lv_obj_t *upload_interval;  /* 状态界面:上报间隔 */
    lv_obj_t *cmd_text;         /* 状态界面:云端指令 */
    /* 控制界面:LED / 蜂鸣器按钮 */
    lv_obj_t *led_btn;
    lv_obj_t *led_btn_dot;
    lv_obj_t *led_btn_text;
    lv_obj_t *buzzer_btn;
    lv_obj_t *buzzer_btn_dot;
    lv_obj_t *buzzer_btn_text;
} iot_ui_refs_t;

/* 各界面的控件引用与屏幕对象。
 * 定义在 iot_ui.c;由各界面构建函数(mian_UI()/weather_UI() 等)填充。
 * 构建函数被 iot_ui_init() 调用后,界面即完成"注册"。
 * 屏幕对象 g_scr_* 存的是 lv_obj_create(NULL) 建出的屏幕,供 lv_disp_load_scr() 切换。 */
extern iot_ui_refs_t iot_refs_home;
extern iot_ui_refs_t iot_refs_control;
extern iot_ui_refs_t iot_refs_state;
extern iot_ui_refs_t iot_refs_weather;
extern iot_ui_refs_t iot_refs_future;
extern lv_obj_t *g_scr_home;
extern lv_obj_t *g_scr_control;
extern lv_obj_t *g_scr_state;
extern lv_obj_t *g_scr_weather;
extern lv_obj_t *g_scr_future;

/*----------------------- 对外接口 -----------------------*/
void iot_ui_init(void);             /* 创建全部界面并加载主界面 */
void iot_ui_show_home(void);        /* 切换界面 */
void iot_ui_show_control(void);
void iot_ui_show_state(void);
void iot_ui_show_weather(void);
void iot_ui_show_future(void);      /* 未来10天天气界面 */
void iot_ui_refresh(void);          /* 用 iot_state 刷新所有界面上的动态控件 */

/* 对接 GPIO / HTTP 天气 / TCP 云通信 的更新入口 */
void iot_ui_set_led(bool on);
void iot_ui_set_buzzer(bool on);
void iot_ui_set_cloud(bool connected);
/* 更新实时天气(含详细数据)。
 * feels_like / humidity / visibility / wind_degree 未知时传 IOT_TEMP_UNKNOWN 或 IOT_VAL_UNKNOWN,
 * 界面会显示 "--";wind_direction 是风向文字(如"东南风"),可传 NULL 或空串。 */
void iot_ui_set_weather(const char *city, int temp, const char *desc,
                        int feels_like, int humidity, int visibility,
                        const char *wind_direction, int wind_degree);
void iot_ui_on_cmd(const char *cmd);
void iot_ui_note_upload(void);

/*----------------------- 通用控件助手(iot_ui.c 实现,供三个界面复用) -----------------------*/
void      iot_ui_screen_bg(lv_obj_t *scr);    /* 屏幕渐变背景 */
lv_obj_t *iot_ui_make_header(lv_obj_t *parent, const char *title, iot_ui_refs_t *refs); /* 顶部栏 */
lv_obj_t *iot_ui_make_card(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h); /* 圆角卡片 */
void      iot_ui_card_title(lv_obj_t *card, const char *title); /* 卡片标题 */
lv_obj_t *iot_ui_make_dot(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_color_t color); /* 圆形状态灯 */
void      iot_ui_make_status_row(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                                 const char *name, lv_obj_t **dot_ref, lv_obj_t **text_ref); /* "名称 + 状态灯 + 值" 行 */
lv_obj_t *iot_ui_make_back_btn(lv_obj_t *parent, lv_coord_t x, lv_coord_t y); /* 返回主界面按钮 */

/* 界面构建入口(各界面 .c 文件实现) */
void mian_UI(void);
void control_UI(void);
void state_UI(void);
void weather_UI(void);

#ifdef __cplusplus
}
#endif

#endif /* __IOT_UI_H__ */
