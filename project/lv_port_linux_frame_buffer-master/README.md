# 知贤天气安全指挥中心 —— IOT 数据检测系统 技术文档

> 本文档面向项目验收/学生手册,描述项目的功能、技术栈、实现思路、亮点与难点。

---

## 一、项目概述

### 1.1 项目是什么

本项目是一个运行在嵌入式 Linux 开发板上的 **IOT 数据检测与远程控制系统**。设备端带一块 800×480 的 LCD 触摸屏,通过 LVGL 图形库绘制了一套"天气安全指挥中心"风格的监控界面,实现:

- **实时天气监控**:开机自动从"心知天气"云 API 查询广州实时天气(温度/体感/湿度/能见度/风向),支持 8 个城市切换查询;
- **未来 10 天天气预报**:查询并列表展示最高温、最低温、降水概率、天气现象;
- **云端远程控制**:设备通过 TCP 把天气数据上报到 Ubuntu 中转服务器,华为云平台经服务器下发指令,可远程开关 **LED 灯**和**蜂鸣器**,并可动态调整数据上报间隔;
- **设备状态监控**:界面实时显示 LED、蜂鸣器、云端连接状态,记录最近一次上报时间和云端下发的最后一条指令;
- **自动校时**:开机后台从阿里云 NTP 服务器同步系统时间,主界面时钟自动校准为北京时间;
- **开机动画**:启动时通过 mplayer 直接写 framebuffer 播放 MP4 开机动画,播完再进入主界面。

### 1.2 运行环境

| 项目 | 说明 |
|------|------|
| 硬件 | 嵌入式 Linux 开发板(ARM),800×480 触摸屏 |
| 显示 | Linux framebuffer(`/dev/fb0`) |
| 输入 | evdev 触摸/鼠标(`/dev/input/...`) |
| 设备节点 | `/dev/Led`(ioctl 控制灯)、`/dev/beep`(ioctl 控制蜂鸣器) |
| 编译器 | `arm-linux-gcc`(交叉编译) |
| 分辨率 | 800 × 480 |
| 服务器 | Ubuntu(IP `192.168.44.134`,端口 `60000`),转发华为云指令 |

---

## 二、技术栈

| 类别 | 技术 | 说明 |
|------|------|------|
| 开发语言 | C(`-std=gnu99`) | 纯 C 工程,非 C++ |
| 图形界面 | **LVGL 8.x** | 开源嵌入式 GUI 库,控件式绘图 |
| 显示驱动 | **lv_drivers / fbdev** | framebuffer 输出设备驱动 |
| 输入驱动 | **lv_drivers / evdev** | 触摸屏/鼠标输入设备驱动 |
| 系统编程 | **Linux 系统调用 + pthread** | socket、ioctl、select、多线程 |
| 网络(天气) | **HTTP/1.1 + 心知天气 API** | 查询实时天气与每日预报 |
| 网络(上报) | **TCP(SOCK_STREAM)** | 板端 ↔ Ubuntu 中转服务器 |
| 网络(校时) | **UDP + NTP 协议**(端口 123) | 阿里云 NTP 服务器 |
| 数据格式 | **JSON(cJSON 库)** | 天气数据打包/解析、云端指令解析 |
| 硬件控制 | **ioctl** | 操作 `/dev/Led`、`/dev/beep` 设备节点 |
| 中文显示 | **自生成中文字库** `chinese_ziku` | LVGL 自定义字体 |
| 构建 | **Makefile + arm-linux-gcc** | 自动收集源码、`obj/` 目录存放 `.o` |
| 工具链辅助 | SquareLine Studio | `usrcode/` 为可视化生成的模板工程(当前工程未调用其 `ui_init()`) |

---

## 三、系统架构

工程采用 **分层 + 模块化** 组织,源码位于 `src/` 下,按职责划分:

```
main.c                       启动入口:初始化、开线程、播放动画、进入 LVGL 主循环
src/
├── ui/       界面层      iot_ui.c/.h、mian_UI.c、control_UI.c、state_UI.c、weather_UI.c、future_UI.c
├── network/  网络层      forecast.c/.h、net_client.c/.h、ntp_client.c/.h、ntp_timer.c/.h
├── hardware/ 硬件层      led_beep.c/.h
├── common/   公共层      wrap.c/.h(socket 错误包装)
└── lib/      第三方库    cJSON.c/.h
Ubuntu服务器.c             PC 端 TCP 中转服务器(接收天气 JSON 并打印/转发)
usrcode/                  SquareLine Studio 生成模板(编译但不运行)
```

```
┌───────────────────────────── LVGL 界面层 ─────────────────────────────┐
│   主界面 mian_UI.c   控制界面 control_UI.c   状态界面 state_UI.c      │
│   天气查询 weather_UI.c   未来10天 future_UI.c                        │
│   ┌───────────────────────────────────────────────────────────────┐   │
│   │ 共享状态 iot_state + 统一刷新 iot_ui_refresh()                 │   │
│   └───────────────────────────────────────────────────────────────┘   │
└──────────────┬───────────────────────────────┬───────────────────────┘
               │ iot_ui_set_*()                │ iot_ui_set_*()
   ┌───────────▼───────────┐      ┌────────────▼─────────────┐
   │ 网络层 src/network    │      │ 硬件层 src/hardware      │
   │ forecast  天气HTTP    │      │ led_beep  ioctl控制      │
   │ net_client TCP上报/收 │      └──────────────────────────┘
   │ ntp        时间同步   │
   └───────────────────────┘
```

**数据流核心模式**:所有界面不直接访问硬件/网络,而是统一读写全局状态 `iot_state`;
网络和硬件模块通过 `iot_ui_set_*()` 系列接口写状态并触发 `iot_ui_refresh()` 统一刷新界面,界面切换时再同步一次,保证任意时刻切到任意界面显示的都是最新数据。

---

## 四、功能说明

### 功能一:主界面(实时监控看板)
- 顶部标题栏:系统名称 + **云端在线/离线**状态灯;
- 大号数字时钟(时分秒,每秒刷新)+ 日期星期(中文星期);
- **实时天气卡片**:城市、温度(`°C`)、天气描述(如"晴/多云");
- **天气详细卡片**:体感温度、相对湿度、能见度、风向;
- **功能导航卡片**:跳转控制界面、状态界面;
- 底部**滚动公告栏**:超长文字自动循环滚动。

### 功能二:控制界面
- **LED 控制按钮**、**蜂鸣器控制按钮**:点击即切换开关,通过 ioctl 操作 `/dev/Led`、`/dev/beep`,按钮颜色与状态灯同步变化。

### 功能三:状态界面
- 设备状态:LED 状态、蜂鸣器状态、云端连接状态;
- 云端通信:上报间隔、最近上报时间、云端最后指令(超长省略号截断)。

### 功能四:天气查询界面
- 8 个城市按钮(上饶/广州/北京/上海/成都/杭州/武汉/西安),点击后调用 HTTP 查询,成功回主界面更新天气,失败显示提示。

### 功能五:未来 10 天天气界面
- 10 行列表:日期、天气、最高温、最低温、降水概率。

### 功能六:云端远程控制与上报
- 板端每 `upload_interval` 秒(默认 10s)把天气打包成单行 JSON 上报 Ubuntu 服务器;
- 服务器把华为云下发的指令(JSON)转发给板端,板端解析执行:
  - `led` → 远程开/关 LED;
  - `beep` → 远程开/关蜂鸣器;
  - `setReportingFrequency` → 动态修改上报间隔(1~3600 秒)。

### 功能七:开机自检与美化
- NTP 自动校时(阿里云 UDP);
- mplayer 播放 MP4 开机动画。

---

## 五、项目亮点

1. **界面美观统一**:全工程配色收敛为 `iot_ui.h` 中的 8 个 `IOT_COL_*` 常量,深蓝渐变背景 + 圆角半透明卡片 + 状态灯,风格一致;大号时钟、滚动公告、开机动画提升观感。
2. **模块化程度高**:界面/网络/硬件/公共四层分离,各界面独立成文件,公共控件(卡片、标题栏、状态行、状态灯、返回按钮)全部抽成复用函数,避免重复代码。
3. **统一数据模型**:一个全局 `iot_state` + 一套 `iot_ui_set_*` 接口,外部模块对接点清晰,界面与业务解耦。
4. **防御性编程防段错误**:
   - cJSON 所有取值前判空(`jstr()` 等辅助函数),接口异常返回时不会崩;
   - 天气数据"未知值"用 `IOT_TEMP_UNKNOWN`/`IOT_VAL_UNKNOWN` 标记,界面显示 `--` 而不是编造数据;
   - 所有 `strncpy` 都带长度上限并手动补 `\0`。
5. **网络健壮性细节**:
   - 上报用 `cJSON_PrintUnformatted()` 输出**单行** JSON,末尾加 `\n` 当消息边界;
   - 收发两侧都按 `\n` **攒行解析,兼容拆包和粘包**;
   - 用 `select()` 设定超时等待云端指令,顶替裸 `sleep()`,收发共用一条 TCP 全双工连接。
6. **扩展功能丰富**:未来 10 天预报、多城市查询、NTP 校时、上报间隔云端可调,超出课程规定范围。
7. **注释完整**:每个文件头部有模块说明,关键算法有中文注释解释"为什么这样写"。

---

## 六、难点与解决

| 难点 | 现象/风险 | 解决方案 |
|------|-----------|----------|
| **中文显示** | 中文字库缺 "°" 等符号,体感温度 `°C` 显示空白 | 数值/符号用 `lv_font_montserrat_16` 数字字体,中文用 `chinese_ziku` 中文字库,按控件类型分别指定字体 |
| **TCP 拆包/粘包** | 一条消息可能被切碎或几条粘在一起,直接解析会错乱 | 按 `\n` 行边界攒缓冲,攒满一行再交给 `handle_cmd()` 解析;收发两侧同一约定 |
| **cJSON 空指针段错误** | 接口返回 error JSON 时 `results`/`now` 为 NULL,直接取值必崩 | 每一层 `cJSON_GetObjectItem` 结果都判空后再取成员;封装 `jstr()` 兜底 |
| **同步网络查询卡界面** | 天气查询是阻塞的,查询期间界面短暂等待 | 点击城市先 `lv_refr_now(NULL)` 强制重绘"正在查询…"提示,再执行网络查询,避免"点了没反应" |
| **多线程共享数据** | 网络线程与 LVGL 主线程同时读写 `iot_state`,无锁 | 用 `volatile` 级短字段 + 只在单线程内改复杂字段,验收场景可接受;文档标注为后续可加 mutex 优化点 |
| **NTP 时区** | NTP 返回的是 UTC 时间戳,直接显示差 8 小时 | 启动时 `setenv("TZ","CST-8",1); tzset();` 设定东八区,所有 `localtime()` 自动转北京时间 |
| **无网络优雅降级** | 开机没网 / 查询失败,界面不该显示假数据 | 温度等未知字段显示 `--`、天气显示"获取失败",云端状态灯显示"离线",系统照常运行 |
| **字节序/字节对齐** | 网络字节序大端、本机小端 | `htons`/`ntohs`/`inet_addr`/`inet_pton` 等统一转换 |
| **交叉编译** | 板端程序要在 PC 上交叉编译,依赖多目录 | Makefile 用 `wildcard`/`find` 自动收集 `src/*` 与 lvgl 源码,`obj/` 目录隔离中间文件 |

---

## 七、代码整体实现

### 7.1 启动流程(`main.c`)

```
main()
 ├─ setenv("TZ","CST-8") + tzset()          # ① 设置东八区
 ├─ lv_init()                               # ② LVGL 核心初始化
 ├─ fbdev_init()                            # ③ 显示驱动(framebuffer)
 ├─ 注册显示驱动(800×480, 单色缓冲区)
 ├─ evdev_init() + 注册触摸输入驱动
 ├─ net_client_start()                      # ④ 启动 TCP 上报/收指令线程
 ├─ 创建 NTP 校时线程(异步)
 ├─ system("mplayer ... kaiji.mp4")         # ⑤ 开机动画(framebuffer 直写)
 ├─ iot_ui_init()                           # ⑥ 打开LED/蜂鸣器设备 + 创建5个界面
 │     └─ 开机自动 forecast("广州") 查一次实时天气
 ├─ while(1){ lv_timer_handler(); lv_tick_inc(5); usleep(5000); }   # ⑦ LVGL 主循环
```

### 7.2 界面注册与切换机制

- 每个界面一个构建函数(`mian_UI()`/`control_UI()`/…),在 `iot_ui_init()` 中调用**一次**,创建独立屏幕对象存入全局 `g_scr_*`,并把需要动态刷新的控件指针填进全局 `iot_refs_*`;
- 切换界面 = 调 `iot_ui_show_*()` 用 `lv_disp_load_scr(g_scr_*)` 加载对应屏幕;
- `iot_ui_refresh()` 遍历 5 个 `iot_refs_*`,把 `iot_state` 的最新值刷到所有界面(包括当前不可见的),保证切过去就是新数据。

### 7.3 天气数据流

```
心知天气 API ──HTTP──▶ forecast(city) ──解析 cJSON──▶ g_weather(weather_t)
    ▶ iot_ui_set_weather(...) ──▶ iot_state ──▶ iot_ui_refresh() ──▶ 主界面卡片
```

### 7.4 云端指令数据流(下行)

```
华为云 ──▶ Ubuntu(AgentLiteDemo) ──TCP──▶ mysend 线程 select() 收到指令
    ▶ 攒行到 \n ──▶ handle_cmd(line) 解析 cJSON
    ▶ led/beep → iot_ui_set_led()/iot_ui_set_buzzer() → ioctl 控制硬件 + 刷新界面
    ▶ setReportingFrequency → 改 iot_state.upload_interval(下次循环生效)
```

### 7.5 天气数据流(上行)

```
mysend 线程每 upload_interval 秒:
    g_weather ──▶ cJSON_CreateObject() 组装
    ▶ cJSON_PrintUnformatted() 单行 JSON + "\n"
    ▶ send() 上报 Ubuntu ──▶ iot_ui_note_upload() 记录上报时间
```

### 7.6 硬件控制

```
控制界面按钮回调 → iot_ui_set_led()/iot_ui_set_buzzer()
    ▶ 更新 iot_state + 刷新界面
    ▶ led_set()/beep_set() → ioctl(fd, 命令, 参数) → /dev/Led、/dev/beep
```

---

## 八、重点函数解析

| 函数 | 所在文件 | 作用 |
|------|----------|------|
| `main()` | main.c | 启动入口:时区 → LVGL/显示/输入初始化 → 启动网络/NTP 线程 → 开机动画 → 建界面 → 主循环 |
| `custom_tick_get()` | main.c | 基于 `gettimeofday` 的实现节拍函数(毫秒级,LVGL 时钟源) |
| `iot_ui_init()` | src/ui/iot_ui.c | 初始化入口:打开 LED/蜂鸣器设备、注册 5 个界面、开机查一次广州天气、刷新并显示主界面 |
| `iot_ui_refresh()` | src/ui/iot_ui.c | 遍历 5 个界面的控件登记表 `iot_refs_*`,统一刷新所有动态控件 |
| `iot_ui_apply_refs()` | src/ui/iot_ui.c | 单个界面的刷新核心:按 `iot_state` 更新颜色/文字(云端、LED、蜂鸣器、天气、上报、按钮) |
| `iot_ui_make_header/card/status_row/dot/back_btn()` | src/ui/iot_ui.c | 公共控件工厂,被 5 个界面复用 |
| `forecast(char *name)` | src/network/forecast.c | HTTP 查实时天气:DNS → TCP 连接 → GET 请求 → 逐字节读头部 → 读正文 → cJSON 解析 → 填 `g_weather` |
| `forecast_daily(char *name)` | src/network/forecast.c | HTTP 查未来 10 天:解析 `daily` 数组,降水概率小数转百分比 |
| `parse(char *res,...)` | src/network/forecast.c | 解析 HTTP 响应状态码(200~299 成功)与 `Content-Length` |
| `net_client_start()` | src/network/net_client.c | 创建 `mysend` 后台线程 |
| `mysend()` | src/network/net_client.c | TCP 客户端线程:socket→connect→循环{上报天气 → select 收指令 → 攒行解析} |
| `handle_cmd(const char *line)` | src/network/net_client.c | 解析云端指令 JSON,分发到 LED/蜂鸣器/上报间隔处理 |
| `paras_int()` | src/network/net_client.c | 从参数对象按多个候选键取整数值(兼容真/假/数字/字符串) |
| `get_ntp_time(const char *server)` | src/network/ntp_client.c | UDP 发 48 字节 NTP 请求包,收时间戳,`ntohl` 解字节序,转 UNIX 时间戳(减 2208988800) |
| `ntp_sync_system_time()` | src/network/ntp_timer.c | 拿 NTP 时间后 `settimeofday()` 写入系统时钟 |
| `led_set()` / `beep_set()` | src/hardware/led_beep.c | ioctl 控制 LED / 蜂鸣器开关 |
| `led_beep_init()` | src/hardware/led_beep.c | 打开 `/dev/Led`、`/dev/beep` 设备节点 |
| `Socket()/Connect()/…` | src/common/wrap.c | socket 系统调用错误包装(打印 perror),供网络层复用 |

---

## 九、关键数据结构与全局变量

### 9.1 设备状态 `iot_state`(`iot_ui_state_t`,src/ui/iot_ui.h)

界面显示数据的唯一数据源,外部模块通过 `iot_ui_set_*()` 修改:

```c
typedef struct {
    bool     led_on;            /* LED 开关 */
    bool     buzzer_on;         /* 蜂鸣器开关 */
    bool     cloud_connected;   /* 云端连接 */
    int16_t  temperature;       /* 温度(℃),==IOT_TEMP_UNKNOWN 显示 "--" */
    char     weather_desc[16];  /* 天气描述,如 "晴" "多云" "小雨" */
    char     city[16];          /* 城市 */
    int16_t  feels_like;        /* 体感温度(℃),<0 显示 "--" */
    int16_t  humidity;          /* 相对湿度(%),<0 显示 "--" */
    int16_t  visibility;        /* 能见度(km),<0 显示 "--" */
    char     wind_direction[16];/* 风向文字,如 "东南风" */
    int16_t  wind_degree;       /* 风向角度(度),<0 显示 "--" */
    char     last_cmd[64];      /* 云端下发的最后一条命令 */
    char     last_upload[16];   /* 最近一次上报时间 "HH:MM:SS" */
    uint16_t upload_interval;   /* 数据上报间隔(秒) */
} iot_ui_state_t;

extern iot_ui_state_t iot_state;
```

### 9.2 天气查询结果

```c
/* 实时天气(src/network/forecast.h) */
typedef struct {
    int   temp;                 /* 温度(℃) */
    char  text[32];             /* 天气描述 */
    char  city[32];             /* 城市名 */
    int   feels_like;           /* 体感温度 */
    int   humidity;             /* 相对湿度 */
    int   visibility;           /* 能见度(km) */
    char  wind_direction[16];   /* 风向文字 */
    int   wind_degree;          /* 风向角度 */
} weather_t;
extern weather_t g_weather;

/* 未来10天(src/network/forecast.h) */
typedef struct {
    char  date[12];             /* 日期 "2026-08-20" */
    char  text_day[16];         /* 白天天气现象 */
    int   high, low;            /* 最高/最低温 */
    int   precip;               /* 降水概率(%) */
} forecast_day_t;
typedef struct {
    int            count;       /* 实际返回天数 */
    char           city[32];
    forecast_day_t day[10];     /* FORECAST_MAX_DAYS */
} forecast10_t;
extern forecast10_t g_forecast10;
```

### 9.3 界面控件登记表 `iot_ui_refs_t`(src/ui/iot_ui.h)

一个结构体把每个界面上**需要动态刷新**的控件指针集中登记,`iot_ui_refresh()` 遍历它刷新。每个界面一个实例(`iot_refs_home`/`iot_refs_control`/`iot_refs_state`/`iot_refs_weather`/`iot_refs_future`):

```c
typedef struct {
    lv_obj_t *cloud_dot;   lv_obj_t *cloud_text;   /* 顶栏云端状态 */
    lv_obj_t *led_dot;     lv_obj_t *led_text;     /* 设备状态行 */
    lv_obj_t *buzzer_dot;  lv_obj_t *buzzer_text;
    lv_obj_t *conn_dot;    lv_obj_t *conn_text;    /* 云端连接行 */
    lv_obj_t *city_text;   lv_obj_t *temp_text; lv_obj_t *weather_text;
    lv_obj_t *feels_like_text; lv_obj_t *humidity_text;
    lv_obj_t *visibility_text; lv_obj_t *wind_text;
    lv_obj_t *upload_text; lv_obj_t *upload_time; lv_obj_t *upload_interval;
    lv_obj_t *cmd_text;
    lv_obj_t *led_btn;     lv_obj_t *led_btn_dot; lv_obj_t *led_btn_text;
    lv_obj_t *buzzer_btn;  lv_obj_t *buzzer_btn_dot; lv_obj_t *buzzer_btn_text;
} iot_ui_refs_t;
```

### 9.4 屏幕对象

```c
extern lv_obj_t *g_scr_home;     /* 主界面 */
extern lv_obj_t *g_scr_control;  /* 控制界面 */
extern lv_obj_t *g_scr_state;    /* 状态界面 */
extern lv_obj_t *g_scr_weather;  /* 天气查询界面 */
extern lv_obj_t *g_scr_future;   /* 未来10天界面 */
```

### 9.5 关键常量/宏

```c
#define IOT_TEMP_UNKNOWN ((int16_t)-32768)  /* 温度"无数据"标记 → "--°C" */
#define IOT_VAL_UNKNOWN  ((int16_t)-1)      /* 其它数值"无数据"标记 → "--" */
#define DISP_BUF_SIZE (480 * 800)           /* LVGL 显存缓冲区大小(全屏一帧) */
#define FORECAST_MAX_DAYS 10                /* 未来预报最大天数 */
/* 界面配色 iot_ui.h 中统一定义 IOT_COL_ON/RED/OFF/ON_BG/OFF_BG/TXT/TXT2/ACC */
```

---

## 十、工程文件说明

| 文件/目录 | 行数(约) | 说明 |
|-----------|----------|------|
| `main.c` | 116 | 启动入口、显示/输入初始化、线程启动、开机动画、LVGL 主循环 |
| `src/ui/iot_ui.c` | 449 | 共享状态、公共控件工厂、界面注册/切换、统一刷新 |
| `src/ui/mian_UI.c` | 214 | 主界面布局(时钟/天气/详细/导航/滚动公告) |
| `src/ui/weather_UI.c` | 97 | 天气查询界面(8 城市选择) |
| `src/ui/future_UI.c` | 129 | 未来 10 天天气界面 |
| `src/ui/control_UI.c` | 76 | 控制界面(LED/蜂鸣器开关) |
| `src/ui/state_UI.c` | 35 | 状态界面(设备状态/云端通信) |
| `src/network/forecast.c` | 392 | 心知天气 HTTP 查询(实时+每日),结果导出 |
| `src/network/net_client.c` | 261 | TCP 上报线程 + 云端指令解析 |
| `src/network/ntp_client.c` | 109 | NTP 协议客户端(48 字节请求包) |
| `src/network/ntp_timer.c` | 57 | NTP 时间写入系统时钟 |
| `src/hardware/led_beep.c` | 60 | LED/蜂鸣器 ioctl 驱动 |
| `src/common/wrap.c` | 125 | socket 系统调用错误包装 |
| `src/lib/cJSON.c` | 750 | JSON 解析/生成第三方库 |
| `Ubuntu服务器.c` | 140 | PC 端 TCP 服务器,接收板端天气并打印(拆包/粘包处理) |
| `Makefile` | — | 交叉编译脚本,自动收集源码,`.o` 输出到 `obj/` |

---

*文档生成时间:2026-08-19。*
