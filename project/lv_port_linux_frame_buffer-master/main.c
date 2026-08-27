#include "lvgl/lvgl.h"
#include "lv_drivers/display/fbdev.h"
#include "lv_drivers/indev/evdev.h"
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>    /* system():播放开机 MP4;setenv/tzset:设东八区时区 */
#include "lv_font_source_han_sans_bold.h"
#include "iot_ui.h"
#include "ntp_timer.h"   /* 阿里云 NTP 校时:开机把系统时间改成正确时间 */
#include "net_client.h"  /* 与 Ubuntu 的 TCP 客户端线程(上报天气/收云端指令) */
#include "./usrcode/ui.h"

#define DISP_BUF_SIZE (480 * 800)

/* NTP 校时线程:开机后台从阿里云 NTP 服务器拿正确时间写进系统时钟。
 * 主界面时钟每秒读一次 time(),写完就自动显示对;拿不到就保持板子当前时间。 */
static void *ntp_sync_thread(void *arg)
{
    (void)arg;
    printf("NTP: 正在从阿里云同步时间...\n");
    if (ntp_sync_system_time("182.92.12.11") == 0)
        printf("NTP: 时间同步成功\n");
    else
        printf("NTP: 时间同步失败,沿用板子当前时间\n");
    return NULL;
}

int main(void)
{
    /* 时区设为东八区(北京时间):NTP 拿到的 epoch 是 UTC,localtime 按此时区转。
     * 放在最前面,保证主界面时钟 / 最近上报时间等所有 localtime 都显示北京时间。 */
    setenv("TZ", "CST-8", 1);
    tzset();

    /*lvgl初始化*/
    lv_init();

    /*输出设备初始化及注册*/
    fbdev_init();
    /*A small buffer for LittlevGL to draw the screen's content*/
    static lv_color_t buf[DISP_BUF_SIZE];
    /*Initialize a descriptor for the buffer*/
    static lv_disp_draw_buf_t disp_buf;
    lv_disp_draw_buf_init(&disp_buf, buf, NULL, DISP_BUF_SIZE);
    /*Initialize and register a display driver*/
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.draw_buf = &disp_buf;
    disp_drv.flush_cb = fbdev_flush;
    disp_drv.hor_res  = 800;
    disp_drv.ver_res  = 480;
    lv_disp_drv_register(&disp_drv);

    // 输入设备初始化及注册
    evdev_init();
    static lv_indev_drv_t indev_drv_1;
    lv_indev_drv_init(&indev_drv_1); /*Basic initialization*/
    indev_drv_1.type = LV_INDEV_TYPE_POINTER;
    /*This function will be called periodically (by the library) to get the mouse position and state*/
    indev_drv_1.read_cb      = evdev_read;
    lv_indev_t * mouse_indev = lv_indev_drv_register(&indev_drv_1);

    /* 开机动画:mplayer 直接写 framebuffer 播 MP4,播完才进主界面。
     * 此时 LVGL 还没画过东西,屏幕归 mplayer 独占,播完自动退出。 */
    system("mplayer -vo fbdev2 -zoom -x 800 -y 480 /IOT/xian/06-lvgl/kaiji.mp4");

    /* 启动与 Ubuntu 的 TCP 通信线程(上报天气 / 接收云端指令),框架在 net_client.c */
    int ret;
    if (net_client_start() != 0)
    {
        printf("net_client_start fail\n");
        return -1;
    }

    /* 后台 NTP 校时线程:开机自动把系统时间改成阿里云 NTP 的准确时间 */
    pthread_t ntp_thread;
    ret = pthread_create(&ntp_thread, NULL, ntp_sync_thread, NULL);
    if (ret == 0)
        pthread_detach(ntp_thread);   /* 同步完就退出,detach 防止变僵尸线程 */

    // IOT 数据监控中心 —— 设备端界面初始化(主界面 / 控制界面 / 状态界面)
    iot_ui_init();

    /*事物处理及告知lvgl节拍数*/
    while(1) {
        lv_timer_handler(); // 事务处理
        lv_tick_inc(5);     // 节拍累计
        usleep(5000);
    }

    return 0;
}

/*用户节拍获取*/
uint32_t custom_tick_get(void)
{
    static uint64_t start_ms = 0;
    if(start_ms == 0) {
        struct timeval tv_start;
        gettimeofday(&tv_start, NULL);
        start_ms = (tv_start.tv_sec * 1000000 + tv_start.tv_usec) / 1000;
    }

    struct timeval tv_now;
    gettimeofday(&tv_now, NULL);
    uint64_t now_ms;
    now_ms = (tv_now.tv_sec * 1000000 + tv_now.tv_usec) / 1000;

    uint32_t time_ms = now_ms - start_ms;
    return time_ms;
}
