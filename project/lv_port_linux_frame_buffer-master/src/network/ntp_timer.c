/**
 * @file ntp_timer.c
 * @brief 阿里云 NTP 校时:拿网络时间并写入系统时钟
 *
 * 由原 standalone 测试程序改造而来:
 *   - 原来的 main() 演示逻辑封装成 ntp_sync_system_time(),板子工程直接调用;
 *   - 独立测试入口 main() 用 NTP_TIMER_TEST_MAIN 宏包起来,
 *     和板子工程一起编(src/*.c 通配)时不会和 main.c 的 main 冲突。
 */
#include <stdio.h>       // 标准输入输出(printf, fprintf, perror)
#include <stdlib.h>      // 标准库函数(EXIT_SUCCESS, EXIT_FAILURE)
#include <time.h>        // 时间处理(time_t, localtime, strftime)
#include <sys/time.h>    // 时间类型(settimeofday, struct timeval)
#include "ntp_client.h"
#include "ntp_timer.h"

int ntp_sync_system_time(const char *ntp_server)
{
    if(ntp_server == NULL) return -1;

    /* 1. UDP 向 NTP 服务器拿网络时间(成功返回 UNIX 时间戳,失败/超时返回 -1) */
    time_t ntp_time = get_ntp_time(ntp_server);
    if(ntp_time == (time_t)-1)
    {
        fprintf(stderr, "NTP: 获取 %s 时间失败\n", ntp_server);
        return -1;
    }

    /* 2. 写进系统时钟(time(NULL) / localtime 从此返回正确时间)。
     *    需要 root 权限,板子一般以 root 运行;失败不影响界面(只是时间不准)。 */
    struct timeval tv = { .tv_sec = ntp_time, .tv_usec = 0 };
    if(settimeofday(&tv, NULL) < 0)
    {
        perror("NTP: settimeofday failed (may need root privileges)");
        return -1;
    }

    /* 3. 打印一下同步后的时间,方便串口核对 */
    struct tm *tm_info = localtime(&ntp_time);
    if(tm_info != NULL)
    {
        char time_buf[64];
        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", tm_info);
        printf("NTP: 时间已同步为 %s\n", time_buf);
    }
    return 0;
}

#ifdef NTP_TIMER_TEST_MAIN
/* 独立测试入口:make NTP_TIMER_TEST_MAIN=1 时才编译这个 main */
int main(void)
{
    if(ntp_sync_system_time("182.92.12.11") == 0)
        return EXIT_SUCCESS;
    return EXIT_FAILURE;
}
#endif
