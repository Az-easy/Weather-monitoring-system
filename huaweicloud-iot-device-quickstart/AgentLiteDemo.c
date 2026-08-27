#include "stdio.h"
#include "signal.h"

#if defined(WIN32) || defined(WIN64)
#include "windows.h"
#endif

#include "pthread.h"

#include <math.h>
#include "hw_type.h"
#include "iota_init.h"
#include "iota_cfg.h"

#include "LogUtil.h"
#include "JsonUtil.h"
#include "StringUtil.h"
#include "iota_login.h"
#include "iota_datatrans.h"
#include "string.h"
#include "cJSON.h"
#include "sys/types.h"
#include "unistd.h"
#include "iota_error_type.h"
#include <malloc.h>

#include <sys/socket.h>
#include <netinet/in.h> //man 3 inet_addr
#include <arpa/inet.h>

#define SERVER_IP "192.168.44.134" // ubuntu MY_IP
#define SERVER_PORT 60000

/* if you want to use syslog,you should do this:
 *
 * #include "syslog.h"
 * #define _SYS_LOG
 *
 * */

char *workPath = ".";
char *gatewayId = NULL;

int alarmValue = 0;

char *serverIp_ = "";
int port_ = 8883;

char *username_ = "600a9852942d2302db444b53_12334353"; // deviceId
char *password_ = "78b662105979a77dedcd8ad351c7563f";

int disconnected_ = 0;

char *subDeviceId = "f6cd4bbb1a8ab53acbb595efd0e90199_ABC123456789";

int sleepTime = 5000;

/* myrecv 线程 accept 到的板子套接字:云端指令回调 handleCommandRequest()
 * 通过它把下发的指令转发给板子(和天气上行复用同一条 TCP 连接) */
int g_client_sock = -1;

/* 云端(MQTT)在线状态:handleAuthSuccess 置 1,连接丢失/失败置 0。
 * 通过 notify_board_cloud() 推给板子,板端据此刷新"云端在线/离线"。 */
int g_cloud_online = 0;

/* 把当前云端在线状态推给板子(一行 JSON 以 \n 结尾,板端 handle_cmd 解析)。
 * 板子还没连上来时跳过(板子连上后 accept 处会再补发一次)。 */
static void notify_board_cloud(void)
{
	if (g_client_sock >= 0)
	{
		char buf[64];
		int n = snprintf(buf, sizeof(buf), "{\"type\":\"status\",\"cloud\":%d}\n", g_cloud_online);
		if (n > 0)
			send(g_client_sock, buf, (size_t)n, 0);
	}
}

void timeSleep(int ms)
{
#if defined(WIN32) || defined(WIN64)
	Sleep(ms);
#else
	usleep(ms * 1000);
#endif
}

//------------------------------------------------------Test  data report---------------------------------------------------------------------

void Test_messageReport()
{
	int messageId = IOTA_MessageReport(NULL, "data123", "123", "hello");
	if (messageId != 0)
	{
		printfLog(EN_LOG_LEVEL_ERROR, "AgentLiteDemo: Test_messageReport() failed, messageId %d\n", messageId);
	}
}

void Test_propertiesReport()
{
	int serviceNum = 1; // ����Ҫ�ϱ���service����
	ST_IOTA_SERVICE_DATA_INFO services[serviceNum];

	cJSON *root;
	root = cJSON_CreateObject();

	cJSON *weather_obj = cJSON_CreateObject();
	cJSON_AddStringToObject(weather_obj, "city", "北京");
	cJSON_AddNumberToObject(weather_obj, "temp", 26);
	cJSON_AddStringToObject(weather_obj, "text", "多云");

	cJSON_AddNumberToObject(root, "weather", 0);


	char *payload;
	payload = cJSON_Print(root);
	cJSON_Delete(root);

	services[0].event_time = getEventTimeStamp(); // if event_time is set to NULL, the time will be the iot-platform's time.
	services[0].service_id = "smokeDetector";
	services[0].properties = payload;

	int messageId = IOTA_PropertiesReport(services, serviceNum);
	if (messageId != 0)
	{
		printfLog(EN_LOG_LEVEL_ERROR, "AgentLiteDemo: Test_batchPropertiesReport() failed, messageId %d\n", messageId);
	}
	free(payload);
}

void Test_batchPropertiesReport()
{
	int deviceNum = 1;							 // Ҫ�ϱ������豸�ĸ���
	ST_IOTA_DEVICE_DATA_INFO devices[deviceNum]; // ���豸Ҫ�ϱ��Ľṹ������
	int serviceList[deviceNum];					 // ��Ӧ�洢ÿ�����豸Ҫ�ϱ��ķ������
	serviceList[0] = 2;							 // ���豸һҪ�ϱ���������
	//	serviceList[1] = 1;		  //���豸��Ҫ�ϱ�һ������

	char *device1_service1 = "{\"Load\":\"1\",\"ImbA_strVal\":\"3\"}"; // service1Ҫ�ϱ����������ݣ�������json��ʽ

	char *device1_service2 = "{\"PhV_phsA\":\"2\",\"PhV_phsB\":\"4\"}"; // service2Ҫ�ϱ����������ݣ�������json��ʽ

	devices[0].device_id = subDeviceId;
	devices[0].services[0].event_time = getEventTimeStamp();
	devices[0].services[0].service_id = "parameter";
	devices[0].services[0].properties = device1_service1;

	devices[0].services[1].event_time = getEventTimeStamp();
	devices[0].services[1].service_id = "analog";
	devices[0].services[1].properties = device1_service2;

	//	char *device2_service1 = "{\"AA\":\"2\",\"BB\":\"4\"}";
	//	devices[1].device_id = "subDevices22222";
	//	devices[1].services[0].event_time = "d2s1";
	//	devices[1].services[0].service_id = "device2_service11111111";
	//	devices[1].services[0].properties = device2_service1;

	int messageId = IOTA_BatchPropertiesReport(devices, deviceNum, serviceList);
	if (messageId != 0)
	{
		printfLog(EN_LOG_LEVEL_ERROR, "AgentLiteDemo: Test_batchPropertiesReport() failed, messageId %d\n", messageId);
	}
}

void Test_commandResponse(char *requestId)
{
	char *pcCommandRespense = "{\"cmdRsp\":\"success\"}"; // just test response

	int result_code = 0;
	char *response_name = NULL;

	int messageId = IOTA_CommandResponse(requestId, result_code, response_name, pcCommandRespense);
	if (messageId != 0)
	{
		printfLog(EN_LOG_LEVEL_ERROR, "AgentLiteDemo: Test_commandResponse() failed, messageId %d\n", messageId);
	}
}

void Test_propSetResponse(char *requestId)
{
	int messageId = IOTA_PropertiesSetResponse(requestId, 0, "success");
	if (messageId != 0)
	{
		printfLog(EN_LOG_LEVEL_ERROR, "AgentLiteDemo: Test_propSetResponse() failed, messageId %d\n", messageId);
	}
}

void Test_propGetResponse(char *requestId)
{
	int serviceNum = 2;
	ST_IOTA_SERVICE_DATA_INFO serviceProp[serviceNum];

	char *property = "{\"Load\":\"5\",\"ImbA_strVal\":\"6\"}";

	serviceProp[0].event_time = getEventTimeStamp();
	serviceProp[0].service_id = "parameter";
	serviceProp[0].properties = property;

	char *property2 = "{\"PhV_phsA\":\"2\",\"PhV_phsB\":\"4\"}";

	serviceProp[1].event_time = getEventTimeStamp();
	serviceProp[1].service_id = "analog";
	serviceProp[1].properties = property2;

	int messageId = IOTA_PropertiesGetResponse(requestId, serviceProp, serviceNum);
	if (messageId != 0)
	{
		printfLog(EN_LOG_LEVEL_ERROR, "AgentLiteDemo: Test_propGetResponse() failed, messageId %d\n", messageId);
	}
}

//--------------------------------------------------------------------------------------------------------------------------------------------------

void handleAuthSuccess(void *context, int messageId, int code, char *message)
{
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleConnectSuccess(), login success\n");
	disconnected_ = 0;
	g_cloud_online = 1;     /* 云端已在线:同步给板子 */
	notify_board_cloud();
}

void handleAuthFailure(void *context, int messageId, int code, char *message)
{
	printfLog(EN_LOG_LEVEL_ERROR, "AgentLiteDemo: handleConnectFailure() error, messageId %d, code %d, messsage %s\n", messageId, code, message);
	g_cloud_online = 0;     /* 云端掉线:同步给板子 */
	notify_board_cloud();
	// judge if the network is available etc. and login again
	//...
	printfLog(EN_LOG_LEVEL_ERROR, "AgentLiteDemo: handleConnectFailure() login again\n");
	int ret = IOTA_Connect();
	if (ret != 0)
	{
		printfLog(EN_LOG_LEVEL_ERROR, "AgentLiteDemo: handleConnectionLost() error, login again failed, result %d\n", ret);
	}
}

void handleConnectionLost(void *context, int messageId, int code, char *message)
{
	printfLog(EN_LOG_LEVEL_WARNING, "AgentLiteDemo: handleConnectionLost() warning, messageId, code %d, messsage %s\n", messageId, code, message);
	g_cloud_online = 0;     /* 云端掉线:同步给板子 */
	notify_board_cloud();
	// judge if the network is available etc. and login again
	//...
	int ret = IOTA_Connect();
	if (ret != 0)
	{
		printfLog(EN_LOG_LEVEL_ERROR, "AgentLiteDemo: handleConnectionLost() error, login again failed, result %d\n", ret);
	}
}

void handleDisAuthSuccess(void *context, int messageId, int code, char *message)
{
	disconnected_ = 1;

	printf("AgentLiteDemo: handleLogoutSuccess, login again\n");
	printf("AgentLiteDemo: handleDisAuthSuccess(), messageId %d, code %d, messsage %s\n", messageId, code, message);
}

void handleDisAuthFailure(void *context, int messageId, int code, char *message)
{
	printf("AgentLiteDemo: handleLogoutFailure, login again\n");
	int ret = IOTA_Connect();
	if (ret != 0)
	{
		printfLog(EN_LOG_LEVEL_ERROR, "AgentLiteDemo: handleConnectionLost() error, login again failed, result %d\n", ret);
	}
	printf("AgentLiteDemo: handleDisAuthFailure(), messageId %d, code %d, messsage %s\n", messageId, code, message);
}

void handleSubscribesuccess(void *context, int messageId, int code, char *message)
{
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleSubscribesuccess() messageId %d\n", messageId);
}

void handleSubscribeFailure(void *context, int messageId, int code, char *message)
{
	printfLog(EN_LOG_LEVEL_WARNING, "AgentLiteDemo: handleSubscribeFailure() warning, messageId %d, code %d, messsage %s\n", messageId, code, message);
}

void handlePublishSuccess(void *context, int messageId, int code, char *message)
{
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handlePublishSuccess() messageId %d\n", messageId);
}

void handlePublishFailure(void *context, int messageId, int code, char *message)
{
	printfLog(EN_LOG_LEVEL_WARNING, "AgentLiteDemo: handlePublishFailure() warning, messageId %d, code %d, messsage %s\n", messageId, code, message);
}

//-------------------------------------------handle  message   arrived------------------------------------------------------------------------------

void handleMessageDown(void *context, int messageId, int code, char *message)
{
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleMessageDown(), messageId %d, code %d, messsage %s\n", messageId, code, message);

	JSON *root = JSON_Parse(message); // Convert string to JSON

	char *content = JSON_GetStringFromObject(root, "content", "-1"); // get value of content
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleMessageDown(), content %s\n", content);

	char *object_device_id = JSON_GetStringFromObject(root, "object_device_id", "-1"); // get value of object_device_id
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleMessageDown(), object_device_id %s\n", object_device_id);

	char *name = JSON_GetStringFromObject(root, "name", "-1"); // get value of name
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleMessageDown(), name %s\n", name);

	char *id = JSON_GetStringFromObject(root, "id", "-1"); // get value of id
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleMessageDown(), id %s\n", id);

	JSON_Delete(root);
}

void handleCommandRequest(void *context, int messageId, int code, char *message, char *requestId)
{
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleCommandRequest(), messageId %d, code %d, messsage %s, requestId %s\n", messageId, code, message, requestId);

	JSON *root = JSON_Parse(message); // Convert string to JSON

	char *object_device_id = JSON_GetStringFromObject(root, "object_device_id", "-1"); // get value of object_device_id
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleCommandRequest(), object_device_id %s\n", object_device_id);

	char *service_id = JSON_GetStringFromObject(root, "service_id", "-1"); // get value of service_id
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleCommandRequest(), content %s\n", service_id);

	char *command_name = JSON_GetStringFromObject(root, "command_name", "-1"); // get value of command_name
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleCommandRequest(), name %s\n", command_name);

	JSON *paras = JSON_GetObjectFromObject(root, "paras"); // get value of data
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleCommandRequest(), id %s\n", paras);

	if (paras)
	{
		sleepTime = JSON_GetIntFromObject(paras, "value", 1) * 1000;

		printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleCommandRequest(), sleepTime %d\n", sleepTime);
	}

	/* —— 把云端指令转发给板子 ——
	 * 通过 myrecv 线程 accept 到的那条 TCP 连接(g_client_sock)发给板子。
	 * 云端命令消息本身是单行 JSON,原样包一层 {"type":"cmd","data":...} 发过去,
	 * 以 \n 结尾当消息边界;板子收到后按 command_name 自己匹配,这里不写死命令名。 */
	if (g_client_sock >= 0)
	{
		char cmd_buf[1024];
		int n = snprintf(cmd_buf, sizeof(cmd_buf),
		                 "{\"type\":\"cmd\",\"data\":%s}\n",
		                 (message != NULL) ? message : "{}");
		if (n > 0)
		{
			int sret = send(g_client_sock, cmd_buf, (size_t)n, 0);
			printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: 已把命令[%s]转发给板子, ret=%d\n", command_name, sret);
		}
	}

	Test_commandResponse(requestId); // command reponse

	JSON_Delete(root);
}

void handlePropertiesSet(void *context, int messageId, int code, char *message, char *requestId)
{
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handlePropertiesSet(), messageId %d, code %d, messsage %s, requestId %s\n", messageId, code, message, requestId);

	JSON *root = JSON_Parse(message); // Convert string to JSON

	char *object_device_id = JSON_GetStringFromObject(root, "object_device_id", "-1"); // get value of object_device_id
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handlePropertiesSet(), object_device_id %s\n", object_device_id);

	JSON *services = JSON_GetObjectFromObject(root, "services"); // get  services array
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handlePropertiesSet(), services %s\n", services);

	int dataSize = JSON_GetArraySize(services); // get length of services array
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handlePropertiesSet(), dataSize %d\n", dataSize);

	if (dataSize > 0)
	{
		JSON *service = JSON_GetObjectFromArray(services, 0); // only get the first one to demonstrate
		printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleSubDeviceMessageDown(), service %s\n", service);
		if (service)
		{
			char *service_id = JSON_GetStringFromObject(service, "service_id", NULL);

			printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handlePropertiesSet(), service_id %s\n", service_id);

			JSON *properties = JSON_GetObjectFromObject(service, "properties");

			alarmValue = JSON_GetIntFromObject(properties, "alarm", NULL);
			printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handlePropertiesSet(), alarmValue %d\n", alarmValue);
		}
	}

	Test_propSetResponse(requestId); // command response

	JSON_Delete(root);
}

void handlePropertiesGet(void *context, int messageId, int code, char *message, char *requestId)
{
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handlePropertiesGet(), messageId %d, code %d, messsage %s, requestId %s\n", messageId, code, message, requestId);

	JSON *root = JSON_Parse(message);

	char *object_device_id = JSON_GetStringFromObject(root, "object_device_id", "-1"); // get value of object_device_id
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handlePropertiesGet(), object_device_id %s\n", object_device_id);

	char *service_id = JSON_GetStringFromObject(root, "service_id", "-1"); // get value of service_id
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handlePropertiesGet(), service_id %s\n", service_id);

	Test_propGetResponse(requestId); // command response

	JSON_Delete(root);
}

void handleDeviceProGetResp(void *context, int messageId, int code, char *message, char *requestId)
{
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleDeviceProGetResp(), messageId %d, code %d, messsage %s, requestId %s\n", messageId, code, message, requestId);
	// start to reponse  command  by the topic  of $oc/devices/{device_id}/sys/commands/response/request_id={request_id}
}

void handleEventsDown(void *context, int messageId, int code, char *message)
{
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleSubDeviceMessageDown(), messageId %d, code %d, messsage %s\n", messageId, code, message);

	// the demo of how to get the parameter
	JSON *root = JSON_Parse(message);

	char *object_device_id = JSON_GetStringFromObject(root, "object_device_id", "-1"); // get value of object_device_id
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), object_device_id %s\n", object_device_id);

	JSON *service = JSON_GetObjectFromObject(root, "services"); // get object of services
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), services %s\n", service);

	int dataSize = JSON_GetArraySize(service); // get size of services
	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), dataSize %d\n", dataSize);

	if (dataSize > 0)
	{
		JSON *serviceEvent = JSON_GetObjectFromArray(service, 0); // get object of ServiceEvent
		printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), serviceEvent %s\n", serviceEvent);
		if (serviceEvent)
		{
			char *service_id = JSON_GetStringFromObject(serviceEvent, "service_id", NULL); // get value of service_id
			printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), service_id %s\n", service_id);

			char *event_type = NULL;												 // To determine whether to add or delete a sub device
			event_type = JSON_GetStringFromObject(serviceEvent, "event_type", NULL); // get value of event_type
			printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), event_type %s\n", event_type);

			char *event_time = JSON_GetStringFromObject(serviceEvent, "event_time", NULL); // get value of event_time
			printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), event_time %s\n", event_time);

			JSON *paras = JSON_GetObjectFromObject(serviceEvent, "paras"); // get object of paras
			printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), paras %s\n", paras);

			JSON *devices = JSON_GetObjectFromObject(paras, "devices"); // get object of devices
			printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), devices %s\n", devices);

			int version = JSON_GetIntFromObject(paras, "version", -1); // get value of version
			printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), version %d\n", version);

			int devicesSize = JSON_GetArraySize(devices); // get size of devicesSize
			printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), devicesSize %d\n", devicesSize);

			if (devicesSize > 0)
			{
				JSON *deviceInfo = JSON_GetObjectFromArray(devices, 0); // get object of deviceInfo
				printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), deviceInfo %s\n", deviceInfo);

				char *parent_device_id = JSON_GetStringFromObject(serviceEvent, "parent_device_id", NULL); // get value of parent_device_id
				printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), parent_device_id %s\n", parent_device_id);

				char *node_id = JSON_GetStringFromObject(serviceEvent, "node_id", NULL); // get value of node_id
				printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), node_id %s\n", node_id);

				subDeviceId = JSON_GetStringFromObject(serviceEvent, "device_id", NULL); // get value of device_id
				printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), device_id %s\n", subDeviceId);

				if (!strcmp(event_type, "add_sub_device_notify")) // add a sub device
				{
					char *name = JSON_GetStringFromObject(serviceEvent, "name", NULL); // get value of name
					printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), name %s\n", name);

					char *description = JSON_GetStringFromObject(serviceEvent, "description", NULL); // get value of description
					printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), description %s\n", description);

					char *manufacturer_id = JSON_GetStringFromObject(serviceEvent, "manufacturer_id", NULL); // get value of manufacturer_id
					printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), manufacturer_id %s\n", manufacturer_id);

					char *model = JSON_GetStringFromObject(serviceEvent, "model", NULL); // get value of model
					printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), model %s\n", model);

					char *product_id = JSON_GetStringFromObject(serviceEvent, "product_id", NULL); // get value of product_id
					printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), product_id %s\n", product_id);

					char *fw_version = JSON_GetStringFromObject(serviceEvent, "fw_version", NULL); // get value of fw_version
					printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), fw_version %s\n", fw_version);

					char *sw_version = JSON_GetStringFromObject(serviceEvent, "sw_version", NULL); // get value of sw_version
					printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), sw_version %s\n", sw_version);

					char *status = JSON_GetStringFromObject(serviceEvent, "status", NULL); // get value of status
					printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), status %s\n", status);

					Test_batchPropertiesReport(); // command response
				}
				else if (!strcmp(event_type, "delete_sub_device_notify")) // delete a sub device
				{
					printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), delete a sub device.\n");
				}
				else
				{
					printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: handleEventsDown(), default.\n");
				}
			}
		}
	}

	JSON_Delete(root);
}

void setConnectConfig()
{

	FILE *file;
	long length;
	char *content;
	cJSON *json;

	file = fopen("./ClientConf.json", "rb");
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fseek(file, 0, SEEK_SET);
	content = (char *)malloc(length + 1);
	fread(content, 1, length, file);
	fclose(file);

	json = cJSON_Parse(content);

	username_ = JSON_GetStringFromObject(json, "deviceId", NULL);
	password_ = JSON_GetStringFromObject(json, "secret", NULL);
	char *url = JSON_GetStringFromObject(json, "serverUri", NULL);

	deleteSubStr(url, "ssl://");
	deleteSubStr(url, ":8883");

	serverIp_ = url;
}

//----------------------------------------------------------------------------------------------------------------------------------------------

void setAuthConfig()
{
	IOTA_ConfigSetStr(EN_IOTA_CFG_MQTT_ADDR, serverIp_);
	IOTA_ConfigSetUint(EN_IOTA_CFG_MQTT_PORT, port_);
	IOTA_ConfigSetStr(EN_IOTA_CFG_DEVICEID, username_);
	IOTA_ConfigSetStr(EN_IOTA_CFG_DEVICESECRET, password_);

#ifdef _SYS_LOG
	IOTA_ConfigSetUint(EN_IOTA_CFG_LOG_LOCAL_NUMBER, LOG_LOCAL7);
	IOTA_ConfigSetUint(EN_IOTA_CFG_LOG_LEVEL, LOG_INFO);
#endif
}

void setMyCallbacks()
{
	IOTA_SetCallback(EN_IOTA_CALLBACK_CONNECT_SUCCESS, handleAuthSuccess);
	IOTA_SetCallback(EN_IOTA_CALLBACK_CONNECT_FAILURE, handleAuthFailure);
	IOTA_SetCallback(EN_IOTA_CALLBACK_CONNECTION_LOST, handleConnectionLost);

	IOTA_SetCallback(EN_IOTA_CALLBACK_DISCONNECT_SUCCESS, handleDisAuthSuccess);
	IOTA_SetCallback(EN_IOTA_CALLBACK_DISCONNECT_FAILURE, handleDisAuthFailure);

	IOTA_SetCallback(EN_IOTA_CALLBACK_SUBSCRIBE_SUCCESS, handleSubscribesuccess);
	IOTA_SetCallback(EN_IOTA_CALLBACK_SUBSCRIBE_FAILURE, handleSubscribeFailure);

	IOTA_SetCallback(EN_IOTA_CALLBACK_PUBLISH_SUCCESS, handlePublishSuccess);
	IOTA_SetCallback(EN_IOTA_CALLBACK_PUBLISH_FAILURE, handlePublishFailure);

	IOTA_SetCallback(EN_IOTA_CALLBACK_MESSAGE_DOWN, handleMessageDown);
	IOTA_SetCallbackWithTopic(EN_IOTA_CALLBACK_COMMAND_REQUEST, handleCommandRequest);
	IOTA_SetCallbackWithTopic(EN_IOTA_CALLBACK_PROPERTIES_SET, handlePropertiesSet);
	IOTA_SetCallbackWithTopic(EN_IOTA_CALLBACK_PROPERTIES_GET, handlePropertiesGet);
	IOTA_SetCallback(EN_IOTA_CALLBACK_SUB_DEVICE_MESSAGE_DOWN, handleEventsDown);
}

void myPrintLog(int level, char *format, va_list args)
{
	vprintf(format, args); // ��־��ӡ�ڿ���̨
						   /*if you want to printf log in system log files,you can do this:
							*
							* vsyslog(level, format, args);
							* */
}
/* 解析板端发来的一行天气 JSON(板端 cJSON_Print 输出,以 \n 结尾)并打印 */
static void handle_weather(const char *json)
{
    cJSON *root1 = cJSON_Parse(json);
    if(root1 == NULL)
    {
        printf("JSON 解析失败:%s\n", json);
        return;
    }

    /* cJSON_GetObjectItem 拿到空会返回 NULL,先判空再取值,
     * 否则对空指针取成员会段错误(和板端 forecast.c 同一个套路)。 */
    cJSON *city = cJSON_GetObjectItem(root1, "city");
    cJSON *text = cJSON_GetObjectItem(root1, "text");
    cJSON *temp = cJSON_GetObjectItem(root1, "temp");
    cJSON *feels = cJSON_GetObjectItem(root1, "feels_like");
    cJSON *hum  = cJSON_GetObjectItem(root1, "humidity");
    cJSON *vis  = cJSON_GetObjectItem(root1, "visibility");
    cJSON *wind = cJSON_GetObjectItem(root1, "wind_direction");
    cJSON *deg  = cJSON_GetObjectItem(root1, "wind_degree");
    cJSON *led  = cJSON_GetObjectItem(root1, "led");
    cJSON *beep = cJSON_GetObjectItem(root1, "beep");

    printf("收到天气: 城市=%s 天气=%s 温度=%d℃ 体感=%d℃ 湿度=%d%% 能见度=%dkm 风向=%s 角度=%d° LED=%s 蜂鸣器=%s\n",
           (city && city->valuestring) ? city->valuestring : "?",
           (text && text->valuestring) ? text->valuestring : "?",
           temp ? temp->valueint : -1,
           feels ? feels->valueint : -1,
           hum ? hum->valueint : -1,
           vis ? vis->valueint : -1,
           (wind && wind->valuestring) ? wind->valuestring : "?",
           deg ? deg->valueint : -1,
           (led  && led->valueint)  ? "开" : "关",
           (beep && beep->valueint) ? "开" : "关");

    /* —— 上报华为云平台 ——
     * 天气字段拼成 JSON 子对象挂到 weather 属性下(cJSON 类型,不转字符串);
     * LED/蜂鸣器状态是模型里单独定义的属性,和 weather 平级上报。
     * service_id 要和你模型里那个服务名一致(示例模板里是 smokeDetector)。 */
    {
        ST_IOTA_SERVICE_DATA_INFO services[1];

        /* 1. 8 个天气字段拼成一个对象 */
        cJSON *w = cJSON_CreateObject();
        cJSON_AddStringToObject(w, "city",           (city && city->valuestring) ? city->valuestring : "");
        cJSON_AddStringToObject(w, "text",           (text && text->valuestring) ? text->valuestring : "");
        cJSON_AddNumberToObject(w, "temp",           temp ? temp->valueint : -1);
        cJSON_AddNumberToObject(w, "feels_like",     feels ? feels->valueint : -1);
        cJSON_AddNumberToObject(w, "humidity",       hum ? hum->valueint : -1);
        cJSON_AddNumberToObject(w, "visibility",     vis ? vis->valueint : -1);
        cJSON_AddStringToObject(w, "wind_direction", (wind && wind->valuestring) ? wind->valuestring : "");
        cJSON_AddNumberToObject(w, "wind_degree",    deg ? deg->valueint : -1);

        /* 2. 把 w 子对象直接挂到 weather 属性下(cJSON_AddItemToObject 接管 w,
         * 后面 cJSON_Delete(prop) 会一起释放,不要重复 free)。
         * 3. led/beep 是产品模型里单独定义的 bool 属性,和 weather 平级上报,
         *    只认 true/false,用 cJSON_AddBoolToObject 发布尔:
         *    {"weather":{...}, "led":true, "beep":false} */
        cJSON *prop = cJSON_CreateObject();
        if(w != NULL)
        {
            cJSON_AddItemToObject(prop, "weather", w);   /* {"weather":{...}} */
        }
        else
        {
            cJSON_AddItemToObject(prop, "weather", cJSON_CreateObject());
        }
        cJSON_AddBoolToObject(prop, "led",  (led  && led->valueint)  ? 1 : 0);
        cJSON_AddBoolToObject(prop, "beep", (beep && beep->valueint) ? 1 : 0);

        char *payload = cJSON_PrintUnformatted(prop);
        cJSON_Delete(prop);
        if(payload != NULL)
        {
            services[0].event_time = getEventTimeStamp();   /* 平台收到的时间戳 */
            services[0].service_id = "smokeDetector";       /* 你的产品模型里的服务名 */
            services[0].properties = payload;

            int mid = IOTA_PropertiesReport(services, 1);
            if(mid != 0)
                printfLog(EN_LOG_LEVEL_ERROR, "AgentLiteDemo: weather report failed, messageId %d\n", mid);
            free(payload);
        }
    }

    cJSON_Delete(root1);
}


void *myrecv(void *arg)
{

	// 1. 创建套接字(文件描述符) socket
	//   AF_INET:ipv4 SOCK_STREAM:tcp
	int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (socket_fd < 0)
	{
		printf("socket fail\n");
		return -1;
	}

	// 所以设置端口号可以复用,这两条语句放在 绑定bind 之前
	int optval = 1;
	setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

	// 2. 绑定本机IP和端口 bind
	struct sockaddr_in server_addr;					 // 难点1：小端转大端
	server_addr.sin_family = AF_INET;				 // IPv 地址族
	server_addr.sin_port = htons(SERVER_PORT);		 // 本机端口转网络端口
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY); // 本机IP转网络IP
	int ret = bind(socket_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
	if (ret != 0)
	{
		printf("bind fail[%s][%d]\n", SERVER_IP, SERVER_PORT);
		return -1;
	}
	printf("绑定本机成功[%s][%d]\n", SERVER_IP, SERVER_PORT);

	// 3. 监听 listen
	ret = listen(socket_fd, 20);
	if (ret != 0)
	{
		printf("listen fail\n");
		return -1;
	}

	// 4. 接收客户端的连接 accept
	int socket_client = -1;
	struct sockaddr_in client_addr; // 存放客户端的地址
	socklen_t addrlen = sizeof(struct sockaddr_in);
	// socket_client = accept(socket_fd,NULL,NULL); //简单用法：不知道谁上线
	socket_client = accept(socket_fd, (struct sockaddr *)&client_addr, (socklen_t *)&addrlen); // 复杂用法：知道谁上线
	if (socket_client < 0)
	{
		printf("accept fail\n");
		return -1;
	}

	// 解析客户端的ip和端口号:难点2 ：大端转小端
	char *ip = inet_ntoa(client_addr.sin_addr); // net to string  atoi网络ip转本机ip
	int port = ntohs(client_addr.sin_port);		// net to host short 网络端口转本机端口
	// printf("新的客户端上线[%d]\n", socket_client);
	printf("新的客户端上线[%d][%s][%d]\n", socket_client, ip, port);

	g_client_sock = socket_client; // 保存板子套接字:云端指令转发用
	notify_board_cloud();          // 板子刚上线,把当前云端在线状态同步给它(顶栏/状态界面不再一直离线)

	// 5. 接收客户端的数据 recv/read —— 板端每5秒发来一行天气 JSON(以 \n 结尾)
	char buf[1024] = {0};
	char line[1024] = {0}; // 攒一整行再解析(网络可能把一条消息拆成多段到达)
	int line_len = 0;
	while (1)
	{
		bzero(buf, sizeof(buf));
		ret = recv(socket_client, buf, sizeof(buf), 0);
		if (ret <= 0) // ret==0 掉线,ret<0 出错,都当断开处理
		{
			printf("客户端掉线[%d][%s][%d]\n", socket_client, ip, port);
			break;
		}

		/* 攒到 \n 才算一条完整 JSON,再解析(同时处理拆包和粘包) */
		for (int i = 0; i < ret; i++)
		{
			if (buf[i] == '\n')
			{
				line[line_len] = '\0';
				handle_weather(line);
				line_len = 0;
			}
			else if (line_len < (int)sizeof(line) - 1)
			{
				line[line_len++] = buf[i];
			}
		}
	}

	// 6. 关闭套接字 close
	g_client_sock = -1; // 板子掉线后,云端指令不再转发
	close(socket_fd);
	close(socket_client);

	return 0;
}

int main(int argc, char **argv)
{

	pthread_t thread;
	int ret = pthread_create(&thread, NULL, myrecv, NULL);
	if (ret != 0)
	{
		perror("thread fail");
		return -1;
	}

#if defined(_DEBUG)
	setvbuf(stdout, NULL, _IONBF, 0); // in order to make the console log printed immediately at debug mode
#endif

	IOTA_SetPrintLogCallback(myPrintLog);

	setConnectConfig();

	printfLog(EN_LOG_LEVEL_INFO, "AgentLiteDemo: start test ===================>\n");

	if (IOTA_Init(workPath) > 0)
	{
		printfLog(EN_LOG_LEVEL_ERROR, "AgentLiteDemo: IOTA_Init() error, init failed\n");
		return 1;
	}

	setAuthConfig();
	setMyCallbacks();

	// see handleLoginSuccess and handleLoginFailure for login result
	ret = IOTA_Connect();
	if (ret != 0)
	{
		printfLog(EN_LOG_LEVEL_ERROR, "AgentLiteDemo: IOTA_Auth() error, Auth failed, result %d\n", ret);
	}

	timeSleep(1500);
	int count = 0;
	while (count < 10000)
	{

		//        //message up
		//        Test_messageReport();

		// properties report
		// 真实天气已在 myrecv 线程的 handle_weather() 里收到板端数据后上报,
		// 这里不再报假的测试数据(北京/26/多云);想测上报功能再放开下面这行。
		// Test_propertiesReport();

		//        //batchProperties report
		//        Test_batchPropertiesReport();
		//
		//        //command response
		//        Test_commandResponse("1005");
		//
		//        timeSleep(1500);
		//
		//        //propSetResponse
		//        Test_propSetResponse("1006");
		//
		//        timeSleep(1500);
		//
		//        //propSetResponse
		//        Test_propGetResponse("1007");

		timeSleep(sleepTime);

		count++;
	}
	while (1)
	{
		timeSleep(50);
	}

	return 0;
}
