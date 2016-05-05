#if 1

#ifndef _AT_MQTT_CMD_H
#define _AT_MQTT_CMD_H
#include "osapi.h"
#include "at_custom.h"
#include "user_interface.h"
#include "../mqtt/mqtt.h"


#define MAC_ID "%02x%02x%02x%02x%02x%02x"
#if LIGHT_DEVICE
#define TOPIC_PREFIX "/iot/SMART_LIGHT"
#define TOPIC_FRM_UPSTREAM  TOPIC_PREFIX"/upstream/"MAC_ID
#define TOPIC_FRM_DOWNSTREAM  TOPIC_PREFIX"/downstream/"MAC_ID
#elif PLUG_DEVICE
#define TOPIC_PREFIX "/iot/SMART_LIGHT"
#define TOPIC_FRM_UPSTREAM  TOPIC_PREFIX"/upstream/"MAC_ID
#define TOPIC_FRM_DOWNSTREAM  TOPIC_PREFIX"/downstream/"MAC_ID
#elif SENSOR_DEVICE
#define TOPIC_PREFIX "/iot/SMART_LIGHT"
#define TOPIC_FRM_UPSTREAM  TOPIC_PREFIX"/upstream/"MAC_ID
#define TOPIC_FRM_DOWNSTREAM  TOPIC_PREFIX"/downstream/"MAC_ID

#elif CONTROL_DEVICE
#define TOPIC_PREFIX "/iot/SMART_LIGHT"
#define TOPIC_FRM_UPSTREAM  TOPIC_PREFIX"/upstream/"MAC_ID
#define TOPIC_FRM_DOWNSTREAM  TOPIC_PREFIX"/downstream/"MAC_ID

#endif


typedef enum {
    MQTT_IDLE,
	MQTT_WIFI_CONNECTED,
    MQTT_CONNECTED,
    MQTT_REGISTERED,
    MQTT_DISCONNECTED,
    MQTT_WIFI_DISCONNECTED,
    
}MQTT_STATUS;

enum{
    RST_NORMAL = 0,
    RST_RESET_ESPTOUCH = 3,
    RST_RESET_SYS = 5,
    RST_UPGRADE = 100,
    MODE_UPGRADE_DONE = 101,
    MODE_UPGRADE_ERR = 102
};

typedef enum{
DEV_CHECK_IDLE,//MACHINE_TYPE_IDLE, //DEV_CHECK_IDLE
//DEV_CHECK_IDLE,//MACHINE_TYPE_WAIT, //DEV_CHECK_IDLE
DEV_CHECK_PASS,//MACHINE_TYPE_CHECK_PASS,//DEV_CHECK_PASS
DEV_CHECK_FAIL,//MACHINE_TYPE_CHECK_FAIL,//DEV_CHECK_FAIL
DEV_AUTH_PASS,//MACHINE_TYPE_AUTH_PASS, //DEV_AUTH_PASS
DEV_AUTH_FAIL,//MACHINE_TYPE_AUTH_FAIL, //DEV_AUTH_FAIL  //ADD RETRY
DEV_AUTH_ERROR,
DEV_CHECK_TIMEOUT,//MACHINE_TYPE_CHECK_TIMEOUT,//DEV_CHECK_TIMEOUT
DEV_MQTT_CONNECTED,//MACHINE_TYPE_MQTT_CONNECTED,//DEV_MQTT_CONNECTED
}DEV_AUTH_STATUS;

void at_mqtt_SetupCmdConf(uint8_t id, char *pPara);
void at_mqtt_SetupCmdPub(uint8_t id,char *pPara);
//void flyco_RegisterSub();
void at_mqtt_SetupCmdCnwifi(uint8_t id,char *pPara);
MQTT_STATUS mqtt_GetStatus();
void mqtt_Init();
//void flyco_Uart2MqttStateMachine(uint8 c);
void at_AirkissStart(uint8_t id);
void at_EsptouchStart(uint8_t id);
void at_SmartconfigStop(uint8_t id);

void at_mqtt_exeCmdDisc(uint8_t id);
void at_HttpCmdTest(uint8_t id,char *pPara);

char* user_json_find_section(const char *pbuf,uint16 len,const char* section);
int user_JsonGetValue(const char* pbuffer,uint16 buf_len,const uint8* json_key,char* val_buf, uint16 val_len);
void at_test(uint8_t id);
void wifi_ConnectCb(uint8_t status);
void EsptouchTimeoutStop();
void at_mqtt_exeCmdConn(uint8_t id);
void at_mqtt_setupCmdSub(uint8_t id,char *pPara);
void at_mqtt_SetupCmdPub(uint8_t id,char *pPara);

extern MQTT_Client mqttClient;  //the main and only mqttclient in this application
extern MQTT_STATUS mqtt_status;//IDLE



#endif


#endif

