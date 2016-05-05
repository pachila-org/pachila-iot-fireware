/*
 * wifi.c
 *
 *  Created on: Dec 30, 2014
 *      Author: Minh
 */
#include "wifi.h"
#include "user_interface.h"
#include "osapi.h"
#include "espconn.h"
#include "os_type.h"
#include "mem.h"
#include "mqtt_msg.h"
#include "debug.h"
#include "user_config.h"
#include "config.h"
#include "../user/at_mqtt_cmd.h"


static ETSTimer WiFiLinker;
WifiCallback wifiCb = NULL;
static uint8_t wifiStatus = STATION_IDLE, lastWifiStatus = STATION_IDLE;

static void ICACHE_FLASH_ATTR wifi_check_ip(void *arg);

//****************//
HTTP_STAU httpStatus = 0;
uint8 IBM_IP[4] = {120,27,4,46};
char *post_url_act = "/iotpass/inbound.php?s=/DeviceAPI/activation";
char *post_url_rst = "/iotpass/inbound.php?s=/DeviceAPI/cancellation";
char *post_url_getip = "/iotpass/inbound.php?s=/DeviceAPI/urlroot/key/demo";


struct espconn * preHttpConn = NULL;
struct espconn * httpcConn = NULL;
#define MACSTR_IBM "\"%02x%02x%02x%02x%02x%02x\""

#define phttpGetBuffer "Connection: keep-alive\r\n\
Cache-Control: no-cache\r\n\
User-Agent: Mozilla/5.0 (Windows NT 5.1) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/30.0.1599.101 Safari/537.36 \r\n\
Accept: */*\r\n\
Accept-Encoding: gzip,deflate\r\n\
Accept-Language: zh-CN,eb-US;q=0.8\r\n\r\n"



LOCAL bool ICACHE_FLASH_ATTR
check_data(char *precv, uint16 length, uint16* payloadLen)
{
        //bool flag = true;
    char length_buf[10] = {0};
    char *ptemp = NULL;
    char *pdata = NULL;
    char *tmp_precvbuffer;
    uint16 tmp_length = length;
    uint32 tmp_totallength = 0;
	uint16 data_sumlength;
    
    ptemp = (char *)os_strstr(precv, "\r\n\r\n");
    
    if (ptemp != NULL) {
        tmp_length -= ptemp - precv;
        tmp_length -= 4;
        tmp_totallength += tmp_length;
        
        pdata = (char *)os_strstr(precv, "Content-Length: ");
        
        if (pdata != NULL){
            pdata += 16;
            tmp_precvbuffer = (char *)os_strstr(pdata, "\r\n");
            
            if (tmp_precvbuffer != NULL){
                os_memcpy(length_buf, pdata, tmp_precvbuffer - pdata);
                data_sumlength = atoi(length_buf);
                os_printf("A_dat:%u,tot:%u,lenght:%u\n",data_sumlength,tmp_totallength,tmp_length);
                if(data_sumlength != tmp_totallength){
                    return false;
                }
            }
        }
    }
	*payloadLen = data_sumlength;
	return true;
}
static void ICACHE_FLASH_ATTR IBM_Platform_recv_cb(void *arg, char *pdata, unsigned short len)
{
	
	os_printf("-----------------------\r\n");
	os_printf("sock recv:\r\n");
	os_printf("%s\r\n",pdata);
	os_printf("-----------------------\r\n");

	uint16 data_len = 0;
	char *p_str = NULL;
	char *p_tral = NULL;
	char *p_temp = NULL;
	uint8 mqtt_ip[32];
	uint16 mqtt_port=0;
	char  len_buf[10];
	int	  res = -1;
	uint8 i;
	
	if(TRUE != check_data(pdata, len, &data_len)){
		os_printf("data check err\n");
		return;
	}

	p_str = (char *)os_strstr(pdata, "\r\n\r\n");
    
    if (p_str != NULL) {
        p_str += 4;
		p_tral = (char *)os_strstr(p_str, "\"code\":");
		
		if(p_tral != NULL){
			p_tral += 7;
			p_temp = (char*)os_strstr(p_tral, ",");
			os_bzero(len_buf, sizeof(len_buf));
			os_memcpy(len_buf, p_tral, p_temp-p_tral);
			res = (uint8)atoi(len_buf);
		}
		
		if (httpStatus == DEV_ACTIVE){
	        p_tral = (char *)os_strstr(p_str, "\"host\":");
			if(p_tral != NULL){
				p_tral += 8;
				p_temp = (char*)os_strstr(p_tral, ",");
				os_bzero(mqtt_ip, sizeof(mqtt_ip));
				os_memcpy(mqtt_ip, (p_tral), p_temp-p_tral-1);
				
			}
	        p_tral = (char *)os_strstr(p_str, "\"port\":");
			if(p_tral != NULL){
				p_tral += 8;
				p_temp = (char*)os_strstr(p_tral, "\"");
				os_bzero(len_buf, sizeof(len_buf));	
				os_memcpy(len_buf, p_tral, p_temp-p_tral);
				mqtt_port = (uint16)atoi(len_buf);
			}
		}
		
    }
	if(res == 0)//dont't check
	{
		if (httpStatus == DEV_ACTIVE){
			os_printf("ip:%s\n",mqtt_ip);
			os_printf("port:%d\n",mqtt_port);
			sysCfg.auth_if = 1;
			sysCfg.cfg_holder = CFG_HOLDER;
			os_memcpy(sysCfg.mqtt_host, mqtt_ip, os_strlen(mqtt_ip));
			sysCfg.mqtt_port = mqtt_port;
			CFG_Save();
			mqtt_Init();
			//wifi_ConnectCb(STATION_GOT_IP);
		}
		else if (httpStatus == DEV_CANCELL){
			CFG_RESET();
			system_restart();
		}
		httpStatus = HTTP_IDLE;
	}
}

static void ICACHE_FLASH_ATTR IBM_Platform_sent_cb(void *arg)
{
	os_printf("sent cb\n");
	
}

LOCAL void ICACHE_FLASH_ATTR
data_send(void *arg, bool responseOK, char *psend, char *url)
{
    uint16 length = 0;
    char *pbuf = NULL;
    char httphead[256];
    struct espconn *ptrespconn = arg;
    os_memset(httphead, 0, 256);

    if (responseOK) {
		if(url){
        	os_sprintf(httphead,
                   "POST %s HTTP/1.0\r\nContent-Length: %d\r\nServer: lwIP/1.4.0\r\n", 
                   url, psend ? os_strlen(psend) : 0);
		}else{
			os_sprintf(httphead,
                   "HTTP/1.0 200 OK\r\nContent-Length: %d\r\nServer: lwIP/1.4.0\r\n",
                   psend ? os_strlen(psend) : 0);
		}
		
        if (psend) {
            os_sprintf(httphead + os_strlen(httphead),
                       "Content-type: application/json\r\nExpires: Fri, 10 Apr 2008 14:00:00 GMT\r\nPragma: no-cache\r\n\r\n");
            length = os_strlen(httphead) + os_strlen(psend);
            pbuf = (char *)os_zalloc(length + 1);
            os_memcpy(pbuf, httphead, os_strlen(httphead));
            os_memcpy(pbuf + os_strlen(httphead), psend, os_strlen(psend));
        } else {
            os_sprintf(httphead + os_strlen(httphead), "\n");
            length = os_strlen(httphead);
        }
    } else {
        os_sprintf(httphead, "HTTP/1.0 400 BadRequest\r\n\
Content-Length: 0\r\nServer: lwIP/1.4.0\r\n\n");
        length = os_strlen(httphead);
    }

    if (psend) {
		os_printf("Sent:%s\n",pbuf);
        espconn_sent(ptrespconn, pbuf, length);
    }

    if (pbuf) {
        os_free(pbuf);
        pbuf = NULL;
    }
}

static void ICACHE_FLASH_ATTR IBM_RegistSent(struct espconn *pespconn)
{
	char post_cmd[100];
	char hwaddr[6];
	int length = 0;
	
	wifi_get_macaddr(STATION_IF, hwaddr);
	os_printf("httpStatus:%d\n",httpStatus);
		
	switch(httpStatus){
		case DEV_ACTIVE:
			os_sprintf(post_cmd, "{\"device_mac\":"MACSTR_IBM"}", MAC2STR(hwaddr));
			os_printf("post:%s\n", post_cmd);
		    length = os_strlen(post_cmd);
			data_send(pespconn, true, post_cmd, post_url_act);
			break;
		case DEV_CANCELL:
			os_sprintf(post_cmd, "{\"device_mac\":"MACSTR_IBM",\"device_access_scret\":\"xxx\"}", MAC2STR(hwaddr));
			os_printf("post:%s\n", post_cmd);
		    length = os_strlen(post_cmd);
			data_send(pespconn, true, post_cmd, post_url_rst);
			break;
		case DEV_OTA:
			break;
		default:
			break;
	}
}
static void ICACHE_FLASH_ATTR http_connect_callback (void *arg)
{
	struct espconn *pespconn = arg;
   	uint8 i =0;
   
   	os_printf("http_connect_cb\n");
   	espconn_regist_recvcb(pespconn, IBM_Platform_recv_cb);
   	espconn_regist_sentcb(pespconn, IBM_Platform_sent_cb);
	
	IBM_RegistSent(pespconn);
}
static void ICACHE_FLASH_ATTR http_disconnect_callback(void *arg)
{
	struct espconn *pespconn = arg;
	os_printf("IBM_platform_discon_cb\n");
	
	if (httpStatus == DEV_CANCELL){
		CFG_RESET();
#if CONTROL_DEVICE
		user_down_power();
#endif	
		system_restart();
	}
}
static void ICACHE_FLASH_ATTR http_error_callback(void *arg, sint8 err)
{
	os_printf("err connect cb%d\n",err);
	
	if (httpStatus == DEV_CANCELL){
		CFG_RESET();
#if CONTROL_DEVICE
		user_down_power();
#endif		
		system_restart();
	}
}

void ICACHE_FLASH_ATTR http_connect(void)
{
	//connect http
	os_printf("get ip, connect http server...\n");
	httpcConn = (struct espconn *)os_zalloc(sizeof(struct espconn));
	httpcConn->type = ESPCONN_TCP;
	httpcConn->state = ESPCONN_NONE;
	httpcConn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
	httpcConn->proto.tcp->local_port = espconn_port();
	httpcConn->proto.tcp->remote_port = 80;
	httpcConn->reverse = NULL;

	os_memcpy(httpcConn->proto.tcp->remote_ip, sysCfg.http_ip, 4);

	espconn_regist_connectcb(httpcConn, http_connect_callback);
	espconn_regist_disconcb(httpcConn, http_disconnect_callback);
	espconn_regist_reconcb(httpcConn, http_error_callback);
	espconn_connect(httpcConn);
	
	if(sysCfg.auth_if != 1)
		httpStatus = DEV_ACTIVE;
}
static void ICACHE_FLASH_ATTR pre_IBM_Platform_recv_cb(void *arg, char *pdata, unsigned short len)
{
	
	os_printf("-----------------------\r\n");
	os_printf("sock recv:\r\n");
	//os_printf("%s\r\n",pdata);
	

	uint16 data_len = 0;
	char *p_str = NULL;
	char *p_tral = NULL;
	char *p_temp = NULL;
	uint8 mqtt_ip[32];
	uint16 mqtt_port=0;
	char  len_buf[10];
	int	  res = -1;
	uint8 i;
	
	if(TRUE != check_data(pdata, len, &data_len)){
		os_printf("data check err\n");
		return;
	}
	//{"code":0,"data":{"ip":"120.27.4.46"}}
	p_str = (char *)os_strstr(pdata, "\r\n\r\n");
    
    if (p_str != NULL) {
        p_str += 4;
		p_tral = (char *)os_strstr(p_str, "\"ip\":");
		
		if(p_tral != NULL){
			p_tral += 6;
			p_temp = (char*)os_strstr(p_tral, ".");
			os_bzero(len_buf, sizeof(len_buf));
			os_memcpy(len_buf, p_tral, p_temp-p_tral);
			sysCfg.http_ip[0] = (uint8)atoi(len_buf);

			p_temp++;
			p_tral = (char*)os_strstr(p_temp, ".");
			os_bzero(len_buf, sizeof(len_buf));
			os_memcpy(len_buf, p_temp, p_tral-p_temp);
			sysCfg.http_ip[1] = (uint8)atoi(len_buf);

			p_tral ++;
			p_temp = (char*)os_strstr(p_tral, ".");
			os_bzero(len_buf, sizeof(len_buf));
			os_memcpy(len_buf, p_tral, p_temp-p_tral);
			sysCfg.http_ip[2] = (uint8)atoi(len_buf);

			p_temp++;
			p_tral = (char*)os_strstr(p_temp, "\"}");
			os_bzero(len_buf, sizeof(len_buf));
			os_memcpy(len_buf, p_temp, p_tral-p_temp);
			sysCfg.http_ip[3] = (uint8)atoi(len_buf);

			CFG_Save();
			os_printf("got ip:%d.%d.%d.%d\n",sysCfg.http_ip[0],sysCfg.http_ip[1],sysCfg.http_ip[2],sysCfg.http_ip[3]);
			http_connect();
		}
    }
	os_printf("-----------------------\r\n");
}

static void ICACHE_FLASH_ATTR pre_http_connect_callback (void *arg)
{
	struct espconn *pespconn = arg;
   	uint8 i =0;
   
   	os_printf("pre http_connect_cb\n");
   	espconn_regist_recvcb(pespconn, pre_IBM_Platform_recv_cb);
   	espconn_regist_sentcb(pespconn, IBM_Platform_sent_cb);
	
	char get_cmd[512];
	char hwaddr[6];
	int length = 0;
	uint8 ip[4];
	
	os_memcpy(ip, pespconn->proto.tcp->remote_ip, 4);
	os_bzero(get_cmd, sizeof(get_cmd));
	os_sprintf(get_cmd, "GET %s HTTP/1.0\r\nHost: "IPSTR":%d\r\n"phttpGetBuffer"",
               post_url_getip, IP2STR(ip),pespconn->proto.tcp->remote_port);
               
	//os_printf("%s\n",get_cmd);
	espconn_sent(pespconn, get_cmd, os_strlen(get_cmd));
}
static void ICACHE_FLASH_ATTR pre_http_disconnect_callback(void *arg)
{
	struct espconn *pespconn = arg;
	os_printf("pre_http_disconnect_callback\n");
}
static void ICACHE_FLASH_ATTR pre_http_error_callback(void *arg, sint8 err)
{
	os_printf("err connect cb%d\n",err);
}

static void ICACHE_FLASH_ATTR pre_http_connect(void)
{
	//connect http
	os_printf("get ip, connect pre http server...\n");
	preHttpConn = (struct espconn *)os_zalloc(sizeof(struct espconn));
	preHttpConn->type = ESPCONN_TCP;
	preHttpConn->state = ESPCONN_NONE;
	preHttpConn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
	preHttpConn->proto.tcp->local_port = espconn_port();
	preHttpConn->proto.tcp->remote_port = 80;
	preHttpConn->reverse = NULL;

	os_memcpy(preHttpConn->proto.tcp->remote_ip, IBM_IP, 4);

	espconn_regist_connectcb(preHttpConn, pre_http_connect_callback);
	espconn_regist_disconcb(preHttpConn, pre_http_disconnect_callback);
	espconn_regist_reconcb(preHttpConn, pre_http_error_callback);
	espconn_connect(preHttpConn);
}
#if 0
os_timer_t suber;
static void ICACHE_FLASH_ATTR send_sub(void*p)
{
	uint8* data_buf = "{\"rgb\":{\"red\":19171}}";
	uint8 pub_topic[100];
	char hwaddr[6];
	wifi_get_macaddr(STATION_IF, hwaddr);
	os_sprintf(pub_topic, TOPIC_FRM_UPSTREAM,  MAC2STR(hwaddr));
	if( MQTT_Publish(&mqttClient, pub_topic, data_buf, sizeof(data_buf) , 1, 0) ==TRUE){
		os_printf("--------------------\nMQTT_Publish ok\n");
		os_printf("%s\n",data_buf);
	}
}
#endif

static void ICACHE_FLASH_ATTR wifi_check_ip(void *arg)
{
	TIMER_DBG("%s\r\n",__func__);

	struct ip_info ipConfig;

	os_timer_disarm(&WiFiLinker);
	wifi_get_ip_info(STATION_IF, &ipConfig);
	wifiStatus = wifi_station_get_connect_status();
	if (wifiStatus == STATION_GOT_IP && ipConfig.ip.addr != 0)
	{
		if(sysCfg.auth_if != 1){
			pre_http_connect();
		}else{
			http_connect();
#if CONTROL_DEVICE
			user_led_off();
#endif
		}
		//user_light_connect_ap();
	}
	else
	{
		if(wifi_station_get_connect_status() == STATION_WRONG_PASSWORD)
		{

			INFO("STATION_WRONG_PASSWORD\r\n");
			wifi_station_connect();


		}
		else if(wifi_station_get_connect_status() == STATION_NO_AP_FOUND)
		{

			INFO("STATION_NO_AP_FOUND\r\n");
			wifi_station_connect();


		}
		else if(wifi_station_get_connect_status() == STATION_CONNECT_FAIL)
		{

			INFO("STATION_CONNECT_FAIL\r\n");
			wifi_station_connect();

		}
		else
		{
			INFO("STATION_IDLE\r\n");
			//INFO("TEST STATION STATUS: %d \r\n",wifi_station_get_connect_status());
		}
		os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
		os_timer_arm(&WiFiLinker, 1000, 0);
	}
	os_printf("wifiStatus:%d, lastWifiStatus:%d\n\n",wifiStatus,lastWifiStatus);
	if(wifiStatus != lastWifiStatus){
		lastWifiStatus = wifiStatus;
		if(wifiCb){
			wifiCb(wifiStatus);
#if LIGHT_DEVICE
			user_light_connect_ap();
#endif
			//os_timer_disarm(&suber);
			//os_timer_setfn(&suber, (os_timer_func_t *)send_sub, NULL);
			//os_timer_arm(&suber, 3000, 0);
		}
	}
}

void ICACHE_FLASH_ATTR WIFI_Connect(uint8_t* ssid, uint8_t* pass, WifiCallback cb)
{
	struct station_config stationConf;
	
	INFO("WIFI_INIT\r\n");
	wifi_set_opmode(STATION_MODE);//150924
	wifi_station_set_auto_connect(FALSE);
	wifiCb = cb;
	
	if(STATION_GOT_IP != wifi_station_get_connect_status()){
		os_memset(&stationConf, 0, sizeof(struct station_config));

		os_sprintf(stationConf.ssid, "%s", ssid);
		os_sprintf(stationConf.password, "%s", pass);

		wifi_station_set_config(&stationConf);
		wifi_station_set_auto_connect(TRUE);
		wifi_station_connect();
	}

	os_timer_disarm(&WiFiLinker);
	os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
	os_timer_arm(&WiFiLinker, 1000, 0);

}

void ICACHE_FLASH_ATTR
	disable_check_ip()
{
	os_timer_disarm(&WiFiLinker);
}

