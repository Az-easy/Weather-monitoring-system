
#ifndef __FORECAST_H__
#define __FORECAST_H__

/* 根据地区名查询实时天气(心知天气 api.seniverse.com)。
 * 成功:填充下面三个全局结果变量并返回 0;失败返回 -1。
 * 注意:name 会被 strtok 修改,请传入可写的缓冲区。 */
int forecast(char *name);

/* 查询结果:forecast() 成功后填充 g_weather。
 * temp/text/city 一定能拿到;feels_like/humidity/visibility/wind_degree
 * 查询失败或接口没返回时为 -1,界面据此显示 "--"。 */
typedef struct {
    int   temp;            /* 温度(℃) */
    char  text[32];        /* 天气描述,如 "晴" "多云" */
    char  city[32];        /* 查询到的城市名 */
    int   feels_like;      /* 体感温度(℃) */
    int   humidity;        /* 相对湿度(%) */
    int   visibility;      /* 能见度(km) */
    char  wind_direction[16]; /* 风向文字,如 "东南风"(接口 now.wind_direction) */
    int   wind_degree;     /* 风向角度(度) */
} weather_t;

extern weather_t g_weather;

/*----------------------- 未来10天天气(daily 接口) -----------------------*/
#define FORECAST_MAX_DAYS 10

/* 单天预测(心知天气 daily.json 返回) */
typedef struct {
    char date[12];        /* 日期 "2026-08-20"(该城市本地时间) */
    char text_day[16];    /* 白天天气现象,如 "多云" */
    int  high;            /* 当天最高温度(℃),查询失败为 -1 */
    int  low;             /* 当天最低温度(℃),查询失败为 -1 */
    int  precip;          /* 降水概率(%),查询失败为 -1 */
} forecast_day_t;

/* 未来10天查询结果:forecast_daily() 成功后填充 */
typedef struct {
    int            count;   /* 实际返回的天数(≤ FORECAST_MAX_DAYS) */
    char           city[32];/* 查询到的城市名 */
    forecast_day_t day[FORECAST_MAX_DAYS];
} forecast10_t;

extern forecast10_t g_forecast10;

/* 查询指定城市未来10天天气(心知天气 daily.json,days=10)。
 * 成功:填充 g_forecast10 并返回 0;失败返回 -1。
 * 注意:name 会被 strtok 修改,请传入可写的缓冲区。 */
int forecast_daily(char *name);

#endif /* __FORECAST_H__ */
