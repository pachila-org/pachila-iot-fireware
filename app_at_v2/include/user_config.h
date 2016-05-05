#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#define AT_CUSTOM_UPGRADE

#ifdef AT_CUSTOM_UPGRADE
    #ifndef AT_UPGRADE_SUPPORT
    #error "upgrade is not supported when eagle.flash.bin+eagle.irom0text.bin!!!"
    #endif
#endif

#define PLUG_DEVICE             0
#define LIGHT_DEVICE            0
#define SENSOR_DEVICE			1
#define CONTROL_DEVICE			0


#define CFG_HOLDER	0x00FF55A4	/* Change this value to load default configurations */
#define CFG_LOCATION	0x79	/* Please don't change or if you know what you doing */
#define CLIENT_SSL_ENABLE
//#define CLIENT_SSL_ENABLE

//#define USE_OPTIMIZE_PRINTF

/*DEFAULT CONFIGURATIONS*/


#define VERSION    6
#define TIMER_DBG  //os_printf

#define MQTT_HOST			"123.57.90.89" //or "mqtt.yourdomain.com"
#define MQTT_PORT			11883
#define MQTT_BUF_SIZE		(1500*1)
#define MQTT_KEEPALIVE		60	 /*second*/

#define MQTT_CLIENT_ID		"ESP_CLIENT"
#define MQTT_USER			"DVES_USER"
#define MQTT_PASS			"DVES_PASS"

#define MQTT_SUB_DEFAULT_TOPIC  "/SMART_LIGHT/"
#define MQTT_SUB_DEFAULT_QOS  1

#define STA_SSID "pachila_demo"
#define STA_PASS "12345678"
#define STA_TYPE AUTH_WPA2_PSK

#define MQTT_RECONNECT_TIMEOUT 	5	/*second*/

#define DEFAULT_SECURITY	0
#define QUEUE_BUFFER_SIZE		 		(2048*1)

//airkiss 2.0
#define AIRKISS_DEVICE_TYPE 		"gh_9e2cff3dfa51" //wechat public num 160218
#define AIRKISS_DEVICE_ID 			"122475" //device id  160218


#define MQTT_LIGHT_DEVICE   1

#define RST_MODE


#endif
