/**
 * @file led_beep.h
 * @brief LED / 蜂鸣器 硬件驱动接口
 *
 * 对应设备节点:
 *   - /dev/Led     (ioctl 命令 LED1~LED4,arg:0=亮 1=灭)
 *   - /dev/beep    (ioctl 命令 0=响 1=停)
 * 详见 led_beep.c 实现(由原 led_test.c / buz_test.c 整理而来)。
 */
#ifndef __LED_BEEP_H__
#define __LED_BEEP_H__

#include <stdbool.h>

/* 打开 /dev/Led 和 /dev/beep,返回 0 表示至少一个设备打开成功(失败会打印错误) */
int led_beep_init(void);

/* 控制 LED1 开关:on=true 点亮 / on=false 熄灭。返回 0 成功 */
int led_set(bool on);

/* 控制蜂鸣器开关:on=true 响 / on=false 停。返回 0 成功 */
int beep_set(bool on);

#endif /* __LED_BEEP_H__ */
