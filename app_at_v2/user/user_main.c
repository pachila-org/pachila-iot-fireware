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

#include "osapi.h"
#include "at_custom.h"
#include "user_interface.h"
#include "driver/uart.h"
#include "at_mqtt_cmd.h"

// test :AT+TEST=1,"abc"<,3>
void ICACHE_FLASH_ATTR
at_setupCmdTest(uint8_t id, char *pPara)
{
    int result = 0, err = 0, flag = 0;
    uint8 buffer[32] = {0};
    pPara++; // skip '='

    //get the first parameter
    // digit
    flag = at_get_next_int_dec(&pPara, &result, &err);

    // flag must be ture because there are more parameter
    if (flag == FALSE) {
        at_response_error();
        return;
    }

    if (*pPara++ != ',') { // skip ','
        at_response_error();
        return;
    }

    os_sprintf(buffer, "the first parameter:%d\r\n", result);
    at_port_print(buffer);

    //get the second parameter
    // string
    at_data_str_copy(buffer, &pPara, 10);
    at_port_print("the second parameter:");
    at_port_print(buffer);
    at_port_print("\r\n");

    if (*pPara == ',') {
        pPara++; // skip ','
        result = 0;
        //there is the third parameter
        // digit
        flag = at_get_next_int_dec(&pPara, &result, &err);
        // we donot care of flag
        os_sprintf(buffer, "the third parameter:%d\r\n", result);
        at_port_print(buffer);
    }

    if (*pPara != '\r') {
        at_response_error();
        return;
    }

    at_response_ok();
}

void ICACHE_FLASH_ATTR
at_testCmdTest(uint8_t id)
{
    uint8 buffer[32] = {0};

    os_sprintf(buffer, "%s\r\n", "at_testCmdTest");
    at_port_print(buffer);
    at_response_ok();
}

void ICACHE_FLASH_ATTR
at_queryCmdTest(uint8_t id)
{
    uint8 buffer[32] = {0};

    os_sprintf(buffer, "%s\r\n", "at_queryCmdTest");
    at_port_print(buffer);
    at_response_ok();
}

void ICACHE_FLASH_ATTR
at_exeCmdTest(uint8_t id)
{
    uint8 buffer[32] = {0};

    os_sprintf(buffer, "%s\r\n", "at_exeCmdTest");
    at_port_print(buffer);
    at_response_ok();
}

extern void at_exeCmdCiupdate(uint8_t id);
at_funcationType at_custom_cmd[] = {
    {"+TEST", 5, at_testCmdTest, at_queryCmdTest, at_setupCmdTest, at_exeCmdTest},

#if 1
    {"+MQTT_CONF", 10, NULL, NULL, at_mqtt_SetupCmdConf, NULL},
    {"+MQTT_CONN", 10, NULL, NULL, NULL, at_mqtt_exeCmdConn},
    {"+MQTT_SUB", 9,NULL, NULL, at_mqtt_setupCmdSub, NULL},
    {"+MQTT_PUB", 9,NULL , NULL, at_mqtt_SetupCmdPub, NULL},
    //{"+MQTT_REG", 9, NULL, NULL, NULL, at_mqtt_exeCmdReg},
    {"+MQTT_CNWIFI", 12, NULL, NULL, at_mqtt_SetupCmdCnwifi, NULL},
    {"+AIRKISS_START", 14, NULL, NULL, NULL,at_AirkissStart},
    {"+ESPTOUCH_START", 15, NULL, NULL, NULL,at_EsptouchStart},
    {"+MQTT_DISC", 10, NULL, NULL, NULL, at_mqtt_exeCmdDisc},
    {"+SCFG_STOP", 10, NULL, NULL, NULL,at_SmartconfigStop},
#endif

#ifdef AT_UPGRADE_SUPPORT
    {"+CIUPDATE", 9,       NULL,            NULL,            NULL, at_exeCmdCiupdate}
#endif
};
os_timer_t test_t;

void print_heap()
{
    os_printf("HEAP: %d \r\n",system_get_free_heap_size());
	

}
void user_rf_pre_init(void)
{
}

void user_init(void)
{
    wifi_set_opmode(STATIONAP_MODE);
	wifi_station_set_auto_connect(0);
    char buf[64] = {0};
    //at_customLinkMax = 5;
    at_init();
	os_install_putc1((void *)uart0_write_char_t);
	
    os_sprintf(buf,"compile time:%s %s",__DATE__,__TIME__);
    //at_set_custom_info(buf);
    at_port_print("\r\nready\r\n");
    at_cmd_array_regist(&at_custom_cmd[0], sizeof(at_custom_cmd)/sizeof(at_custom_cmd[0]));

    //os_timer_disarm(&test_t);
	//os_timer_setfn(&test_t,print_heap,NULL);
	//os_timer_arm(&test_t,1000,1);
#if LIGHT_DEVICE
	#if MQTT_LIGHT_DEVICE
	user_light_init();
	#endif
#elif PLUG_DEVICE
	user_plug_init();
#elif SENSOR_DEVICE
	user_sensor_init(0);
#elif CONTROL_DEVICE
	user_controller_init(0);
#endif
	mqtt_Init();
	
}
