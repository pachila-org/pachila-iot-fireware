/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_main.c
 *
 * Description: entry file of user application
 *
 * Modification history:
 *     2015/1/23, v1.0 create this file.
*******************************************************************************/

#if 1



#include "osapi.h"
#include "at_custom.h"
//#include "at.h"
#include "user_interface.h"
#include "at_mqtt_cmd.h"
#include "../mqtt/mqtt.h"
#include "../mqtt/config.h"
#include "../mqtt/wifi.h"
#include "osapi.h"
#include "mem.h"
//#include "flyco_protocol.h"
#include "smartconfig.h"
#include "espconn.h"
#include "upgrade.h"
#include "user_plug.h"

#include "espconn.h"
//#include "httpclient.h"
//#include "user_json.h"


extern SYSCFG sysCfg;

MQTT_Client mqttClient;  //the main and only mqttclient in this application
MQTT_STATUS mqtt_status = MQTT_IDLE;//IDLE

#define INFO os_printf
#define INFO2 //os_printf


uint32 smartCfgType;



//=============
//JSON PARSE
//=============
char* ICACHE_FLASH_ATTR user_json_find_section(const char *pbuf,uint16 len,const char* section)
{
    char* sect = NULL,c;
	bool json_split = false;
	if(!pbuf || !section)
		return NULL;

	sect = (char*)os_strstr(pbuf,section);

    /*
    * the formal json format
    *{"elem":"value"}\r\n
    *{"elem":value}\r\n
    */
    if(sect){
        sect += os_strlen(section);
		while((uint32)sect<(uint32)(pbuf + len)){
            c = *sect++;
			if(c == ':' && !json_split){
                json_split = true;
				continue;
			}
			//{"elem":"value"}\r\n
			if(c == '"'){
                break;
			}
			//{"elem":value}\r\n
			//{"elem":  value}\r\n
			//{"elem":\tvalue}\r\n
			//{"elem":\nvalue}\r\n
			//{"elem":\rvalue}\r\n
			if(c != ' ' && c!='\n' && c!='\r'){
                sect--;
				break;
			}
		}
		return sect == NULL || (uint32)sect - (uint32)pbuf >= len?NULL:sect ;

    }
}

int ICACHE_FLASH_ATTR
	user_JsonGetValue(const char* pbuffer,uint16 buf_len,const uint8* json_key,char* val_buf, uint16 val_len)
{
    //char val[20];
	uint32 result = 0;
	char* pstr = NULL,*pparse = NULL;
	uint16 val_size=0;
	uint16 len_tmp = buf_len;
	bool int_flg = false;

	pstr = user_json_find_section(pbuffer,buf_len,json_key);

	if(NULL == pstr) return -1;

	while(len_tmp>0){
		pstr++;
		if( *pstr == '"'){
			int_flg = false;
			pstr++;
			break;
		}else if(*pstr != ' '){
		    int_flg = true;
			break;
		}
		len_tmp--;
	}

	if(len_tmp<=0) return -2;


    //find the end char of value
    if( int_flg == true &&
		(pparse = (char*)os_strstr(pstr,","))!= NULL ||
		(pparse = (char*)os_strstr(pstr,"\""))!= NULL ||
		(pparse = (char*)os_strstr(pstr,"}"))!= NULL ||
		(pparse = (char*)os_strstr(pstr,"]"))!= NULL){
		val_size = pparse - pstr;
    }else if(int_flg == false && (pparse = (char*)os_strstr(pstr,"\""))!= NULL){
		val_size = pparse - pstr;
	}else{
        return -3;
    }
    os_memset(val_buf,0,val_len);

	if(val_size > val_len)
		os_memcpy(val_buf,pstr,val_len);
	else
		os_memcpy(val_buf,pstr,val_size);

	//result = atol(val);
	return 0;

}

//====================================================================
/*Return mqtt_status for this application*/
/*MQTT_IDEL: nothing*/
/*MQTT_CONNECTED: connect to mqtt server(tcp)*/
/*MQTT_REGISTERED: receive server register successful message published by server*/
/*MQTT_DISCONNECTED: disconnect with mqtt server*/
MQTT_STATUS mqtt_GetStatus()
{
    return mqtt_status;
}

/*set mqtt client parameters via AT+ COMMAND*/
/*THIS FUNCTION MIGHT NOT BE NECESSARY IN A FORMAL VERSION*/
/*EXAMPLE:*/
/*AT+MQTT_CONF="192.168.100.106",11883,0,60*/
/*AT+MQTT_CONF=  IP, PORT,SECURITY, KEEP-ALIVE*/
void ICACHE_FLASH_ATTR
at_mqtt_SetupCmdConf(uint8_t id, char *pPara)
{
    at_port_print("in mqtt setupcmd\n\r");
    int result = 0, err = 0, flag = 0;
    uint8 host_ip[32] = {0};
    uint32 host_port = 0;
    uint8 security_if = 0;
    uint32 keep_alive = 60;
    uint8* client_id = NULL;
    uint8 buf[100]={0};
    
    pPara++; // skip '='
    at_data_str_copy(host_ip, &pPara, 15);
    at_port_print("host_ip:");
    at_port_print(host_ip);
    at_port_print("\r\n");

    if (*pPara!= ',') { // skip ','
        at_response_error();
        return;
    }
    pPara++;
    flag = at_get_next_int_dec(&pPara, &result, &err);
    
    // flag must be ture because there are more parameter
    if (flag == FALSE) {
        at_port_print("flg error\n\r");
        at_response_error();
        return;
    }
    host_port = result;
    
    if (*pPara++ != ',') { // skip ','
        at_response_error();
        return;
    }
    flag = at_get_next_int_dec(&pPara, &result, &err);
    // flag must be ture because there are more parameter
    if (flag == FALSE) {
        at_response_error();
        return;
    }
    security_if = ( result>1 ? 0:result );
    
    if (*pPara  != ',') { // skip ','
        at_response_error();
        return;
    }
    pPara++;
    flag = at_get_next_int_dec(&pPara, &result, &err);
    keep_alive= result;
    os_sprintf(buf,"hport:%d\r\nsecurity:%d\r\nkeepalive:%d\r\n",host_port,security_if,keep_alive);
    at_port_print(buf);
    if (*pPara != '\r') {
        at_response_error();
        return;
    }


    CFG_SET(host_ip, host_port,security_if, keep_alive, MQTT_CLIENT_ID,0,0);
	
	at_mqtt_exeCmdDisc(0);
	wifi_station_set_auto_connect(0);
	MQTT_FreeClient(&mqttClient);
	disable_check_ip();
	wifi_station_disconnect();
	mqtt_status=MQTT_IDLE;
	
    INFO("security is : %d \r\n",sysCfg.security);
    MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.security);
    //MQTT_InitConnection(&mqttClient, "192.168.1.102", 11883, 0);
    MQTT_InitClient(&mqttClient, sysCfg.device_id, MQTT_USER, MQTT_PASS, sysCfg.mqtt_keepalive, 1);
    //MQTT_InitClient(&mqttClient, sysCfg.device_id, sysCfg.mqtt_user, sysCfg.mqtt_pass, sysCfg.mqtt_keepalive, 1);
    //MQTT_InitClient(&mqttClient, "client_id_t", "user_t", "pass_t", 120, 1);
    //at_response_ok();
    
	//FOR DEBUG
	sysCfg.auth_if=1;
	sysCfg.cfg_holder = CFG_HOLDER;
	CFG_Save();
    INFO("call at_mqtt_exeCmdConn\r\n");
	at_mqtt_exeCmdConn(0);

	INFO("end\r\n");
}

/*for debug, to print that mqtt publish ok*/
void ICACHE_FLASH_ATTR
mqtt_PublishedCb(uint32_t *args)
{
    MQTT_Client* client = (MQTT_Client*)args;
    INFO("MQTT: Published\r\n");
}

/*An MQTT level disconnect event, dont*/
void ICACHE_FLASH_ATTR
mqtt_DisconnectedCb(uint32_t *args)
{
    MQTT_Client* client = (MQTT_Client*)args;
    INFO("MQTT: Disconnected\r\n");
    mqtt_status = MQTT_DISCONNECTED;
    INFO("wifi disconnected status to MCU : mqtt_status : %d\r\n ",mqtt_status);
}



/*MQTT DATA RECEIVE CALLBACK*/
/*PROCESS AND PARSE RECEIVE MESSAGE */
/*1.RCV REGISTER MSG:  IF REGISTERED , PUB OTA, SUB CMD & DISREG*/
/*2.RCV OTA: GET URL AND UPGRADE*/
/*3.RCV DISREG: FORGET AP INFO, RESET REG STATUS*/
/*4.RCV CMD: PARSE AND SEND VIA UART*/
void ICACHE_FLASH_ATTR
mqtt_DataCb(uint32_t *args, const char* topic, uint32_t topic_len, const char *data, uint32_t data_len)
{
    
    /*do not copy data again, process the pointer passed in directly*/
    /*
    char *topicBuf = (char*)os_zalloc(topic_len+1),
    *dataBuf = (char*)os_zalloc(data_len+1);
    os_memcpy(topicBuf, topic, topic_len);
    topicBuf[topic_len] = 0;
    
    os_memcpy(dataBuf, data, data_len);
    dataBuf[data_len] = 0;
    */
    MQTT_Client* client = (MQTT_Client*)args;
    INFO("--------------------\r\n");
    INFO("MQTT RCV:\r\n");
    INFO("topic: %s  \r\n data: %s \r\n", topic, data);
    uint8 idx;
    /*1.RCV REGISTER MSG:  IF REGISTERED , PUB OTA, SUB CMD & DISREG*/
	
    if( (0==os_memcmp(topic,TOPIC_PREFIX,os_strlen(TOPIC_PREFIX))) ){    //check reg status pub
        INFO("GOT LIGHT TOPIC\r\n");

		if(os_strstr(data,"cancellation")){
			CFG_RESET();
#if CONTROL_DEVICE
			user_down_power();
#endif	
			system_restart();
		}
		
#if LIGHT_DEVICE
		if(os_strstr(data,"rgb")){
			char red_str[30],green_str[30],blue_str[30],white_str[30];
			os_memset(red_str,0,sizeof(red_str));
			os_memset(green_str,0,sizeof(green_str));
			os_memset(blue_str,0,sizeof(blue_str));
			os_memset(white_str,0,sizeof(white_str));
			user_JsonGetValue(data,data_len,"red",red_str, sizeof(red_str));
			user_JsonGetValue(data,data_len,"green",green_str, sizeof(green_str));
			user_JsonGetValue(data,data_len,"blue",blue_str, sizeof(blue_str));
			user_JsonGetValue(data,data_len,"white",white_str, sizeof(white_str));
			int red = atoi(red_str);
			int green = atoi(green_str);
			int blue = atoi(blue_str);
			int white = atoi(white_str);
			
			os_printf("red: %d; green: %d; blue: %d; white:%d\r\n",red,green,blue,white);
			light_set_aim(red,green,blue,white,white,1000);

		}else{
			os_printf("PAYLOAD NOT RGB COMMAND\r\n");
		}
#elif PLUG_DEVICE
		if(os_strstr(data,"swh")){
			char on_str[30];
			os_memset(on_str,0,sizeof(on_str));
			user_JsonGetValue(data,data_len,"on",on_str, sizeof(on_str));
			int on = atoi(on_str);
			
			os_printf("plug on/off:%d \r\n",on);
			if(on)
				user_plug_set_status(1);
			else
				user_plug_set_status(0);
			//light_set_aim(red,green,blue,white,white,1000);

		}else{
			os_printf("PAYLOAD NOT RGB COMMAND\r\n");
		}			
#endif

    }
}



/*callback function called after connected to the server*/
/*register data pub cb,data rcv cb,disconnect cb*/
void ICACHE_FLASH_ATTR
mqtt_ConnectedCb()
{
    INFO("mqtt connected..\r\n");
    //MQTT_OnConnected(&mqttClient, mqtt_ConnectedCb);
    MQTT_OnPublished(&mqttClient, mqtt_PublishedCb);
    MQTT_OnData(&mqttClient, mqtt_DataCb);
    MQTT_OnDisconnected(&mqttClient, mqtt_DisconnectedCb);

/*
    if( MQTT_Subscribe(&mqttClient, MQTT_SUB_DEFAULT_TOPIC, MQTT_SUB_DEFAULT_QOS) ==TRUE){
        INFO("SUB DEFAULT TOPIC OK\n\r");
        at_response_ok();
    }else{
        INFO("SUB DEFAULT TOPIC ERROR\n\r");
        at_response_error();
    }
*/   

    uint8 mac_sta[6] = {0};
    uint8* topic_cmd_sub=NULL  ;
    wifi_get_macaddr(STATION_IF, mac_sta);
    topic_cmd_sub = (uint8*)os_zalloc( os_strlen(TOPIC_FRM_DOWNSTREAM)+1);
    os_sprintf(topic_cmd_sub,TOPIC_FRM_DOWNSTREAM, MAC2STR(mac_sta) );
    
    if( MQTT_Subscribe(&mqttClient, topic_cmd_sub, MQTT_SUB_DEFAULT_QOS) ==TRUE){
        INFO("SUB to TOPIC [%s] OK\n\r", topic_cmd_sub);
        at_response_ok();
    }else{
        INFO("SUB to TOPIC [%s] ERROR\n\r", topic_cmd_sub);
        at_response_error();
    }
   
}




/*after esptouch have the device connected to the router, */
/*the device wait for phone confirm it's type */
void ICACHE_FLASH_ATTR
wifi_SmartConfigConnectCb(uint8_t status)
{
    if(status == STATION_GOT_IP){		
		mqtt_status = MQTT_WIFI_CONNECTED;

		if(sysCfg.auth_if == 1 && sysCfg.cfg_holder == CFG_HOLDER){
    		os_printf("AUTHENTICATED ALREADY...\r\n");
    		wifi_ConnectCb(status);
		}else{
    		INFO("wifi connected\r\n");
    		if(smartCfgType== SC_TYPE_AIRKISS){
				INFO("--------------\r\n");
				INFO("TYPE: AIRKISS , SKIP TYPE CHECK...\r\n");
				INFO("--------------\r\n");
    		}else{
				INFO("--------------\r\n");
				INFO("TYPE: ESPTOUCH , RUN TYPE CHECK...\r\n");
				INFO("--------------\r\n");
    		}
		}
		#if 0
		if((mqtt_status == MQTT_IDLE) || (mqtt_status == MQTT_DISCONNECTED)){
            INFO("wifi connected\r\n");
            MQTT_OnConnected(&mqttClient, mqtt_ConnectedCb);
            INFO("set mqtt conn cb\r\n");
            MQTT_Connect(&mqttClient);
            INFO("end of wifi connectcb\r\n");
		}else{
			INFO("MQTT ALREADY CONNECTED...\r\n");
		}
		#endif
    }
}


os_timer_t wifi_disc_timer;
void ICACHE_FLASH_ATTR
	wifi_disc_func()
{
	wifi_station_set_auto_connect(0);
	disable_check_ip();
	wifi_station_disconnect();
}


/*called after wifi connected*/
void ICACHE_FLASH_ATTR
wifi_ConnectCb(uint8_t status)
{
    if(status == STATION_GOT_IP){
		mqtt_status = MQTT_WIFI_CONNECTED;

		if(sysCfg.auth_if == 1 && sysCfg.cfg_holder == CFG_HOLDER){
            INFO("wifi connected\r\n");
            MQTT_OnConnected(&mqttClient, mqtt_ConnectedCb);
            INFO("set mqtt conn cb\r\n");
            MQTT_Connect(&mqttClient);
            INFO("end of wifi connectcb\r\n");
		}else{

		}
    }
}



void ICACHE_FLASH_ATTR
at_mqtt_exeCmdDisc(uint8_t id)
{
    INFO("do mqtt disconnect here\r\n");
    MQTT_OnDisconnected(&mqttClient, NULL);
	if(mqttClient.pCon){
        espconn_disconnect(mqttClient.pCon);
	}
    MQTT_Disconnect(&mqttClient);
	//MQTT_FreeClient(&mqttClient);
    mqtt_DisconnectedCb((uint32_t*)&mqttClient);
}



void ICACHE_FLASH_ATTR
at_mqtt_exeCmdConn(uint8_t id)
{
    struct ip_info ipconfig;
    wifi_get_ip_info(STATION_IF, &ipconfig);

    if (wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0) {
		mqtt_status = MQTT_WIFI_CONNECTED;
    	MQTT_OnConnected(&mqttClient, mqtt_ConnectedCb);
    	MQTT_Connect(&mqttClient);
    }
}



void ICACHE_FLASH_ATTR
at_mqtt_setupCmdSub(uint8_t id,char *pPara)
{
    int result = 0, err = 0, flag = 0;
    uint8 topic[200] = {0};
	uint8 qos = 0;
    pPara++; // skip '='

    //get the first parameter
    // digit
    //flag = at_get_next_int_dec(&pPara, &result, &err);
    //get the second parameter
    // string
    at_data_str_copy(topic, &pPara, 200);
    at_port_print("topic:");
    at_port_print(topic);
    at_port_print("\r\n");

        if (*pPara!= ',') { // skip ','
	        at_response_error();
	        return;
        }
		pPara++;
        flag = at_get_next_int_dec(&pPara, &result, &err);
        // flag must be ture because there are more parameter
        
	qos = result;
		
    if (*pPara != '\r') {
        at_response_error();
        return;
    }
	INFO("\r\n-----MQTT_SUB-----\r\n");
	INFO("topic: %s \r\nqos:%d\r\n",topic,qos);
	INFO("------------------\r\n");

    if( MQTT_Subscribe(&mqttClient, topic, qos) ==TRUE){
        at_response_ok();
    }else{
        at_response_error();
    }

}

#if 0
void ICACHE_FLASH_ATTR
	at_testCmdPub_1(uint8_t id)
{
	INFO(" IN at_testCmdPub\r\n");
	uint8 data_buf[100]={0};
	
	os_sprintf(data_buf,DEVICE_ID "%06x", system_get_chip_id() & 0xffffff);
	uint8 cmd_buf[32] = { 0x2,0xaa , 0x1, 0x00, 0x01 , 0x2 , 0x55 , 1 , 0x20 , 0xaa , 0x1, 0x00, 0x00 , 0x1 , 0x55 , 1};
	//uint8 cmd_data[7] = { 0xaa , 0x1, 0x00, 0x01 , 0x2 , 0x55 , 0}
	//cmd_buf[0]=0x2;
	os_memcpy(data_buf+DEV_ID_LEN, cmd_buf , 16);
	


	
    if( MQTT_Publish(&mqttClient, flyco_GetSubCmdTopic(),data_buf,32 , 1, 0) ==TRUE){
        at_response_ok();
    }else{
        at_response_error();
    }
	

}
#endif


#if 1
void ICACHE_FLASH_ATTR
	at_testCmdPub(uint8_t id)
{
#if 0
FLYCO_PAYLOAD  payload;
os_sprintf(payload.device_id, DEVICE_ID "%06x", 0xffffff&system_get_chip_id());
payload.cmd_num = 0x3;

#if 0
FLYCO_CMD cmd_tmp_1 = (FLYCO_CMD*)os_malloc(sizeof(FLYCO_CMD))  ;
cmd_tmp_1->cmd_header = 0x55;
cmd_tmp_1->cmd_num = 0x1;
cmd_tmp_1->cmd_data = 0x0001;
cmd_tmp_1->cmd_csum = (cmd_tmp_1->cmd_num+cmd_tmp_1->cmd_data)&0xff;
cmd_tmp_1->cmd_tail = 0xaa;
#endif
//========   CMD1  =============
FLYCO_SINGLE_SET cmd_set_single;
cmd_set_single.cmd.cmd_header = 0x55;
cmd_set_single.cmd.cmd_num = 0x1;
cmd_set_single.cmd.cmd_data_h = 0x12;
cmd_set_single.cmd.cmd_data_l = 0x01;
cmd_set_single.cmd.cmd_csum = (cmd_set_single.cmd.cmd_num+cmd_set_single.cmd.cmd_data_l)&0xff;
cmd_set_single.cmd.cmd_tail = 0xaa;

cmd_set_single.cmd_index = 1;
cmd_set_single.cmd_separator = ' ';

uint8* pCmdSet = payload.cmd_set;
os_memcpy( pCmdSet, &cmd_set_single , sizeof(FLYCO_SINGLE_SET));
pCmdSet+=sizeof(FLYCO_SINGLE_SET);

//========   CMD2  =============

cmd_set_single.cmd.cmd_header = 0x55;
cmd_set_single.cmd.cmd_num = 0x2;
cmd_set_single.cmd.cmd_data_h = 0x23;
cmd_set_single.cmd.cmd_data_l = 0x03;
cmd_set_single.cmd.cmd_csum = (cmd_set_single.cmd.cmd_num+cmd_set_single.cmd.cmd_data_l)&0xff;
cmd_set_single.cmd.cmd_tail = 0xaa;

cmd_set_single.cmd_index = 2;
cmd_set_single.cmd_separator = ' ';

os_memcpy( pCmdSet, &cmd_set_single , sizeof(FLYCO_SINGLE_SET));
pCmdSet+=sizeof(FLYCO_SINGLE_SET);

//========   CMD3  =============

cmd_set_single.cmd.cmd_header = 0x55;
cmd_set_single.cmd.cmd_num = 0x3;
cmd_set_single.cmd.cmd_data_h = 0x34;
cmd_set_single.cmd.cmd_data_l= 0x19;

cmd_set_single.cmd.cmd_csum = (cmd_set_single.cmd.cmd_num+cmd_set_single.cmd.cmd_data_l)&0xff;
cmd_set_single.cmd.cmd_tail = 0xaa;

cmd_set_single.cmd_index = 3;
cmd_set_single.cmd_separator = ' ';

os_memcpy( pCmdSet, &cmd_set_single , sizeof(FLYCO_SINGLE_SET));
pCmdSet+=sizeof(FLYCO_SINGLE_SET);

#if 0
uint8 i ; 
for(i=0;i<CMD_LEN;i++) INFO(" 0x%02x ",*(  ((uint8*)&(cmd_set_single.cmd)) + i));
INFO("\r\n");
INFO("ADDRESS : 0x%08x   ;   0x%08x  ;\r\n",&(cmd_set_single.cmd),&(cmd_set_single.cmd.cmd_header));
INFO("test cmd_set_single.cmd.cmd_header: 0x%02x\r\n",cmd_set_single.cmd.cmd_header);
INFO("test cmd_set_single.cmd.cmd_num: 0x%02x\r\n",cmd_set_single.cmd.cmd_num);
INFO("test cmd_set_single.cmd.cmd_data: 0x%04x\r\n",cmd_set_single.cmd.cmd_data);
INFO("test cmd_set_single.cmd.cmd_csum: 0x%02x\r\n",cmd_set_single.cmd.cmd_csum);
INFO("test cmd_set_single.cmd.cmd_tail: 0x%02x\r\n",cmd_set_single.cmd.cmd_tail);
INFO("===============\r\n");
#endif



#if (CMD_DATE_LEN>0)
//set data here

#endif



    if( MQTT_Publish(&mqttClient, flyco_GetSubCmdTopic(),(char*)&payload,sizeof(FLYCO_PAYLOAD) , 1, 0) ==TRUE){
        at_response_ok();
    }else{
        at_response_error();
    }


#endif
}

#endif



void ICACHE_FLASH_ATTR
at_mqtt_SetupCmdPub(uint8_t id,char *pPara)
{
    int result = 0, err = 0, flag = 0;
    uint8 topic[200] = {0};
    uint8 message[200] = {0};
    uint8 qos = 0;
    uint8 retain = 0;
    pPara++; // skip '='
    at_data_str_copy(topic, &pPara, 200);
    at_port_print("topic:");
    at_port_print(topic);
    at_port_print("\r\n");
    
    if (*pPara!= ',') { // skip ','
        at_response_error();
        return;
    }
    pPara++;
    at_data_str_copy(message, &pPara, 200);
    at_port_print("message:");
    at_port_print(message);
    at_port_print("\r\n");
    if (*pPara++ != ',') { // skip ','
        at_response_error();
        return;
    }
    flag = at_get_next_int_dec(&pPara, &result, &err);
    // flag must be ture because there are more parameter
    if (flag == FALSE) {
        at_response_error();
        return;
    }
    qos = result;
    
    if (*pPara++ != ',') { // skip ','
        at_response_error();
        return;
    }
    
    flag = at_get_next_int_dec(&pPara, &result, &err);
    // flag must be ture because there are more parameter
    
    retain = result;

	INFO("\r\n-----MQTT_PUB-----\r\n");
    INFO("topic: %s \r\nmessage:%s\r\nqos:%d\r\nretain:%d\r\n",topic,message,qos,retain);
	INFO("------------------\r\n");
    if (*pPara != '\r') {
        at_response_error();
        return;
    }
    
    if( MQTT_Publish(&mqttClient, topic, message, os_strlen(message) , qos, retain) ==TRUE){
        at_response_ok();
    }else{
        at_response_error();
    }

}


/*AT command to config device connect to a router*/
/*for debug only*/
void ICACHE_FLASH_ATTR
at_mqtt_SetupCmdCnwifi(uint8_t id,char *pPara)
{
    int result = 0, err = 0, flag = 0;
    uint8 ssid[64] = {0};
    uint8 pswd[64] = {0};
    pPara++; // skip '='
    at_data_str_copy(ssid, &pPara, 64);
    at_port_print("ssid:");
    at_port_print(ssid);
    at_port_print("\r\n");
    if (*pPara!= ',') { // skip ','
        at_response_error();
        return;
    }
    pPara++;
    at_data_str_copy(pswd, &pPara, 64);
    at_port_print("pswd:");
    at_port_print(pswd);
    at_port_print("\r\n");
    if (*pPara != '\r') {
        at_response_error();
        return;
    }
    os_sprintf(sysCfg.sta_ssid, "%s", ssid);
    os_sprintf(sysCfg.sta_pwd, "%s", pswd);
    sysCfg.cfg_holder = CFG_HOLDER;
    CFG_Save();
    WIFI_Connect(ssid, pswd, wifi_ConnectCb);
	
}


/*callback function after airkiss or esptouch is finished*/
void ICACHE_FLASH_ATTR
wifi_SmartconfigDone(sc_status status, void *pdata)  
{  
switch(status) {

    case SC_STATUS_WAIT: 
        os_printf("SC_STATUS_WAIT\n"); 
		break; 
	case SC_STATUS_FIND_CHANNEL: 
		os_printf("SC_STATUS_FIND_CHANNEL\n"); 
		break;
	case SC_STATUS_GETTING_SSID_PSWD: 
		os_printf("SC_STATUS_GETTING_SSID_PSWD\n");
		sc_type *type = pdata;
		if (*type == SC_TYPE_ESPTOUCH) {
			smartCfgType = SC_TYPE_ESPTOUCH;
            os_printf("SC_TYPE:SC_TYPE_ESPTOUCH\n");
        }else{
            smartCfgType = SC_TYPE_AIRKISS;
            os_printf("SC_TYPE:SC_TYPE_AIRKISS\n");
        }
		break;
	case SC_STATUS_LINK: 
		EsptouchTimeoutStop();
		os_printf("SC_STATUS_LINK\n"); 
		/////////////////////////////////////////////////////
		struct station_config *sta_conf = pdata;
		os_memcpy(sysCfg.sta_ssid, sta_conf->ssid,sizeof(sta_conf->ssid));
		os_printf("smt ssid :%s\r\n", sta_conf->ssid);
		os_printf("set ssid :%s\r\n", sysCfg.sta_ssid);
		os_memcpy(sysCfg.sta_pwd,sta_conf->password,sizeof(sta_conf->password));
		os_printf("smt pwd :%s\r\n", sta_conf->password);
		os_printf("set pwd :%s\r\n", sysCfg.sta_pwd);
		sysCfg.cfg_holder = CFG_HOLDER;
		CFG_Save();
		//WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifi_ConnectCb);
		WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifi_SmartConfigConnectCb);
		//#endif
		break;
	case SC_STATUS_LINK_OVER: 
		os_printf("SC_STATUS_LINK_OVER\n"); 
		if (pdata != NULL) {
            uint8 phone_ip[4] = {0};
            os_memcpy(phone_ip, (uint8*)pdata, 4);
            os_printf("Phone ip: %d.%d.%d.%d\n",phone_ip[0],phone_ip[1],phone_ip[2],phone_ip[3]);
        }
		smartconfig_stop(); 
		break; 
	}  

}

os_timer_t esptouch_tout_t;
#define ESPTOUCH_TIME_OUT_S  30
void ICACHE_FLASH_ATTR
	EsptouchToutFunc()
{
	os_timer_disarm(&esptouch_tout_t);
	smartconfig_stop();
	os_printf("ESP-TOUCH TIMEOUT...\r\n");
	if(sysCfg.auth_if == 1 && sysCfg.cfg_holder == CFG_HOLDER){
		os_printf("AUTH ALREADY , CONNECT TO ORIGINAL ROUTER...\r\n");
		WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd,wifi_SmartConfigConnectCb);
	}else{
		os_printf("NOT AUTH YET, WAIT FOR COMMAND...\r\n");
	}
}
void ICACHE_FLASH_ATTR
	EsptouchTimeoutStart()
{
	os_timer_disarm(&esptouch_tout_t);
	os_timer_setfn(&esptouch_tout_t,EsptouchToutFunc,NULL);
	os_timer_arm(&esptouch_tout_t,ESPTOUCH_TIME_OUT_S*1000,0);
}

void ICACHE_FLASH_ATTR
	EsptouchTimeoutStop()
{
	os_timer_disarm(&esptouch_tout_t);
}


/*airkiss start*/
void ICACHE_FLASH_ATTR
at_AirkissStart(uint8_t id)
{
	wifi_set_opmode(STATION_MODE);
    smartconfig_stop();
    disable_check_ip();
    mqtt_DisconnectedCb((uint32_t*)&mqttClient);
    smartconfig_start( wifi_SmartconfigDone);
}

/*ESP-TOUCH start*/
void ICACHE_FLASH_ATTR
at_EsptouchStart(uint8_t id)
{
	at_mqtt_exeCmdDisc(0);
	wifi_station_set_auto_connect(0);
	disable_check_ip();
	mqtt_status=MQTT_IDLE;	  
	wifi_station_disconnect();

	wifi_set_opmode(STATION_MODE);
    smartconfig_stop();
    //disable_check_ip();
    INFO("MQTT: Disconnected\r\n");
    //mqtt_status = MQTT_DISCONNECTED;
    INFO("wifi disconnected status to MCU : mqtt_status : %d\r\n ",mqtt_status);
	EsptouchTimeoutStart();
    smartconfig_start( wifi_SmartconfigDone);//SC_TYPE_AIRKISS
    
}

/*SMARTCONFIG STOP*/
void ICACHE_FLASH_ATTR
at_SmartconfigStop(uint8_t id)
{
    smartconfig_stop();
}





/////////////////////////////////////////
//debug
/////////////////////////////////////////
//#include "httpclient.h"
os_timer_t wifi_disc_t;
void ICACHE_FLASH_ATTR
	wifi_disc()
{
	disable_check_ip();
	wifi_station_disconnect();
}
void ICACHE_FLASH_ATTR
at_test(uint8_t id)
{
	//test_md5();
	
	CFG_RESET();
	at_mqtt_exeCmdDisc(0);
	wifi_station_set_auto_connect(0);
	//MQTT_FreeClient(&mqttClient);
	//disable_check_ip();
	//wifi_station_disconnect();
	mqtt_status=MQTT_IDLE;
	
	os_timer_disarm(&wifi_disc_t);
	os_timer_setfn(&wifi_disc_t,wifi_disc,NULL);
	os_timer_arm(&wifi_disc_t,1000,0);

}


void ICACHE_FLASH_ATTR
at_HttpCmdTest(uint8_t id,char *pPara)
{
    int result = 0, err = 0, flag = 0;
    uint8 url[200] = {0};
    uint8 data[64] = {0};
    pPara++; // skip '='
    
    at_data_str_copy(url, &pPara, 200);
    at_port_print("url:");
    at_port_print(url);
    at_port_print("\r\n");
	
    if (*pPara!= ',') { // skip ','
        at_response_error();
        return;
    }
    pPara++;
    at_data_str_copy(data, &pPara, 64);
    at_port_print("data:");
    at_port_print(data);
    at_port_print("\r\n");
    if (*pPara != '\r') {
        at_response_error();
        return;
    }
	at_port_print("http parse ok\r\n");
	

	
}

#if 0
void ICACHE_FLASH_ATTR
	mqtt_client_init()
{
    sysCfg.cfg_holder = CFG_HOLDER;
    CFG_SET(FLYCO_SERVER_IP, FLYCO_SERVER_PORT,FLYCO_SERVER_SECURITY_IF, 60,(char*)flyco_GetMacId() ,0,0);
    INFO("security is : %d \r\n",sysCfg.security);
    MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.security);
    MQTT_InitClient(&mqttClient, sysCfg.device_id, "espressif", "passw0rd", sysCfg.mqtt_keepalive, 1);
}
#endif



void ICACHE_FLASH_ATTR
EsptouchDoneCb(sc_status status, void *pdata)  
{  
	struct station_config stationConf;
	switch(status) {
	
    case SC_STATUS_WAIT: 
        os_printf("SC_STATUS_WAIT\n"); 
		break; 
	case SC_STATUS_FIND_CHANNEL: 
		os_printf("SC_STATUS_FIND_CHANNEL\n"); 
		break;
	case SC_STATUS_GETTING_SSID_PSWD: 
		os_printf("SC_STATUS_GETTING_SSID_PSWD\n");
		sc_type *type = pdata;
		if (*type == SC_TYPE_ESPTOUCH) {
			smartCfgType = SC_TYPE_ESPTOUCH;
            os_printf("SC_TYPE:SC_TYPE_ESPTOUCH\n");
        }else{
            smartCfgType = SC_TYPE_AIRKISS;
            os_printf("SC_TYPE:SC_TYPE_AIRKISS\n");
        }
		break;
	case SC_STATUS_LINK: 
		EsptouchTimeoutStop();
		os_printf("SC_STATUS_LINK\n"); 
		/////////////////////////////////////////////////////
		struct station_config *sta_conf = pdata;
		os_memcpy(sysCfg.sta_ssid, sta_conf->ssid,sizeof(sta_conf->ssid));
		os_printf("smt ssid :%s\r\n", sta_conf->ssid);
		os_printf("set ssid :%s\r\n", sysCfg.sta_ssid);
		os_memcpy(sysCfg.sta_pwd,sta_conf->password,sizeof(sta_conf->password));
		os_printf("smt pwd :%s\r\n", sta_conf->password);
		os_printf("set pwd :%s\r\n", sysCfg.sta_pwd);
		sysCfg.cfg_holder = CFG_HOLDER;
		CFG_Save();
		WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, NULL);
		//#endif
		break;
	case SC_STATUS_LINK_OVER: 
		os_printf("SC_STATUS_LINK_OVER\n"); 
		if (pdata != NULL) {
            uint8 phone_ip[4] = {0};
            os_memcpy(phone_ip, (uint8*)pdata, 4);
            os_printf("Phone ip: %d.%d.%d.%d\n",phone_ip[0],phone_ip[1],phone_ip[2],phone_ip[3]);
        }
		wifi_ConnectCb(STATION_GOT_IP);
#if LIGHT_DEVICE
		user_light_connect_ap();
#elif CONTROL_DEVICE
		user_led_off();
#endif
		

		smartconfig_stop(); 
		break; 
	}  

}


void ICACHE_FLASH_ATTR
esptouch_start()
{
	wifi_station_set_auto_connect(0);
	disable_check_ip();
	mqtt_status=MQTT_IDLE;	  
	wifi_station_disconnect();

	wifi_set_opmode(STATION_MODE);
    smartconfig_stop();
    //disable_check_ip();
    INFO("MQTT: Disconnected\r\n");
    //mqtt_status = MQTT_DISCONNECTED;
    INFO("wifi disconnected status to MCU : mqtt_status : %d\r\n ",mqtt_status);
	//EsptouchTimeoutStart();
    smartconfig_start(EsptouchDoneCb);//SC_TYPE_AIRKISS
}

os_timer_t RstFlagTimer;

//set flag for next time chip reboots
void ICACHE_FLASH_ATTR
    sys_set_reset_flg(uint32 rst)
{
	os_printf("rst flag\n");
	sysCfg.rst_flag = rst&0xff;
    CFG_Save();
}


/*MQTT init */
void ICACHE_FLASH_ATTR
mqtt_Init()
{
	CFG_Load();//RST_NORMAL
	
	if(sysCfg.rst_flag == RST_RESET_ESPTOUCH){
		httpStatus = DEV_CANCELL; //dis activation 
		sysCfg.rst_flag = RST_NORMAL;
	}else if(sysCfg.rst_flag == RST_RESET_SYS){
		httpStatus = DEV_CANCELL; //dis activation 
		sysCfg.rst_flag = RST_NORMAL;
	}else{
		os_printf("rst flag++:%d\n",sysCfg.rst_flag);
		sysCfg.rst_flag++;
	}
	CFG_Save();
	
#if LIGHT_DEVICE
	if(httpStatus == DEV_CANCELL || (sysCfg.auth_if != 1 && sysCfg.cfg_holder != CFG_HOLDER)){
		user_light_cancel();//±ä³ÉºìÉ«
	}
#endif
	os_timer_disarm(&RstFlagTimer);
	os_timer_setfn(&RstFlagTimer, sys_set_reset_flg, RST_NORMAL);
	os_timer_arm(&RstFlagTimer, 5000, 0);

#if 1
    if(sysCfg.auth_if == 1 && sysCfg.cfg_holder == CFG_HOLDER){
	    //connect to router, get mqtt server info, connect mqtt broker
        MQTT_InitConnection(&mqttClient, sysCfg.mqtt_host, sysCfg.mqtt_port, sysCfg.security);
        MQTT_InitClient(&mqttClient, sysCfg.device_id, sysCfg.mqtt_user, sysCfg.mqtt_pass, sysCfg.mqtt_keepalive, 1);
		//MQTT_InitLWT(&mqttClient,(uint8_t*)flyco_GetLwtTopic(),(uint8_t*)flyco_GetLwtPayload(),QOS_LWT,RETAIN_LWT);
        WIFI_Connect(sysCfg.sta_ssid, sysCfg.sta_pwd, wifi_ConnectCb);
    }else{
		esptouch_start();
    }
#endif
    //INFO("mqtt init .. do something\r\n");
    //os_printf("test heap25: %d \r\n",system_get_free_heap_size());
}
#endif
