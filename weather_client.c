//名称：天气HTTP客户端
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//Winsock.h是用于winsock的头文件，里面有多种函数可以帮助使用Winsock
#include <winsock2.h>
#include "cJSON.h"
#include "utf8togbk.h"

/* 调试开关 */ 
#define DEBUG 0 	// 1:调试版本（打开相关printf调试语句） 0：发布版本
 
/* 心知天气（www.seniverse.com）IP及端口 */ 
#define  WEATHER_IP_ADDR 	"116.62.81.138"
#define WEATHER_PORT 	80

/* 秘钥，注意！！如果要用这一份代码，这个一定要改为自己的，因为这个我已经故意改错了，防止有人与我公用一个KEY */
#define  KEY    "SLWyxL5MC23V315IT"		// 这是在心知天气注册后，每个用户自己的一个key

/* GET请求包 */
#define  GET_REQUEST_PACKAGE     \
         "GET https://api.seniverse.com/v3/weather/%s.json?key=%s&location=%s&language=zh-Hans&unit=c\r\n\r\n"
         
/* JSON数据包 */	
#define  NOW_JSON	"now"	
#define  DAILY_JSON   "daily"
//....还用更多其他的天气数据包可查阅心知天气         

/* 天气数据结构体 */
typedef struct{
	/* 实况天气数据 */
	char id[32];				//id
	char name[32];				//地名
	char country[32];			//国家
	char path[32];				//完整地名路径
	char timezone[32];			//时区
	char timezone_offset[32]; 	//时差
	char text[32];				//天气预报文字
	char code[32];				//天气预报代码
	char temperature[32];		//气温
	char last_update[32];		//最后一次更新时间
	
	/* 今天、明后、后天天气数据 */
	char date[3][32];			//日期
	char text_day[3][32]; 		//白天天气现象文字
	char code_day[3][32];		//白天天气现象代码
	char code_night[3][32];		//晚间天气现象代码
	char high[3][32];			//最高温
	char low[3][32];			//最低温
	char wind_direction[3][64];	//风向
	char wind_speed[3][32];		//风速
	char wind_scale[3][32];		//风力等级
}Weather; 

/* cmd窗口设置 */
struct cmd_windows_config
{
	int width;
	int high;
	int color;
};

/* cmd窗口默认配置 */
struct cmd_windows_config cmd_default_config =
{
	60,
	40,
	0xf0
};

//函数声明
static void GetWeather(char *weather_json, char *location, Weather *result);
static int cJSON_NowWeatherParse(char *JSON, Weather *result); 
static int cJSON_DailyWeatherParse(char *JSON, Weather *result);
static void DisplayWeather(Weather *weather_data);
static void cmd_window_set(struct cmd_windows_config *config);
static void printf_topic(void);

/*******************************************************************************************************
** 函数: main，主函数
**------------------------------------------------------------------------------------------------------
** 参数: void
** 返回: void
********************************************************************************************************/
int main(void){
	
	Weather weather_data={0};
	char location[32];
	memset(location,0,sizeof(location));
	
	cmd_window_set(&cmd_default_config);//配置cmd窗口
	printf_topic();
	
	while((1 == scanf("%s", location))){ // 读入地名拼音
		system("cls");	//清屏 
		memset(&weather_data, 0, sizeof(weather_data)); 		//清空数据榜之影响下一次读取 
		GetWeather(NOW_JSON, location, &weather_data); 		//获取此刻的天气 
		GetWeather(DAILY_JSON, location, &weather_data);	//获取今天，明天，后天的天气 
		
#if DEBUG		
		printf("看看是否接受到了数据:%s,%s,%s\n",weather_data.name,weather_data.path,weather_data.country);
#endif
	
		DisplayWeather(&weather_data);						//展示天气 
		printf("\n请输入要查询天气的城市名称的拼音（如：beijing）:"); 
	}
		
	return 0;
}

/*******************************************************************************************************
** 函数: GetWeather，获取天气数据并解析
**------------------------------------------------------------------------------------------------------
** 参数: weather_json：需要解析的json包   location：地名   result：数据解析的结果
** 返回: void
********************************************************************************************************/
static void GetWeather(char *weather_json, char *location, Weather *result){
	SOCKET ClientSock;
	WSADATA wd;
	char GetRequestBuf[256];
	char WeatherRecvBuf[2*1024];
	char GbkRecvBuf[2*1024];
	int gbk_recv_len=0;
	int connect_status=0;
	
	//对字符串清空，防止误操作 
	memset(GetRequestBuf,0,sizeof(GetRequestBuf)); 
	memset(WeatherRecvBuf,0,sizeof(WeatherRecvBuf)); 
	memset(GbkRecvBuf,0,sizeof(GbkRecvBuf)); 
	
	/* 初始化操作sock需要的DLL */ 
	WSAStartup(MAKEWORD(2,2),&wd);
	
	/* 设置要访问的服务器的信息 */ 
	SOCKADDR_IN ServerSockAddr;
	memset(&ServerSockAddr, 0, sizeof(ServerSockAddr)); 			//每个字节都用0填充 
	ServerSockAddr.sin_family = PF_INET;							//ipv4 
	ServerSockAddr.sin_addr.s_addr = inet_addr(WEATHER_IP_ADDR);	//心知天气服务器ip 
	ServerSockAddr.sin_port = htons(WEATHER_PORT);					//端口号
	
	/* 创建客户端socket */ 
	if((ClientSock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1){
		printf("socket error!\n");
		exit(1);
	} 
	
	/* 连接服务器 */
	if((connect_status = connect(ClientSock, (SOCKADDR*)&ServerSockAddr, sizeof(SOCKADDR))) == -1){
		printf("connect error!\n");
		exit(0);
	}
	
	/* 组合GET请求包 */
	sprintf(GetRequestBuf, GET_REQUEST_PACKAGE, weather_json, KEY, location);
	
	/* 发送数据到服务器哦 */
	send(ClientSock, GetRequestBuf, strlen(GetRequestBuf), 0);
	
	/* 接收服务端返回的数据 */
	recv(ClientSock, WeatherRecvBuf, 2*1024, 0);
	
	/* utf-8转为gbk */ 
	SwitchToGbk((const unsigned char*)WeatherRecvBuf, strlen((const char*)WeatherRecvBuf), (unsigned char*)GbkRecvBuf, &gbk_recv_len); 
#if DEBUG
	printf("服务端返回的数据为：%s\n", GbkRecvBuf);
#endif

	/* 解析天气数据并保存到结构体变量weather_data中 */
	if(strcmp(weather_json, NOW_JSON) == 0)				//天气实况 
		cJSON_NowWeatherParse(GbkRecvBuf, result);
	else if(strcmp(weather_json, DAILY_JSON)==0)		//未来三天天气 
		cJSON_DailyWeatherParse(GbkRecvBuf, result);
	
	//对字符串清空，防止误操作 
	memset(GetRequestBuf,0,sizeof(GetRequestBuf)); 
	memset(WeatherRecvBuf,0,sizeof(WeatherRecvBuf)); 
	memset(GbkRecvBuf,0,sizeof(GbkRecvBuf)); 
	
	/* 关闭套接字 */
	closesocket(ClientSock); 
	
	/* 终止使用DLL */
	WSACleanup();
}

/*******************************************************************************************************
** 函数: cJSON_NowWeatherParse，解析天气实况数据
**------------------------------------------------------------------------------------------------------
** 参数: JSON：天气数据包   result：数据解析的结果
** 返回: int -1代表失败 0代表成功 
********************************************************************************************************/
static int cJSON_NowWeatherParse(char *JSON, Weather *result){
	cJSON *json,*arrayItem,*object,*subobject,*item;
	
	json = cJSON_Parse(JSON); //解析JSON数据包
	if(json == NULL)		  //检测JSON数据包是否存在语法上的错误，返回NULL表示数据包无效
	{
		printf("Error before: [%s]\n",cJSON_GetErrorPtr()); //打印数据包语法错误的位置
		return 1;
	}
	else
	{
		if((arrayItem = cJSON_GetObjectItem(json,"results")) != NULL); //匹配字符串"results",获取数组内容
		{
			int size = cJSON_GetArraySize(arrayItem);     //获取数组中对象个数
#if DEBUG
			printf("cJSON_GetArraySize: size=%d\n",size); 
#endif
			//获取父对象内容
			if((object = cJSON_GetArrayItem(arrayItem,0)) != NULL){
				/* 匹配子对象1：城市地区相关 */
				if((subobject = cJSON_GetObjectItem(object,"location")) != NULL)
				{
					// 匹配id
					if((item = cJSON_GetObjectItem(subobject,"id")) != NULL){
						memcpy(result->id, item->valuestring,strlen(item->valuestring)); 		// 保存数据供外部调用
					}
					// 匹配城市名
					if((item = cJSON_GetObjectItem(subobject,"name")) != NULL) {
						memcpy(result->name, item->valuestring,strlen(item->valuestring)); 		// 保存数据供外部调用
					}
					// 匹配城市所在的国家
					if((item = cJSON_GetObjectItem(subobject,"country")) != NULL){
						memcpy(result->country, item->valuestring,strlen(item->valuestring)); 	// 保存数据供外部调用
					}
					// 匹配完整地名路径
					if((item = cJSON_GetObjectItem(subobject,"path")) != NULL)  {
						memcpy(result->path, item->valuestring,strlen(item->valuestring)); 		// 保存数据供外部调用	
					}
					// 匹配时区
					if((item = cJSON_GetObjectItem(subobject,"timezone")) != NULL){
						memcpy(result->timezone, item->valuestring,strlen(item->valuestring)); 	// 保存数据供外部调用	
					}
					// 匹配时差
					if((item = cJSON_GetObjectItem(subobject,"timezone_offset")) != NULL){
						memcpy(result->timezone_offset, item->valuestring,strlen(item->valuestring)); 	// 保存数据供外部调用
					}
				}
				/* 匹配子对象2：今天的天气情况 */
				if((subobject = cJSON_GetObjectItem(object,"now")) != NULL){
					// 匹配天气现象文字
					if((item = cJSON_GetObjectItem(subobject,"text")) != NULL){
						memcpy(result->text, item->valuestring,strlen(item->valuestring));  // 保存数据供外部调用
					}
					// 匹配天气现象代码
					if((item = cJSON_GetObjectItem(subobject,"code")) != NULL){
						memcpy(result->code, item->valuestring,strlen(item->valuestring));  // 保存数据供外部调用
					}
					// 匹配气温
					if((item = cJSON_GetObjectItem(subobject,"temperature")) != NULL) {
						memcpy(result->temperature, item->valuestring,strlen(item->valuestring));   // 保存数据供外部调用
					}	
				}
				/* 匹配子对象3：数据更新时间（该城市的本地时间） */
				if((subobject = cJSON_GetObjectItem(object,"last_update")) != NULL){
					memcpy(result->last_update, subobject->valuestring,strlen(subobject->valuestring));   // 保存数据供外部调用
				}
			} 
		}
	}
	
	cJSON_Delete(json); //释放cJSON_Parse()分配出来的内存空间
	
	return 0;
}

/*******************************************************************************************************
** 函数: cJSON_DailyWeatherParse，解析近三天天气数据
**------------------------------------------------------------------------------------------------------
** 参数: JSON：天气数据包   result：数据解析的结果
** 返回: int -1代表失败 0代表成功
********************************************************************************************************/
static int cJSON_DailyWeatherParse(char *JSON, Weather *result){
	cJSON *json,*arrayItem,*object,*subobject,*item,*sub_child_object,*child_Item;
	
	json = cJSON_Parse(JSON); //解析JSON数据包
	if(json == NULL)		  //检测JSON数据包是否存在语法上的错误，返回NULL表示数据包无效
	{
		printf("Error before: [%s]\n",cJSON_GetErrorPtr()); //打印数据包语法错误的位置
		return 1;
	}
	else{
		//匹配字符串"results",获取数组内容
		if((arrayItem = cJSON_GetObjectItem(json,"results")) != NULL){
			int size = cJSON_GetArraySize(arrayItem);     //获取数组中对象个数
#if DEBUG
			printf("Get Array Size: size=%d\n",size); 
#endif
			if((object = cJSON_GetArrayItem(arrayItem,0)) != NULL)//获取父对象内容{
				/* 匹配子对象1------结构体location */
				if((subobject = cJSON_GetObjectItem(object,"location")) != NULL){
					//匹配子对象1成员"name"
					if((item = cJSON_GetObjectItem(subobject,"name")) != NULL){
						memcpy(result->name, item->valuestring,strlen(item->valuestring)); 		// 保存数据供外部调用
					}
				}
				/* 匹配子对象2------数组daily */
				if((subobject = cJSON_GetObjectItem(object,"daily")) != NULL){
					int sub_array_size = cJSON_GetArraySize(subobject);
#if DEBUG
					printf("Get Sub Array Size: sub_array_size=%d\n",sub_array_size);
#endif
					for(int i = 0; i < sub_array_size; i++){
						if((sub_child_object = cJSON_GetArrayItem(subobject,i))!=NULL){
							// 匹配日期
							if((child_Item = cJSON_GetObjectItem(sub_child_object,"date")) != NULL){
								memcpy(result->date[i], child_Item->valuestring,strlen(child_Item->valuestring)); 		// 保存数据
							}
							// 匹配白天天气现象文字
							if((child_Item = cJSON_GetObjectItem(sub_child_object,"text_day")) != NULL){
								memcpy(result->text_day[i], child_Item->valuestring,strlen(child_Item->valuestring)); 	// 保存数据
							}
							// 匹配白天天气现象代码
							if((child_Item = cJSON_GetObjectItem(sub_child_object,"code_day")) != NULL){
								memcpy(result->code_day[i], child_Item->valuestring,strlen(child_Item->valuestring)); 	// 保存数据
							}
							// 匹配夜间天气现象代码
							if((child_Item = cJSON_GetObjectItem(sub_child_object,"code_night")) != NULL){
								memcpy(result->code_night[i], child_Item->valuestring,strlen(child_Item->valuestring)); // 保存数据
							}
							// 匹配最高温度
							if((child_Item = cJSON_GetObjectItem(sub_child_object,"high")) != NULL){
								memcpy(result->high[i], child_Item->valuestring,strlen(child_Item->valuestring)); 		//保存数据
							}
							// 匹配最低温度
							if((child_Item = cJSON_GetObjectItem(sub_child_object,"low")) != NULL){
								memcpy(result->low[i], child_Item->valuestring,strlen(child_Item->valuestring)); 		// 保存数据
							}
							// 匹配风向
							if((child_Item = cJSON_GetObjectItem(sub_child_object,"wind_direction")) != NULL){
								memcpy(result->wind_direction[i],child_Item->valuestring,strlen(child_Item->valuestring)); //保存数据
							}
							// 匹配风速，单位km/h（当unit=c时）
							if((child_Item = cJSON_GetObjectItem(sub_child_object,"wind_speed")) != NULL){
								memcpy(result->wind_speed[i], child_Item->valuestring,strlen(child_Item->valuestring)); // 保存数据
							}
							// 匹配风力等级
							if((child_Item = cJSON_GetObjectItem(sub_child_object,"wind_scale")) != NULL){
								memcpy(result->wind_scale[i], child_Item->valuestring,strlen(child_Item->valuestring)); // 保存数据
							}
						}
					}
				}
				/* 匹配子对象3------最后一次更新的时间 */
				if((subobject = cJSON_GetObjectItem(object,"last_update")) != NULL){
					//printf("%s:%s\n",subobject->string,subobject->valuestring);
				}
			} 
		}
	
	cJSON_Delete(json); //释放cJSON_Parse()分配出来的内存空间
	
	return 0;
}

/*******************************************************************************************************
** 函数: DisplayWeather，显示天气数据
**------------------------------------------------------------------------------------------------------
** 参数: weather_data：天气数据
** 返回: void
********************************************************************************************************/
static void DisplayWeather(Weather *weather_data){
	printf("===========%s此时的天气情况如下===========\n",weather_data->name);
	printf("天气：%s\n",weather_data->text);		
	printf("气温：%s℃\n",weather_data->temperature);	
	printf("时区：%s\n",weather_data->timezone);	
	printf("时差：%s\n",weather_data->timezone_offset);
	printf("天气更新时间：%s\n",weather_data->last_update);
	printf("===========%s近三天的天气情况如下===========\n",weather_data->name);
	printf("【%s】\n",weather_data->date[0]);
	printf("天气：%s\n",weather_data->text_day[0]);
	printf("最高温：%s℃\n",weather_data->high[0]);
	printf("最低温：%s℃\n",weather_data->low[0]);
	printf("风向：%s\n",weather_data->wind_direction[0]);
	printf("风速：%skm/h\n",weather_data->wind_speed[0]);
	printf("风力等级：%s\n",weather_data->wind_scale[0]);
	printf("\n");
	printf("【%s】\n",weather_data->date[1]);
	printf("天气：%s\n",weather_data->text_day[1]);
	printf("最高温：%s℃\n",weather_data->high[1]);
	printf("最低温：%s℃\n",weather_data->low[1]);
	printf("风向：%s\n",weather_data->wind_direction[1]);
	printf("风速：%skm/h\n",weather_data->wind_speed[1]);
	printf("风力等级：%s\n",weather_data->wind_scale[1]);
	printf("\n");
	printf("【%s】\n",weather_data->date[2]);
	printf("天气：%s\n",weather_data->text_day[2]);
	printf("最高温：%s℃\n",weather_data->high[2]);
	printf("最低温：%s℃\n",weather_data->low[2]);
	printf("风向：%s\n",weather_data->wind_direction[2]);
	printf("风速：%skm/h\n",weather_data->wind_speed[2]);
	printf("风力等级：%s\n",weather_data->wind_scale[2]);
}

/*******************************************************************************************************
** 函数: cmd_window_set，设置cmd窗口
**------------------------------------------------------------------------------------------------------
** 参数: weather_data：天气数据
** 返回: void
********************************************************************************************************/
static void cmd_window_set(struct cmd_windows_config *config){
	char cmd[50];
	
	//设置cmd窗口标题
	system("title kakaka");
	//设置cmd窗口的宽、高
	sprintf(cmd,"mode con cols=%d lines=%d",config->width,config->high);
	system(cmd);
	memset(cmd,0,sizeof(cmd));
	//设置窗口背景色
	sprintf(cmd,"color %x",config->color);
	system(cmd);
	memset(cmd,0,sizeof(cmd)); 
}

/*******************************************************************************************************
** 函数: printf_topic, 打印标题
**------------------------------------------------------------------------------------------------------
** 参数: void
** 返回: void
********************************************************************************************************/
static void printf_topic(void){
	system("date /T");	//输出日期
	system("time /T");	//输出时间
	
	printf("=================HTTP天气客户端==================\n");
	printf("请输入要查询天气的城市名称的拼音（如：beijing）："); 
}
