/**
 * @file forecast.c
 * @brief 使用"心知天气"(api.seniverse.com)HTTP 接口查询指定城市的实时天气
 *
 * 由用户提供的原文件整理而来:去掉 main/exit,增加查询结果导出。
 * 调用 forecast(city) 成功后,结果写入天气结构体 g_weather(temp/text/city + 体感/湿度/能见度/风向)。
 */
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <signal.h>
#include <string.h>
#include <strings.h>
#include <stdbool.h>
#include <pthread.h>
#include <semaphore.h>

#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/fb.h>
#include <linux/un.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include "common.h"
#include "cJSON.h"
#include "forecast.h"

/* 查询结果(forecast() 成功后填充) */
weather_t g_weather = {
    .temp           = 0,
    .text           = "无",
    .city           = "",
    .feels_like     = -1,   /* -1 = 未知 */
    .humidity       = -1,
    .visibility     = -1,
    .wind_direction = "",
    .wind_degree    = -1,
};

/* 未来10天查询结果(forecast_daily() 成功后填充,初始为空) */
forecast10_t g_forecast10 = {
    .count = 0,
    .city  = "",
};

/* 从 cJSON 对象里取一个字符串字段:cJSON 函数遇到 NULL 会直接崩,所以先判空;
 * 取不到返回 "?",让 printf 不崩。 */
static const char *jstr(cJSON *obj, const char *key)
{
    cJSON *it = (obj != NULL) ? cJSON_GetObjectItem(obj, key) : NULL;
    return (it != NULL && it->valuestring != NULL) ? it->valuestring : "?";
}

void parse(char * res, int * ok, int * len)
{
    char * retcode = res + strlen("HTTP/1.x ");

    switch(atoi(retcode)) {
        case 200 ... 299:
            *ok = 1;
            printf("查询成功\n");
            break;

        case 400 ... 499:
            *ok = 0;
            printf("客户端错误\n");
            return;

        case 500 ... 599:
            *ok = 0;
            printf("服务端错误\n");
            return;
    }

    char * p;
    if(p = strstr(res, "Content-Length: ")) {
        *len = atoi(p + strlen("Content-Length: "));
    }
}

int forecast(char * name)
{
    /* 每次查询前先把详细数据重置为未知(-1 / 空串),查询失败时界面上不会残留上一次的旧值 */
    g_weather.feels_like  = -1;
    g_weather.humidity    = -1;
    g_weather.visibility  = -1;
    g_weather.wind_degree = -1;
    g_weather.wind_direction[0] = '\0';

    // 利用DNS服务查询指定域名的IP
    struct hostent * he = gethostbyname("api.seniverse.com");
    if(he == NULL) {
        perror("DNS查询失败");
        return -1;
    }

    printf("IP: %s\n", inet_ntoa(*(struct in_addr *)((he->h_addr_list)[0])));

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    bzero(&addr, len);

    addr.sin_family = AF_INET;
    addr.sin_addr   = *(struct in_addr *)((he->h_addr_list)[0]);
    addr.sin_port   = htons(80);

    // 创建TCP套接字(因为HTTP是基于TCP的)，并发起连接
    int fd = Socket(AF_INET, SOCK_STREAM, 0);
    if(connect(fd, (struct sockaddr *)&addr, len) != 0) {
        perror("连接服务器失败");
        close(fd);
        return -1;
    }
    printf("连接服务器成功！\n");

    // 准备好HTTP的请求报文
    static char request[1024];

    snprintf(request, 1024,
             "GET /v3/weather/now.json?key=SsQUuIR1jsaREvf3P&location=%s&language=zh-Hans&unit=c HTTP/1.1\r\n"
             "Host:api.seniverse.com\r\n\r\n",
             strtok(name, "\n"));
    char * s = request;

    write(fd, s, strlen(s));

    // 接收对方的响应头部
    char res[1024];
    int total = 0;
    while(1) {
        int n = read(fd, res + total, 1);

        if(n <= 0) {
            perror("读取HTTP头部失败");
            close(fd);
            return -1;
        }

        total += n;

        if(strstr(res, "\r\n\r\n")) break;
    }

    // 分析响应码，并获取正文的长度
    int ok = 0, jsonlen = 0;
    parse(res, &ok, &jsonlen);
    if(!ok || jsonlen == 0) {
        close(fd);
        return -1;
    }

    // 接收正文(读到的字节数不足或连接中断就停,不无限等)
    char * json = calloc(1, jsonlen);
    total       = 0;
    while(jsonlen > 0) {
        int n = read(fd, json + total, jsonlen);
        if(n <= 0) break;
        total += n;
        jsonlen -= n;
    }
    close(fd);

    cJSON * root = cJSON_Parse(json);
    free(json);
    if(root == NULL) return -1;

    /* cJSON 的函数拿到 NULL 指针会直接崩,所以每一层都要先判空再往下取。
     * 接口出错时返回的是 error JSON(没有 results 数组),body / a / now 会是 NULL,
     * 不判空就是段错误。 */
    cJSON * body = cJSON_GetObjectItem(root, "results");
    if(body == NULL) { cJSON_Delete(root); return -1; }

    cJSON * a = cJSON_GetArrayItem(body, 0);
    if(a == NULL) { cJSON_Delete(root); return -1; }

    cJSON * now      = cJSON_GetObjectItem(a, "now");
    cJSON * location = cJSON_GetObjectItem(a, "location");
    if(now == NULL || location == NULL) { cJSON_Delete(root); return -1; }

    // 打印调试信息(jstr 已判空,接口少字段时不崩)
    printf("天气情况: %s\n", jstr(now, "text"));
    printf("当前气温: %s°C\n", jstr(now, "temperature"));
    printf("体感温度: %s°C\n", jstr(now, "feels_like"));
    printf("相对湿度: %s%%\n", jstr(now, "humidity"));
    printf("能见度: %s km\n", jstr(now, "visibility"));
    printf("风向: %s %s°\n", jstr(now, "wind_direction"), jstr(now, "wind_direction_degree"));

    /* 保存解析结果到天气结构体 g_weather,供界面显示。
     * 注意:体感温度/湿度/能见度/风向都在 now 对象里(不在 location 里),别再写错。 */
    cJSON * now_txt        = cJSON_GetObjectItem(now, "text");
    cJSON * now_temp       = cJSON_GetObjectItem(now, "temperature");
    cJSON * loc_name       = cJSON_GetObjectItem(location, "name");
    cJSON * now_feels_like = cJSON_GetObjectItem(now, "feels_like");
    cJSON * now_humidity   = cJSON_GetObjectItem(now, "humidity");
    cJSON * now_visibility = cJSON_GetObjectItem(now, "visibility");
    cJSON * now_wind_dir   = cJSON_GetObjectItem(now, "wind_direction");
    cJSON * now_wind_deg   = cJSON_GetObjectItem(now, "wind_direction_degree");

    if(now_txt && now_txt->valuestring) {
        snprintf(g_weather.text, sizeof(g_weather.text), "%s", now_txt->valuestring);
    }
    if(now_temp && now_temp->valuestring) {
        g_weather.temp = atoi(now_temp->valuestring);
    }
    if(loc_name && loc_name->valuestring) {
        snprintf(g_weather.city, sizeof(g_weather.city), "%s", loc_name->valuestring);
    } else {
        snprintf(g_weather.city, sizeof(g_weather.city), "%s", strtok(name, "\n"));
    }

    /* 天气详细(字段在 now 里,取不到就保持 -1 = 未知,界面显示 "--") */
    if(now_feels_like && now_feels_like->valuestring) {
        g_weather.feels_like = atoi(now_feels_like->valuestring);
    }
    if(now_humidity && now_humidity->valuestring) {
        g_weather.humidity = atoi(now_humidity->valuestring);
    }
    if(now_visibility && now_visibility->valuestring) {
        g_weather.visibility = atoi(now_visibility->valuestring);
    }
    if(now_wind_dir && now_wind_dir->valuestring) {
        snprintf(g_weather.wind_direction, sizeof(g_weather.wind_direction), "%s", now_wind_dir->valuestring);
    }
    if(now_wind_deg && now_wind_deg->valuestring) {
        g_weather.wind_degree = atoi(now_wind_deg->valuestring);
    }

    cJSON_Delete(root);
    return 0;
}

/* 查询未来10天天气:和 forecast() 同一套 HTTP 套路,只换接口路径(/v3/weather/daily.json)
 * 和解析目标(daily 数组 → 最高温/最低温/降水概率/天气现象)。
 * 结果写入 g_forecast10,成功返回 0,失败返回 -1。 */
int forecast_daily(char *name)
{
    /* 每次查询前先重置结果,查询失败时界面上不残留上一次的数据 */
    g_forecast10.count = 0;
    g_forecast10.city[0] = '\0';
    for(int i = 0; i < FORECAST_MAX_DAYS; i++) {
        g_forecast10.day[i].date[0]     = '\0';
        g_forecast10.day[i].text_day[0] = '\0';
        g_forecast10.day[i].high        = -1;
        g_forecast10.day[i].low         = -1;
        g_forecast10.day[i].precip      = -1;
    }

    // 利用DNS服务查询指定域名的IP
    struct hostent * he = gethostbyname("api.seniverse.com");
    if(he == NULL) {
        perror("DNS查询失败");
        return -1;
    }

    printf("IP: %s\n", inet_ntoa(*(struct in_addr *)((he->h_addr_list)[0])));

    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    bzero(&addr, len);

    addr.sin_family = AF_INET;
    addr.sin_addr   = *(struct in_addr *)((he->h_addr_list)[0]);
    addr.sin_port   = htons(80);

    // 创建TCP套接字(因为HTTP是基于TCP的)，并发起连接
    int fd = Socket(AF_INET, SOCK_STREAM, 0);
    if(connect(fd, (struct sockaddr *)&addr, len) != 0) {
        perror("连接服务器失败");
        close(fd);
        return -1;
    }
    printf("连接服务器成功！\n");

    // 准备好HTTP的请求报文(daily 接口:days=10 拿未来10天)
    static char request[1024];

    snprintf(request, 1024,
             "GET /v3/weather/daily.json?key=SsQUuIR1jsaREvf3P&location=%s&language=zh-Hans&unit=c&start=0&days=10 HTTP/1.1\r\n"
             "Host:api.seniverse.com\r\n\r\n",
             strtok(name, "\n"));
    char * s = request;

    write(fd, s, strlen(s));

    // 接收对方的响应头部
    char res[1024];
    int total = 0;
    while(1) {
        int n = read(fd, res + total, 1);

        if(n <= 0) {
            perror("读取HTTP头部失败");
            close(fd);
            return -1;
        }

        total += n;

        if(strstr(res, "\r\n\r\n")) break;
    }

    // 分析响应码，并获取正文的长度
    int ok = 0, jsonlen = 0;
    parse(res, &ok, &jsonlen);
    if(!ok || jsonlen == 0) {
        close(fd);
        return -1;
    }

    // 接收正文(读到的字节数不足或连接中断就停,不无限等)
    char * json = calloc(1, jsonlen);
    total       = 0;
    while(jsonlen > 0) {
        int n = read(fd, json + total, jsonlen);
        if(n <= 0) break;
        total += n;
        jsonlen -= n;
    }
    close(fd);

    cJSON * root = cJSON_Parse(json);
    free(json);
    if(root == NULL) return -1;

    /* 接口出错时返回的是 error JSON(没有 results 数组),每一层都要判空,不判空就是段错误 */
    cJSON * body = cJSON_GetObjectItem(root, "results");
    if(body == NULL) { cJSON_Delete(root); return -1; }

    cJSON * a = cJSON_GetArrayItem(body, 0);
    if(a == NULL) { cJSON_Delete(root); return -1; }

    cJSON * location = cJSON_GetObjectItem(a, "location");
    cJSON * daily    = cJSON_GetObjectItem(a, "daily");
    if(location == NULL || daily == NULL) { cJSON_Delete(root); return -1; }

    /* 城市名 */
    cJSON * loc_name = cJSON_GetObjectItem(location, "name");
    if(loc_name && loc_name->valuestring) {
        snprintf(g_forecast10.city, sizeof(g_forecast10.city), "%s", loc_name->valuestring);
    } else {
        snprintf(g_forecast10.city, sizeof(g_forecast10.city), "%s", strtok(name, "\n"));
    }

    /* 解析 daily 数组,最多取 10 天 */
    int n = cJSON_GetArraySize(daily);
    if(n > FORECAST_MAX_DAYS) n = FORECAST_MAX_DAYS;
    if(n < 0) n = 0;
    g_forecast10.count = n;

    for(int i = 0; i < n; i++) {
        cJSON * d = cJSON_GetArrayItem(daily, i);
        if(d == NULL) continue;

        cJSON * date     = cJSON_GetObjectItem(d, "date");
        cJSON * text_day = cJSON_GetObjectItem(d, "text_day");
        cJSON * high     = cJSON_GetObjectItem(d, "high");
        cJSON * low      = cJSON_GetObjectItem(d, "low");
        cJSON * precip   = cJSON_GetObjectItem(d, "precip");

        if(date && date->valuestring)
            snprintf(g_forecast10.day[i].date, sizeof(g_forecast10.day[i].date), "%s", date->valuestring);
        if(text_day && text_day->valuestring)
            snprintf(g_forecast10.day[i].text_day, sizeof(g_forecast10.day[i].text_day), "%s", text_day->valuestring);
        if(high && high->valuestring)      g_forecast10.day[i].high   = atoi(high->valuestring);
        if(low  && low->valuestring)       g_forecast10.day[i].low    = atoi(low->valuestring);

        /* 降水概率:接口给的是 0~1 的小数(如 0.45 = 45%),不能直接 atoi(会截成0),
         * 先按小数读,≤1 就乘 100 转成百分比整数;个别日期接口直接给百分数(如 45)也兼容。 */
        if(precip && precip->valuestring) {
            double p = strtod(precip->valuestring, NULL);
            if(p <= 1.0) p *= 100.0;
            g_forecast10.day[i].precip = (int)(p + 0.5);
        }

        printf("第%d天: 日期=%s 天气=%s 最高=%s 最低=%s 降水概率=%d%%\n",
               i + 1, jstr(d, "date"), jstr(d, "text_day"),
               jstr(d, "high"), jstr(d, "low"), g_forecast10.day[i].precip);
    }

    cJSON_Delete(root);
    return 0;
}
