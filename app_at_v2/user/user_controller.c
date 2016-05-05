/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_humiture.c
 *
 * Description: humiture demo's function realization
 *
 * Modification history:
 *     2014/5/1, v1.0 create this file.
*******************************************************************************/
#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "user_interface.h"
#include "../user/at_mqtt_cmd.h"
#include "../mqtt/wifi.h"
#include "user_controller.h"



#if CONTROL_DEVICE
#include "user_sensor.h"

LOCAL struct keys_param keys;
LOCAL struct single_key_param *single_key[CONTROL_KEY_NUM];
LOCAL os_timer_t sensor_sleep_timer;
LOCAL os_timer_t link_led_timer;
LOCAL uint8 link_led_level = 0;
LOCAL uint32 link_start_time;


#define UPLOAD_FRAME  "{\"butt\": {\"num\": %d}}"

#define MVH3004_Addr    0x88

LOCAL uint8 humiture_data[4];

uint32 ledIoInfo[3][3]={
		{PERIPHS_IO_MUX_MTDO_U, FUNC_GPIO15, 15},
		{PERIPHS_IO_MUX_MTCK_U, FUNC_GPIO13, 13},
		{PERIPHS_IO_MUX_MTMS_U, FUNC_GPIO14, 14}
	};




/******************************************************************************
 * FunctionName : user_humiture_long_press
 * Description  : humiture key's function, needed to be installed
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_sensor_long_press(void)
{
    //user_esp_platform_set_active(0);
    //system_restore();
    //system_restart();
    user_led_on();
    os_printf("down power\n");
    user_down_power();
	
	if(mqtt_status == MQTT_CONNECTED || mqtt_status == MQTT_WIFI_CONNECTED)
		user_led_off();
}
/******************************************************************************
 * FunctionName : user_humiture_long_press
 * Description  : humiture key's function, needed to be installed
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
user_key1_long_press(void)
{
    //user_esp_platform_set_active(0);
    //system_restore();
    //system_restart();
    os_printf("sensor reset\n");
    httpStatus = DEV_CANCELL;
    http_connect();
	
	if(mqtt_status == MQTT_CONNECTED || mqtt_status == MQTT_WIFI_CONNECTED)
		user_led_off();
}


LOCAL void ICACHE_FLASH_ATTR
user_key1_short_press(void)
{
	os_printf("key1 short press\n");
	char 	pbuf[50];
	uint8 	pub_topic[100];
	char 	hwaddr[6];
	int 	i = 1;
	
	wifi_get_macaddr(STATION_IF, hwaddr);
	
	os_bzero(pub_topic,sizeof(pub_topic));
	os_sprintf(pub_topic, TOPIC_FRM_UPSTREAM,  MAC2STR(hwaddr));
	
	os_bzero(pbuf, sizeof(pbuf));
	os_sprintf(pbuf, UPLOAD_FRAME, i);
	
	if( MQTT_Publish(&mqttClient, pub_topic, pbuf, sizeof(pbuf) , 1, 0) == TRUE){
		os_printf("--------------------\nMQTT_Publish ok\n");
		os_printf("%s\n",pbuf);
	}
	
	if(mqtt_status == MQTT_CONNECTED || mqtt_status == MQTT_WIFI_CONNECTED)
		user_led_off();

}

LOCAL void ICACHE_FLASH_ATTR
user_key2_short_press(void)
{
	os_printf("key2 short press\n");
	char 	pbuf[50];
	uint8 	pub_topic[100];
	char 	hwaddr[6];
	int 	i = 2;
	
	wifi_get_macaddr(STATION_IF, hwaddr);
	
	os_bzero(pub_topic,sizeof(pub_topic));
	os_sprintf(pub_topic, TOPIC_FRM_UPSTREAM,  MAC2STR(hwaddr));
	
	os_bzero(pbuf, sizeof(pbuf));
	os_sprintf(pbuf, UPLOAD_FRAME, i);
	
	if( MQTT_Publish(&mqttClient, pub_topic, pbuf, sizeof(pbuf) , 1, 0) == TRUE){
		os_printf("--------------------\nMQTT_Publish ok\n");
		os_printf("%s\n",pbuf);
	}
	
	if(mqtt_status == MQTT_CONNECTED || mqtt_status == MQTT_WIFI_CONNECTED)
		user_led_off();
}
LOCAL void ICACHE_FLASH_ATTR
user_key3_short_press(void)
{
	os_printf("key3 short press\n");
	char 	pbuf[50];
	uint8 	pub_topic[100];
	char 	hwaddr[6];
	int 	i = 3;
	
	wifi_get_macaddr(STATION_IF, hwaddr);
	
	os_bzero(pub_topic,sizeof(pub_topic));
	os_sprintf(pub_topic, TOPIC_FRM_UPSTREAM,  MAC2STR(hwaddr));
	
	os_bzero(pbuf, sizeof(pbuf));
	os_sprintf(pbuf, UPLOAD_FRAME, i);
	
	if( MQTT_Publish(&mqttClient, pub_topic, pbuf, sizeof(pbuf) , 1, 0) == TRUE){
		os_printf("--------------------\nMQTT_Publish ok\n");
		os_printf("%s\n",pbuf);
	}

	if(mqtt_status == MQTT_CONNECTED || mqtt_status == MQTT_WIFI_CONNECTED)
		user_led_off();
}
LOCAL void ICACHE_FLASH_ATTR
user_key4_short_press(void)
{
	os_printf("key4 short press\n");
	char 	pbuf[50];
	uint8 	pub_topic[100];
	char 	hwaddr[6];
	int 	i = 4;
	
	wifi_get_macaddr(STATION_IF, hwaddr);
	
	os_bzero(pub_topic,sizeof(pub_topic));
	os_sprintf(pub_topic, TOPIC_FRM_UPSTREAM,  MAC2STR(hwaddr));
	
	os_bzero(pbuf, sizeof(pbuf));
	os_sprintf(pbuf, UPLOAD_FRAME, i);
	
	if( MQTT_Publish(&mqttClient, pub_topic, pbuf, sizeof(pbuf) , 1, 0) == TRUE){
		os_printf("--------------------\nMQTT_Publish ok\n");
		os_printf("%s\n",pbuf);
	}

	if(mqtt_status == MQTT_CONNECTED || mqtt_status == MQTT_WIFI_CONNECTED)
		user_led_off();
}




void ICACHE_FLASH_ATTR
user_sensor_deep_sleep_enter(void)
{
    system_deep_sleep(SENSOR_DEEP_SLEEP_TIME > link_start_time \
    ? SENSOR_DEEP_SLEEP_TIME - link_start_time : 30000000);
}

void ICACHE_FLASH_ATTR
user_sensor_deep_sleep_disable(void)
{
    os_timer_disarm(&sensor_sleep_timer);
}

void ICACHE_FLASH_ATTR
user_sensor_deep_sleep_init(uint32 time)
{
    os_timer_disarm(&sensor_sleep_timer);
    os_timer_setfn(&sensor_sleep_timer, (os_timer_func_t *)user_sensor_deep_sleep_enter, NULL);
    os_timer_arm(&sensor_sleep_timer, time, 0);
}

os_timer_t sensor_upload_timer;


void ICACHE_FLASH_ATTR
upload_status(void*p)
{
	char 	pbuf[100];
	uint8 	pub_topic[100];
	char 	hwaddr[6];
	wifi_get_macaddr(STATION_IF, hwaddr);
	
	os_printf("mqtt_status:%d\n",mqtt_status);	
	
	//if(mqtt_status == MQTT_CONNECTED || mqtt_status == MQTT_WIFI_CONNECTED)
	{
		
		uint16 tp, rh;
        uint8 *data;
        uint32 tp_t, rh_t;

        rh = data[0] << 8 | data[1];
        tp = data[2] << 8 | data[3];
        tp_t = (tp >> 2) * 165 * 100 / (16384 - 1);
        rh_t = (rh & 0x3fff) * 100 * 100 / (16384 - 1);

		os_bzero(pbuf,sizeof(pbuf));
		
        if (tp_t >= 4000) {
            os_sprintf(pbuf, UPLOAD_FRAME, "", tp_t / 100 - 40, tp_t % 100, rh_t / 100, rh_t % 100);
        } else {
            tp_t = 4000 - tp_t;
            os_sprintf(pbuf, UPLOAD_FRAME, "-", tp_t / 100, tp_t % 100, rh_t / 100, rh_t % 100);
        }

		os_printf("payload:%s\n",pbuf);
		os_bzero(pub_topic,sizeof(pub_topic));
		os_sprintf(pub_topic, TOPIC_FRM_UPSTREAM,  MAC2STR(hwaddr));
		if( MQTT_Publish(&mqttClient, pub_topic, pbuf, sizeof(pbuf) , 1, 0) == TRUE){
			os_printf("--------------------\nMQTT_Publish ok\n");
		}
	}
}
LOCAL void ICACHE_FLASH_ATTR
user_hold_power()
{
	PIN_FUNC_SELECT(SENSOR_POWER_IO_MUX, FUNC_GPIO2);
	gpio_output_set((1<<SENSOR_POWER_IO_NUM), 0, (1<<SENSOR_POWER_IO_NUM), 0);
}

void ICACHE_FLASH_ATTR
user_down_power()
{
	gpio_output_set(0, (1<<SENSOR_KEY1_IO_NUM), (1<<SENSOR_KEY1_IO_NUM), 0 );
	gpio_output_set(0, (1<<SENSOR_KEY2_IO_NUM),(1<<SENSOR_KEY2_IO_NUM), 0 );
	gpio_output_set(0, (1<<SENSOR_KEY3_IO_NUM),(1<<SENSOR_KEY3_IO_NUM) ,0 );
	gpio_output_set(0, (1<<SENSOR_KEY4_IO_NUM), (1<<SENSOR_KEY4_IO_NUM),0 );

	gpio_output_set(0, (1<<SENSOR_POWER_IO_NUM), (1<<SENSOR_POWER_IO_NUM), 0);
}


LOCAL void ICACHE_FLASH_ATTR
user_led_init()
{
	uint8 x;
	for (x=0; x<3; x++) {
		PIN_FUNC_SELECT(ledIoInfo[x][0], ledIoInfo[x][1]);
		gpio_output_set(0, (1<<ledIoInfo[x][2]), 0, (1<<ledIoInfo[x][2]));
	}
}

void ICACHE_FLASH_ATTR
user_led_on()
{
	uint8 x;
	for (x=0; x<3; x++) {
		gpio_output_set((1<<ledIoInfo[x][2]), 0, (1<<ledIoInfo[x][2]), 0 );
	}
}

void ICACHE_FLASH_ATTR
user_led_off()
{
	os_printf("led off\n");
	uint8 x;
	for (x=0; x<3; x++) {
		gpio_output_set(0, (1<<ledIoInfo[x][2]), 0, (1<<ledIoInfo[x][2]));
	}
}

/******************************************************************************
 * FunctionName : user_humiture_init
 * Description  : init humiture function, include key and mvh3004
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_controller_init(uint8 active)
{
    //user_link_led_init();
    //wifi_status_led_install(SENSOR_WIFI_LED_IO_NUM, SENSOR_WIFI_LED_IO_MUX, SENSOR_WIFI_LED_IO_FUNC);
	
	user_hold_power();
	user_led_init();
	user_led_on();

    
	single_key[0] = key_init_single(SENSOR_KEY1_IO_NUM, SENSOR_KEY1_IO_MUX, SENSOR_KEY1_IO_FUNC,
                                        user_key1_long_press, user_key1_short_press);
	single_key[1] = key_init_single(SENSOR_KEY2_IO_NUM, SENSOR_KEY2_IO_MUX, SENSOR_KEY2_IO_FUNC,
                                        user_sensor_long_press, user_key2_short_press);
	single_key[2] = key_init_single(SENSOR_KEY3_IO_NUM, SENSOR_KEY3_IO_MUX, SENSOR_KEY3_IO_FUNC,
                                        user_sensor_long_press, user_key3_short_press);
	single_key[3] = key_init_single(SENSOR_KEY4_IO_NUM, SENSOR_KEY4_IO_MUX, SENSOR_KEY4_IO_FUNC,
										user_sensor_long_press, user_key4_short_press);

    keys.key_num = CONTROL_KEY_NUM;
    keys.single_key = single_key;

    key_init(&keys);

    if (GPIO_INPUT_GET(GPIO_ID_PIN(SENSOR_KEY1_IO_NUM)) == 0) {
        //user_sensor_long_press();
    }

#ifdef SENSOR_DEEP_SLEEP
    if (wifi_get_opmode() != STATIONAP_MODE) {
        if (active == 1) {
            user_sensor_deep_sleep_init(SENSOR_DEEP_SLEEP_TIME / 1000 );
        } else {
            user_sensor_deep_sleep_init(SENSOR_DEEP_SLEEP_TIME / 1000 / 3 * 2);
        }
    }
#endif


}
#endif

