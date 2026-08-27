#include <stdio.h>       // 标准输入输出(printf, perror, fprintf)
#include <stdlib.h>      // 标准库函数(EXIT_FAILURE, EXIT_SUCCESS)
#include <string.h>      // 字符串处理(memset)
#include <time.h>        // 时间处理(time_t, time)
#include <sys/socket.h>  // 套接字接口(socket, setsockopt, sendto, recvfrom)
#include <netinet/in.h>  // 互联网地址族(struct sockaddr_in, AF_INET, IPPROTO_UDP)
#include <arpa/inet.h>   // 互联网操作(inet_pton, htons, ntohl)
#include <unistd.h>      // POSIX操作系统API(close)
#include <sys/time.h>    // 时间类型(struct timeval)
#include <stdint.h>      // 固定宽度整数类型(uint32_t)
#include <errno.h>       // 错误号(errno)
#include "ntp_client.h"         // 包含自定义头文件

time_t get_ntp_time(const char* ntp_server) {
    // 创建UDP套接字
    // AF_INET: IPv4协议族
    // SOCK_DGRAM: 数据报套接字(UDP)
    // IPPROTO_UDP: UDP协议
    int sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if(sockfd < 0) {
        perror("Socket creation failed"); // 输出错误信息到stderr
        return (time_t)-1; // 返回错误值
    }
    
    // 设置套接字接收超时为3秒
    // SOL_SOCKET: 套接字选项级别
    // SO_RCVTIMEO: 接收超时选项
    struct timeval timeout = {3, 0}; // 3秒, 0微秒
    if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Setsockopt failed");
        close(sockfd); // 关闭套接字
        return (time_t)-1;
    }
    
    // 配置NTP服务器地址
    struct sockaddr_in servaddr; // IPv4地址结构
    memset(&servaddr, 0, sizeof(servaddr)); // 清空结构体
    servaddr.sin_family = AF_INET; // 地址族: IPv4
    servaddr.sin_port = htons(123); // 端口号: NTP标准端口123(htons: 主机字节序转网络字节序)
    
    // 将字符串IP地址转换为二进制格式
    // AF_INET: IPv4地址族
    // ntp_server: 源字符串(如"182.92.12.11")
    // &servaddr.sin_addr: 目标地址结构
    if(inet_pton(AF_INET, ntp_server, &servaddr.sin_addr) <= 0) {
        perror("Invalid NTP server address");
        close(sockfd);
        return (time_t)-1;
    }
    
    // 构造NTP请求包(NTP协议数据包固定48字节)
    char ntp_packet[48] = {0}; // 初始化所有字节为0
    ntp_packet[0] = 0x1B; // 设置NTP协议头: 
                          // 0x1B = 00011011 (二进制)
                          // 前2位(00): LI=0(无警告)
                          // 中间3位(011): VN=3(版本3)
                          // 后3位(011): Mode=3(客户端模式)
    
    // 发送NTP请求到服务器
    // sockfd: 套接字描述符
    // ntp_packet: 发送缓冲区
    // sizeof(ntp_packet): 发送数据长度
    // 0: 标志位(通常为0)
    // (struct sockaddr*)&servaddr: 目标地址结构
    // sizeof(servaddr): 地址结构长度
    if(sendto(sockfd, ntp_packet, sizeof(ntp_packet), 0, 
              (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("Sendto failed");
        close(sockfd);
        return (time_t)-1;
    }
    
    // 接收NTP响应
    // sockfd: 套接字描述符
    // ntp_packet: 接收缓冲区
    // sizeof(ntp_packet): 接收缓冲区大小
    // 0: 标志位(通常为0)
    // NULL: 不存储发送方地址(因为我们知道是NTP服务器)
    // NULL: 不存储发送方地址长度
    ssize_t recv_len = recvfrom(sockfd, ntp_packet, sizeof(ntp_packet), 0, NULL, NULL);
    if(recv_len < 0) {
        // 检查是否是超时错误
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            fprintf(stderr, "NTP request timed out\n");
        } else {
            perror("Recvfrom failed");
        }
        close(sockfd);
        return (time_t)-1;
    }
    
    // 关闭套接字
    close(sockfd);
    
    // 检查接收到的数据长度是否正确
    if(recv_len < 48) {
        fprintf(stderr, "Incomplete NTP response received\n");
        return (time_t)-1;
    }
    
    // 解析NTP时间戳(从1900年1月1日起的秒数)
    // 使用第10个32位整数(偏移量40-43字节)的Transmit Timestamp
    // ntohl: 网络字节序转换为主机字节序(大端转小端)
    uint32_t ntp_seconds = ntohl(((uint32_t*)ntp_packet)[10]);
    
    // 转换为UNIX时间戳(1970年1月1日起的秒数)
    // 2208988800是1900年到1970年之间的秒数差
    // 添加1秒补偿以抵消网络传输延迟
    return (time_t)(ntp_seconds - 2208988800U) + 1;
}