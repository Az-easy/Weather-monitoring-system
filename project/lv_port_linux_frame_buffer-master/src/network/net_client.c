/**
 * @file net_client.c
 * @brief 与 Ubuntu 服务器的 TCP 客户端线程 + 云端指令解析
 *
 * 从 main.c 抽出的通信部分:
 *   - mysend():连接 Ubuntu → while{上报天气 → select 收云端指令 → 解析} → close
 *   - handle_cmd() / paras_int():解析 Ubuntu 转发过来的一行 JSON
 *
 * 线程框架保持原样(连接→while→关闭),Ubuntu 端 AgentLiteDemo.c 的 myrecv 是对端;
 * main.c 只调用 net_client_start() 一个入口,不再关心通信细节。
 */
#include "net_client.h"
#include "iot_ui.h"        /* iot_state / iot_ui_set_* / iot_ui_note_upload */
#include "forecast.h"      /* g_weather:mysend 线程读取天气数据发给 ubuntu */
#include "cJSON.h"         /* 天气数据打包成 cJSON 发给 ubuntu */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>    // atoi:解析字符串数字参数
#include <unistd.h>    // close()
#include <pthread.h>
#include <sys/types.h> // man 2 socket
#include <sys/socket.h>
#include <sys/select.h>  /* select():等 Ubuntu 转发的云端指令(顶替 sleep) */
#include <netinet/in.h>  // man 3 inet_addr
#include <arpa/inet.h>

#define SERVER_IP "192.168.44.134" // ubuntu
#define SERVER_PORT 60000

/* 从 paras 里取整数/布尔值:依次试 key(布尔 true→1 / false→0),都取不到返回 fallback */
static int paras_int(cJSON *paras, const char *const *keys, int key_count, int fallback)
{
    for (int i = 0; i < key_count; i++)
    {
        cJSON *it = (paras != NULL) ? cJSON_GetObjectItem(paras, keys[i]) : NULL;
        if (it == NULL) continue;
        if (it->type == cJSON_True)  return 1;
        if (it->type == cJSON_False) return 0;
        if (it->type == cJSON_Number || it->type == cJSON_String) return it->valueint;
        if (it->valuestring != NULL)
            return (strcmp(it->valuestring, "true") == 0) ? 1 : atoi(it->valuestring);
    }
    return fallback;
}

/* —— 解析 Ubuntu 转发过来的一行消息(以 \n 结尾的 JSON)——
 *   {"type":"cmd","data":{云端原始命令}}   → 执行 LED/蜂鸣器/上报间隔等云端指令
 *   {"type":"status","cloud":1/0}          → 云端在线状态(Ubuntu 端云连接成功/断开时推送)
 * 命令名/参数键按华为云产品模型的真实名字匹配(led/beep/setReportingFrequency);
 * 收到不认识的名字会打印出来,方便对照模型核对。 */
static void handle_cmd(const char *line)
{
    cJSON *root = cJSON_Parse(line);
    if (root == NULL) return;

    cJSON *type = cJSON_GetObjectItem(root, "type");
    if (type == NULL || type->valuestring == NULL)
    {
        cJSON_Delete(root);
        return;
    }

    /* 云端连接状态通知:Ubuntu 端云连接成功/断开时主动推一条,板子据此刷新"在线/离线" */
    if (strcmp(type->valuestring, "status") == 0)
    {
        cJSON *cloud = cJSON_GetObjectItem(root, "cloud");
        if (cloud != NULL)
        {
            printf("收到云端状态: cloud=%d\n", cloud->valueint);
            iot_ui_set_cloud(cloud->valueint != 0);   /* 1 在线 / 0 离线 */
        }
        cJSON_Delete(root);
        return;
    }

    if (strcmp(type->valuestring, "cmd") != 0)
    {
        cJSON_Delete(root);
        return;
    }

    cJSON *data = cJSON_GetObjectItem(root, "data");
    if (data == NULL)
    {
        cJSON_Delete(root);
        return;
    }

    cJSON *name_item = cJSON_GetObjectItem(data, "command_name");
    const char *name = (name_item != NULL && name_item->valuestring != NULL) ? name_item->valuestring : "?";

    cJSON *paras = cJSON_GetObjectItem(data, "paras");

    /* 参数键按产品模型的真实键名(led状态/beep状态/value),后面再兜底一个常用键 */
    static const char *led_keys[]  = { "led状态", "value" };
    static const char *beep_keys[] = { "beep状态", "value" };
    static const char *freq_keys[] = { "value", "interval" };

    /* 状态界面"云端指令"显示简洁文本,不显示原始 JSON;默认未知,下面按命令覆盖 */
    char cmd_desc[64];
    snprintf(cmd_desc, sizeof(cmd_desc), "未知指令:%s", name);

    if (strcmp(name, "led") == 0)
    {
        int v = paras_int(paras, led_keys, 2, 0);
        printf("收到云端指令: %s = %d\n", name, v);
        iot_ui_set_led(v == 1);                /* 1 开 / 0 关 */
        snprintf(cmd_desc, sizeof(cmd_desc), "LED%s", v ? "开" : "关");
    }
    else if (strcmp(name, "beep") == 0)
    {
        int v = paras_int(paras, beep_keys, 2, 0);
        printf("收到云端指令: %s = %d\n", name, v);
        iot_ui_set_buzzer(v == 1);             /* 1 响 / 0 停 */
        snprintf(cmd_desc, sizeof(cmd_desc), "蜂鸣器%s", v ? "开" : "关");
    }
    else if (strcmp(name, "setReportingFrequency") == 0)
    {
        int v = paras_int(paras, freq_keys, 2, -1);
        printf("收到云端指令: %s = %d\n", name, v);
        if (v > 0 && v <= 3600)                /* 上报间隔(秒),下次循环生效 */
        {
            iot_state.upload_interval = (uint16_t)v;
            snprintf(cmd_desc, sizeof(cmd_desc), "上报间隔:%d秒", v);
        }
        else
            snprintf(cmd_desc, sizeof(cmd_desc), "上报间隔:无效值");
    }
    else
        printf("注意: 未知命令名[%s],请对照华为云产品模型核对\n", name);

    iot_ui_on_cmd(cmd_desc);   /* 只存简洁文本,状态界面不显示原始 JSON(内部会 refresh) */
    cJSON_Delete(root);
}


void *mysend(void *arg)
{
    // 1. 创建套接字(文件描述符)socket
    //  AF_INET:ipv4 SOCK_STREAM:tcp
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
    {
        printf("socket fail\n");
        pthread_exit(NULL);
    }

    /*
        客户端绑定本机（可有可无）
        如果不绑定：客户端的端口号随机
        如果绑定：客户单的端口号固定
    */

    // 填充服务器的ip和port(用新的结构体 struct sockaddr_in)
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);          // host to net short //将本机端口转换为网络端口(将小端字节序转换为大端字节序)
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP); // 将本机ip转换为网络ip

    // 2. 连接服务器connect
    int ret = connect(socket_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
    if (ret != 0)
    {
        printf("connect server fail\n");
        pthread_exit(NULL);
    }
    printf("连接服务器成功[%s][%d]\n", SERVER_IP, SERVER_PORT);

    // 3. 给服务器发送数据send/write —— 把查询到的天气(g_weather)打包成 cJSON 发给 ubuntu
    //    (线程框架保持原样;发送节奏由 iot_state.upload_interval 控制,云端 setReportingFrequency 可改)
    char buf[1024];
    while (1)
    {
        /* 把 g_weather 装进 cJSON 对象,Print 出单行 JSON。
         * 末尾加 \n 当消息边界,ubuntu 端按行攒够再解析。 */
        cJSON *j = cJSON_CreateObject();
        if (j != NULL)
        {
            cJSON_AddStringToObject(j, "type",           "weather");
            cJSON_AddStringToObject(j, "city",           g_weather.city);
            cJSON_AddStringToObject(j, "text",           g_weather.text);
            cJSON_AddNumberToObject(j, "temp",           g_weather.temp);
            cJSON_AddNumberToObject(j, "feels_like",     g_weather.feels_like);
            cJSON_AddNumberToObject(j, "humidity",       g_weather.humidity);
            cJSON_AddNumberToObject(j, "visibility",     g_weather.visibility);
            cJSON_AddStringToObject(j, "wind_direction", g_weather.wind_direction);
            cJSON_AddNumberToObject(j, "wind_degree",    g_weather.wind_degree);
            /* 设备状态一并上报:LED / 蜂鸣器开关(云端可实时看到远程控制结果)。
             * 产品模型里 led/beep 是 bool 属性,只认 true/false,不能发 0/1。 */
            cJSON_AddBoolToObject(j, "led",  iot_state.led_on);
            cJSON_AddBoolToObject(j, "beep", iot_state.buzzer_on);

            /* 注意:老版 cJSON 的 cJSON_Print() 是多行格式化输出(带 \n 缩进),
             * 服务器按 \n 攒行会把它切碎。必须用 cJSON_PrintUnformatted() 出单行。 */
            char *out = cJSON_PrintUnformatted(j);   /* 单行,如 {"type":"weather",...} */
            if (out != NULL)
            {
                snprintf(buf, sizeof(buf), "%s\n", out);   /* \n 作消息边界 */
                free(out);
                ret = send(socket_fd, buf, strlen(buf), 0);
                if (ret > 0)
                    iot_ui_note_upload();   /* 记录最近一次上报时间(状态界面"最近上报"显示) */
            }
            cJSON_Delete(j);
        }

        // ret = write(socket_fd,buf,strlen(buf));
        printf("send ok ret:%d\n", ret);

        /* 等 Ubuntu 转发的云端指令,最多一个上报周期 —— 顶替原来的 sleep(5),线程框架不变。
         * 周期用 iot_state.upload_interval(云端 setReportingFrequency 命令可改)。
         * 板子↔Ubuntu 是同一条 TCP 连接(TCP 全双工),收发互不干扰;
         * 指令按 \n 攒成一行再交给 handle_cmd(),拆包粘包都兼容。 */
        static char rbuf[512];
        static char rline[512];
        static int  rline_len = 0;

        int interval = iot_state.upload_interval;
        if (interval <= 0) interval = 5;       /* 兜底,别死循环 */
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(socket_fd, &rfds);
        struct timeval tv = { interval, 0 };
        int sret = select(socket_fd + 1, &rfds, NULL, NULL, &tv);
        if (sret > 0 && FD_ISSET(socket_fd, &rfds))
        {
            int n = recv(socket_fd, rbuf, sizeof(rbuf) - 1, 0);
            if (n > 0)
            {
                for (int i = 0; i < n; i++)
                {
                    if (rbuf[i] == '\n')
                    {
                        rline[rline_len] = '\0';
                        handle_cmd(rline);
                        rline_len = 0;
                    }
                    else if (rline_len < (int)sizeof(rline) - 1)
                    {
                        rline[rline_len++] = rbuf[i];
                    }
                }
            }
        }
    }

    // 4. 关闭套接字close
    close(socket_fd);

    return 0;

}

/* 线程入口:创建 mysend 线程。main.c 只调这个,通信细节全部在这里 */
int net_client_start(void)
{
    pthread_t thread;
    int ret = pthread_create(&thread, NULL, mysend, NULL);
    if (ret != 0)
    {
        perror("net_client_start: thread fail");
        return -1;
    }
    return 0;
}
