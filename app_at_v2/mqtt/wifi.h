/*
 * wifi.h
 *
 *  Created on: Dec 30, 2014
 *      Author: Minh
 */

#ifndef USER_WIFI_H_
#define USER_WIFI_H_
#include "os_type.h"

typedef enum{
	HTTP_IDLE,
	DEV_ACTIVE,
	DEV_CANCELL,
	DEV_OTA,
} HTTP_STAU;

extern HTTP_STAU httpStatus;

typedef void (*WifiCallback)(uint8_t);
void ICACHE_FLASH_ATTR WIFI_Connect(uint8_t* ssid, uint8_t* pass, WifiCallback cb);
void ICACHE_FLASH_ATTR disable_check_ip(void);
void ICACHE_FLASH_ATTR http_connect(void);

#endif /* USER_WIFI_H_ */
