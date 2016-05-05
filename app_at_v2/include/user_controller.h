#ifndef __USER_CONTROLLER_H__
#define __USER_CONTROLLER_H__

#include "user_config.h"
#include "driver/key.h"

#define SENSOR_DEEP_SLEEP_TIME 10000000

#if CONTROL_DEVICE
#define HUMITURE_SUB_DEVICE         0
#endif

///////////////////////
#define CONTROL_KEY_NUM    4

#define SENSOR_POWER_IO_MUX     PERIPHS_IO_MUX_GPIO2_U
#define SENSOR_POWER_IO_NUM     2
#define SENSOR_POWER_IO_FUNC    FUNC_GPIO2

#define SENSOR_KEY1_IO_MUX     PERIPHS_IO_MUX_GPIO5_U
#define SENSOR_KEY1_IO_NUM     5
#define SENSOR_KEY1_IO_FUNC    FUNC_GPIO5

#define SENSOR_KEY2_IO_MUX     PERIPHS_IO_MUX_SD_DATA3_U
#define SENSOR_KEY2_IO_NUM     10
#define SENSOR_KEY2_IO_FUNC    FUNC_GPIO10

#define SENSOR_KEY3_IO_MUX     PERIPHS_IO_MUX_GPIO4_U
#define SENSOR_KEY3_IO_NUM     4
#define SENSOR_KEY3_IO_FUNC    FUNC_GPIO4

#define SENSOR_KEY4_IO_MUX     PERIPHS_IO_MUX_SD_DATA2_U
#define SENSOR_KEY4_IO_NUM     9
#define SENSOR_KEY4_IO_FUNC    FUNC_GPIO9
///////////////////////

void user_controller_init(uint8 active);
void user_led_off();
void user_down_power();
void user_led_on();


#endif

