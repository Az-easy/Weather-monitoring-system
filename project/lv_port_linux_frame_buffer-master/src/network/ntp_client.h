#ifndef __NTP_CLIENT_H__
#define __NTP_CLIENT_H__

#include <time.h> // 时间相关函数和类型(time_t, struct tm等)

/**
 * @brief 从指定的NTP服务器获取网络时间
 * @param ntp_server NTP服务器的IP地址字符串(如"182.92.12.11")
 * @return 成功返回从1970年1月1日开始的UNIX时间戳，失败返回(time_t)-1
 * 
 * 此函数通过UDP协议与NTP服务器通信，获取准确的网络时间。
 * NTP协议使用48字节固定格式数据包，通过UDP端口123进行通信。
 */
time_t get_ntp_time(const char* ntp_server);

#endif // __NTP_CLIENT_H__