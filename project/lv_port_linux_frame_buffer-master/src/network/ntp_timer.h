#ifndef __NTP_TIMER_H__
#define __NTP_TIMER_H__

/**
 * @brief 从 NTP 服务器拿网络时间并写入系统时钟(settimeofday)
 * @param ntp_server NTP 服务器 IP,如阿里云 "182.92.12.11"
 * @return 0 成功;-1 失败(网络不通 / 超时 / 无权限)
 *
 * 成功写完后,time(NULL) / localtime() 返回的就是正确时间,
 * 主界面时钟每秒读一次会自动跟着变对。需要 root 权限,板子一般以 root 运行。
 */
int ntp_sync_system_time(const char *ntp_server);

#endif /* __NTP_TIMER_H__ */
