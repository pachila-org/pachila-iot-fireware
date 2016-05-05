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




#if SENSOR_DEVICE
#include "user_sensor.h"

LOCAL struct keys_param keys;
LOCAL struct single_key_param *single_key[SENSOR_KEY_NUM];
LOCAL os_timer_t sensor_sleep_timer;
LOCAL os_timer_t link_led_timer;
LOCAL uint8 link_led_level = 0;
LOCAL uint32 link_start_time;

#if HUMITURE_SUB_DEVICE
#include "driver/i2c_master.h"

#define UPLOAD_FRAME  "{\"hum\": {\"x\": %s%d.%02d,\"y\": %d.%02d}}"

#define MVH3004_Addr    0x88

LOCAL uint8 humiture_data[4];

/******************************************************************************
 * FunctionName : user_mvh3004_burst_read
 * Description  : burst read mvh3004's internal data
 * Parameters   : uint8 addr - mvh3004's address
 *                uint8 *pData - data point to put read data
 *                uint16 len - read length
 * Returns      : bool - true or false
*******************************************************************************/
LOCAL bool ICACHE_FLASH_ATTR
user_mvh3004_burst_read(uint8 addr, uint8 *pData, uint16 len)
{
    uint8 ack;
    uint16 i;

    i2c_master_start();
    i2c_master_writeByte(addr);
    ack = i2c_master_getAck();

    if (ack) {
        os_printf("addr not ack when tx write cmd \n");
        i2c_master_stop();
        return false;
    }

    i2c_master_stop();
    i2c_master_wait(40000);

    i2c_master_start();
    i2c_master_writeByte(addr + 1);
    ack = i2c_master_getAck();

    if (ack) {
        os_printf("addr not ack when tx write cmd \n");
        i2c_master_stop();
        return false;
    }

    for (i = 0; i < len; i++) {
        pData[i] = i2c_master_readByte();

        i2c_master_setAck((i == (len - 1)) ? 1 : 0);
    }

    i2c_master_stop();

    return true;
}

/******************************************************************************
 * FunctionName : user_mvh3004_read_th
 * Description  : read mvh3004's humiture data
 * Parameters   : uint8 *data - where data to put
 * Returns      : bool - ture or false
*******************************************************************************/
bool ICACHE_FLASH_ATTR
user_mvh3004_read_th(uint8 *data)
{
    return user_mvh3004_burst_read(MVH3004_Addr, data, 4);
}

/******************************************************************************
 * FunctionName : user_mvh3004_init
 * Description  : init mvh3004, mainly i2c master gpio
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_mvh3004_init(void)
{
    i2c_master_gpio_init();
}

uint8 *ICACHE_FLASH_ATTR
user_mvh3004_get_poweron_th(void)
{
    return humiture_data;
}
#endif

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
    os_printf("sensor reset\n");
    httpStatus = DEV_CANCELL;
    http_connect();
}





LOCAL void ICACHE_FLASH_ATTR
user_link_led_init(void)
{
    PIN_FUNC_SELECT(SENSOR_LINK_LED_IO_MUX, SENSOR_LINK_LED_IO_FUNC);
    PIN_FUNC_SELECT(SENSOR_UNUSED_LED_IO_MUX, SENSOR_UNUSED_LED_IO_FUNC);
    GPIO_OUTPUT_SET(GPIO_ID_PIN(SENSOR_UNUSED_LED_IO_NUM), 0);
}

void ICACHE_FLASH_ATTR
user_link_led_output(uint8 level)
{
    GPIO_OUTPUT_SET(GPIO_ID_PIN(SENSOR_LINK_LED_IO_NUM), level);
}

LOCAL void ICACHE_FLASH_ATTR
user_link_led_timer_cb(void)
{
    link_led_level = (~link_led_level) & 0x01;
    GPIO_OUTPUT_SET(GPIO_ID_PIN(SENSOR_LINK_LED_IO_NUM), link_led_level);
}

void ICACHE_FLASH_ATTR
user_link_led_timer_init(void)
{
    link_start_time = system_get_time();

    os_timer_disarm(&link_led_timer);
    os_timer_setfn(&link_led_timer, (os_timer_func_t *)user_link_led_timer_cb, NULL);
    os_timer_arm(&link_led_timer, 50, 1);
    link_led_level = 0;
    GPIO_OUTPUT_SET(GPIO_ID_PIN(SENSOR_LINK_LED_IO_NUM), link_led_level);
}

void ICACHE_FLASH_ATTR
user_link_led_timer_done(void)
{
    os_timer_disarm(&link_led_timer);
    GPIO_OUTPUT_SET(GPIO_ID_PIN(SENSOR_LINK_LED_IO_NUM), 0);
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
		user_mvh3004_read_th(humiture_data);
		
		uint16 tp, rh;
        uint8 *data;
        uint32 tp_t, rh_t;
        data = (uint8 *)user_mvh3004_get_poweron_th();

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


/******************************************************************************
 * FunctionName : user_humiture_init
 * Description  : init humiture function, include key and mvh3004
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_sensor_init(uint8 active)
{

    user_link_led_init();

    wifi_status_led_install(SENSOR_WIFI_LED_IO_NUM, SENSOR_WIFI_LED_IO_MUX, SENSOR_WIFI_LED_IO_FUNC);

    if (wifi_get_opmode() != SOFTAP_MODE) {
        single_key[0] = key_init_single(SENSOR_KEY_IO_NUM, SENSOR_KEY_IO_MUX, SENSOR_KEY_IO_FUNC,
                                        user_sensor_long_press, NULL);

        keys.key_num = SENSOR_KEY_NUM;
        keys.single_key = single_key;

        key_init(&keys);

        if (GPIO_INPUT_GET(GPIO_ID_PIN(SENSOR_KEY_IO_NUM)) == 0) {
            user_sensor_long_press();
        }
    }

#if HUMITURE_SUB_DEVICE
    user_mvh3004_init();
    user_mvh3004_read_th(humiture_data);
#endif

#ifdef SENSOR_DEEP_SLEEP
    if (wifi_get_opmode() != STATIONAP_MODE) {
        if (active == 1) {
            user_sensor_deep_sleep_init(SENSOR_DEEP_SLEEP_TIME / 1000 );
        } else {
            user_sensor_deep_sleep_init(SENSOR_DEEP_SLEEP_TIME / 1000 / 3 * 2);
        }
    }
#endif

	os_timer_disarm(&sensor_upload_timer);
    os_timer_setfn(&sensor_upload_timer, (os_timer_func_t *)upload_status, NULL);
    os_timer_arm(&sensor_upload_timer, 10000, 1);

}
#endif

