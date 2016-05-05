/*
/* config.c
*
* Copyright (c) 2014-2015, Tuan PM <tuanpm at live dot com>
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
* * Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
* * Neither the name of Redis nor the names of its contributors may be used
* to endorse or promote products derived from this software without
* specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
* ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
* LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
* POSSIBILITY OF SUCH DAMAGE.
*/
#include "ets_sys.h"
#include "os_type.h"
#include "mem.h"
#include "osapi.h"
#include "user_interface.h"

#include "mqtt.h"
#include "config.h"
#include "user_config.h"
#include "debug.h"
#include "../mqtt/wifi.h"

SYSCFG sysCfg;
SAVE_FLAG saveFlag;

#define DEV_MAC_ID "%02x%02x%02x%02x%02x%02x"


void ICACHE_FLASH_ATTR
CFG_Save()
{

    spi_flash_read((CFG_LOCATION + 3) * SPI_FLASH_SEC_SIZE,(uint32 *)&saveFlag, sizeof(SAVE_FLAG));  
    if (saveFlag.flag == 0) {
        spi_flash_erase_sector(CFG_LOCATION + 1);
        spi_flash_write((CFG_LOCATION + 1) * SPI_FLASH_SEC_SIZE,
        (uint32 *)&sysCfg, sizeof(SYSCFG));
        saveFlag.flag = 1;
        spi_flash_erase_sector(CFG_LOCATION + 3);
        spi_flash_write((CFG_LOCATION + 3) * SPI_FLASH_SEC_SIZE,
        (uint32 *)&saveFlag, sizeof(SAVE_FLAG));
		
    } else {
        spi_flash_erase_sector(CFG_LOCATION + 0);
        spi_flash_write((CFG_LOCATION + 0) * SPI_FLASH_SEC_SIZE,
        (uint32 *)&sysCfg, sizeof(SYSCFG));
        saveFlag.flag = 0;
        spi_flash_erase_sector(CFG_LOCATION + 3);
        spi_flash_write((CFG_LOCATION + 3) * SPI_FLASH_SEC_SIZE,
        (uint32 *)&saveFlag, sizeof(SAVE_FLAG));
    }
	
}

void ICACHE_FLASH_ATTR 
CFG_RESET()
{
	char hwaddr[6];
	char post_id[30];
    #if 1
    sysCfg.cfg_holder = 0;
    sysCfg.register_if=0;
	os_memset(&sysCfg,0,sizeof(sysCfg));
#if 1
	wifi_get_macaddr(STATION_IF, hwaddr);
	httpStatus = DEV_CANCELL; //dis activation 
	sysCfg.security = DEFAULT_SECURITY;	/* default non ssl */
    sysCfg.mqtt_keepalive = MQTT_KEEPALIVE;
	os_bzero(post_id, sizeof(post_id));
	os_sprintf(post_id, DEV_MAC_ID, MAC2STR(hwaddr));
	os_memcpy(sysCfg.device_id, post_id, sizeof(post_id));
	os_memcpy(sysCfg.mqtt_user, MQTT_USER, sizeof(MQTT_USER));
	os_memcpy(sysCfg.mqtt_pass, MQTT_PASS, sizeof(MQTT_PASS));
#endif
    CFG_Save();
    #else
    spi_flash_erase_sector(CFG_LOCATION);
    spi_flash_erase_sector(CFG_LOCATION+1);
    spi_flash_erase_sector(CFG_LOCATION+2);
    spi_flash_erase_sector(CFG_LOCATION+3);
    #endif
}


void ICACHE_FLASH_ATTR
CFG_Load()
{
    INFO("\r\nload ...\r\n");
    spi_flash_read((CFG_LOCATION + 3) * SPI_FLASH_SEC_SIZE,(uint32 *)&saveFlag, sizeof(SAVE_FLAG));
    if (saveFlag.flag == 0) {
        spi_flash_read((CFG_LOCATION + 0) * SPI_FLASH_SEC_SIZE,(uint32 *)&sysCfg, sizeof(SYSCFG));
    } else {
        spi_flash_read((CFG_LOCATION + 1) * SPI_FLASH_SEC_SIZE,(uint32 *)&sysCfg, sizeof(SYSCFG));
    }

#if 0
    if(sysCfg.cfg_holder != CFG_HOLDER){
        os_memset(&sysCfg, 0x00, sizeof sysCfg);
        //sysCfg.cfg_holder = CFG_HOLDER;
        //os_sprintf(sysCfg.sta_ssid, "%s", STA_SSID);
        //os_sprintf(sysCfg.sta_pwd, "%s", STA_PASS);
        //sysCfg.sta_type = STA_TYPE;
        os_sprintf(sysCfg.device_id, MQTT_CLIENT_ID, system_get_chip_id());
        os_sprintf(sysCfg.mqtt_host, "%s", MQTT_HOST);
        sysCfg.mqtt_port = MQTT_PORT;
        os_sprintf(sysCfg.mqtt_user, "%s", MQTT_USER);
        os_sprintf(sysCfg.mqtt_pass, "%s", MQTT_PASS);
        sysCfg.security = DEFAULT_SECURITY;	/* default non ssl */
        sysCfg.mqtt_keepalive = MQTT_KEEPALIVE;
        INFO(" default configuration\r\n");
        CFG_Save();
    }
#endif

    if(sysCfg.cfg_holder == CFG_HOLDER & sysCfg.auth_if == 1){
        INFO("======================\r\n");
        INFO("saveFlag.flag: %d \r\n holder : 0x%08x\r\ndevice register flg: %d\r\n",saveFlag.flag,sysCfg.cfg_holder,sysCfg.register_if);
        INFO("sysCfg.device_id : %s \r\n", sysCfg.device_id);
        INFO("sysCfg.mqtt_host : %s \r\n", sysCfg.mqtt_host);
        
        INFO("sysCfg.mqtt_keepalive : %d \r\n", sysCfg.mqtt_keepalive);
        INFO("sysCfg.mqtt_pass : %s \r\n", sysCfg.mqtt_pass);
        INFO("sysCfg.mqtt_port : %d \r\n", sysCfg.mqtt_port);
        INFO("sysCfg.mqtt_user : %s \r\n", sysCfg.mqtt_user);
        INFO("sysCfg.auth_if : %d \r\n", sysCfg.auth_if);
        INFO("sysCfg.security : %d \r\n", sysCfg.security);
        INFO("sysCfg.sta_pwd : %s \r\n", sysCfg.sta_pwd);
        INFO("sysCfg.sta_ssid : %s \r\n", sysCfg.sta_ssid);
        INFO("sysCfg.rst_flag : %d \r\n", sysCfg.rst_flag);
        INFO("======================\r\n");
    }else{
        INFO("sysCfg.cfg_holder : 0x%08x\r\n",sysCfg.cfg_holder);
		INFO("sysCfg RESET\r\n");
		CFG_RESET();
    }
}

void ICACHE_FLASH_ATTR 
CFG_SET(uint8* host_ip, uint32 host_port,bool security,uint32 keep_alive,uint8* client_id,uint8* password,uint8* user_id)
{
    //os_memset(&sysCfg, 0x00, sizeof sysCfg);
    os_bzero(sysCfg.device_id,sizeof(sysCfg.device_id));
	os_bzero(sysCfg.mqtt_user,sizeof(sysCfg.mqtt_user));
	os_bzero(sysCfg.mqtt_host,sizeof(sysCfg.mqtt_host));
	os_bzero(sysCfg.mqtt_pass,sizeof(sysCfg.mqtt_pass));
	
    os_sprintf(sysCfg.device_id, "%s", client_id);
	os_sprintf(sysCfg.mqtt_user, "%s", user_id);
    os_sprintf(sysCfg.mqtt_host, "%s", host_ip);
	os_sprintf(sysCfg.mqtt_pass,"%s",	password);
	
    sysCfg.mqtt_port = host_port;
    sysCfg.security = security;	/* default non ssl */
    sysCfg.mqtt_keepalive = keep_alive;
    INFO(" set configurations\r\n");
    CFG_Save();
    INFO("save config info to flash\r\n");
}



