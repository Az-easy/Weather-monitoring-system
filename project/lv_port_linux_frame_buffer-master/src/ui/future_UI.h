#ifndef __FUTURE_UI_H__
#define __FUTURE_UI_H__

/* 注册(创建)未来10天天气界面:由 iot_ui_init() 启动时调用一次 */
void future_UI(void);

/* 查询当前城市未来10天天气并切换到该界面:由主界面「未来10天天气」按钮调用 */
void open_future_weather(void);

#endif /* __FUTURE_UI_H__ */
