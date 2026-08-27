#ifndef __NET_CLIENT_H__
#define __NET_CLIENT_H__

/**
 * @brief 启动与 Ubuntu 服务器的 TCP 客户端线程(天气上报 + 云端指令接收)
 *
 * 后台线程负责:
 *   - 按 iot_state.upload_interval 周期把 g_weather 打包成 cJSON 单行 JSON 上报 Ubuntu;
 *   - 用 select() 同时监听 Ubuntu 转发的云端指令,逐行解析并刷新设备状态界面。
 *
 * @return 0 成功;非 0 线程创建失败(perror 已打印原因)
 */
int net_client_start(void);

#endif /* __NET_CLIENT_H__ */
