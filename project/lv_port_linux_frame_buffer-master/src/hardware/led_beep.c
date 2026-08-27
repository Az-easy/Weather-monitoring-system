/**
 * @file led_beep.c
 * @brief LED / 蜂鸣器 硬件驱动(基于 led_test.c / buz_test.c 整理)
 *
 * 使用前需确保内核驱动已 insmod,设备节点:
 *   - /dev/Led   :ioctl(fd, LED1, 0/1)   0=灯亮 1=灯灭
 *   - /dev/beep  :ioctl(fd, 0/1, 1)      0=响   1=停
 */
#include "led_beep.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* ------------------- LED ------------------- */
#define LED_MAGIC 'x'              /* 幻数,与驱动保持一致 */

#define LED1 _IO(LED_MAGIC, 0)     /* 命令 0:控制第 1 颗 LED */
#define LED2 _IO(LED_MAGIC, 1)
#define LED3 _IO(LED_MAGIC, 2)
#define LED4 _IO(LED_MAGIC, 3)

#define LED_ON   0                 /* 灯亮 */
#define LED_OFF  1                 /* 灯灭 */

/* ------------------- 蜂鸣器 ------------------- */
#define BEEP_ON   0                /* 响 */
#define BEEP_OFF  1                /* 停 */

static int led_fd  = -1;
static int beep_fd = -1;

int led_beep_init(void)
{
    led_fd = open("/dev/Led", O_RDWR);
    if(led_fd < 0) {
        perror("/dev/Led open");
    }

    beep_fd = open("/dev/beep", O_RDWR);
    if(beep_fd < 0) {
        perror("/dev/beep open");
    }

    return (led_fd >= 0 || beep_fd >= 0) ? 0 : -1;
}

int led_set(bool on)
{
    if(on)
    {
        ioctl(led_fd, LED1, LED_ON);
        ioctl(led_fd, LED2, LED_ON);
        ioctl(led_fd, LED3, LED_ON);
        ioctl(led_fd, LED4, LED_ON);
    }
    else
    {
        ioctl(led_fd, LED1, LED_OFF);
        ioctl(led_fd, LED2, LED_OFF);
        ioctl(led_fd, LED3, LED_OFF);
        ioctl(led_fd, LED4, LED_OFF);
    }
    return 0;
}

int beep_set(bool on)
{
    if(beep_fd < 0) return -1;
    return ioctl(beep_fd, on ? BEEP_ON : BEEP_OFF, 1);
}
