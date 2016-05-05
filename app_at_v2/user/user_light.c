/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_light.c
 *
 * Description: light demo's function realization
 *
 * Modification history:
 *     2014/5/1, v1.0 create this file.
*******************************************************************************/
#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "mem.h"
#include "user_interface.h"

#include "user_light.h"
#include "pwm.h"
#include "user_config.h"

#if MQTT_LIGHT_DEVICE

struct light_saved_param light_param;

/******************************************************************************
 * FunctionName : user_light_get_duty
 * Description  : get duty of each channel
 * Parameters   : uint8 channel : LIGHT_RED/LIGHT_GREEN/LIGHT_BLUE
 * Returns      : NONE
*******************************************************************************/
uint32 ICACHE_FLASH_ATTR
user_light_get_duty(uint8 channel)
{
    return light_param.pwm_duty[channel];
}

/******************************************************************************
 * FunctionName : user_light_set_duty
 * Description  : set each channel's duty params
 * Parameters   : uint8 duty    : 0 ~ PWM_DEPTH
 *                uint8 channel : LIGHT_RED/LIGHT_GREEN/LIGHT_BLUE
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_light_set_duty(uint32 duty, uint8 channel)
{
    if (duty != light_param.pwm_duty[channel]) {
        pwm_set_duty(duty, channel);

        light_param.pwm_duty[channel] = pwm_get_duty(channel);
    }
}

/******************************************************************************
 * FunctionName : user_light_get_period
 * Description  : get pwm period
 * Parameters   : NONE
 * Returns      : uint32 : pwm period
*******************************************************************************/
uint32 ICACHE_FLASH_ATTR
user_light_get_period(void)
{
    return light_param.pwm_period;
}

/******************************************************************************
 * FunctionName : user_light_set_duty
 * Description  : set pwm frequency
 * Parameters   : uint16 freq : 100hz typically
 * Returns      : NONE
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_light_set_period(uint32 period)
{
    if (period != light_param.pwm_period) {
        pwm_set_period(period);

        light_param.pwm_period = pwm_get_period();
    }
}

void ICACHE_FLASH_ATTR
user_light_restart(void)
{
	spi_flash_erase_sector(PRIV_PARAM_START_SEC + PRIV_PARAM_SAVE);
	spi_flash_write((PRIV_PARAM_START_SEC + PRIV_PARAM_SAVE) * SPI_FLASH_SEC_SIZE,
	    		(uint32 *)&light_param, sizeof(struct light_saved_param));

	pwm_start();
}

void ICACHE_FLASH_ATTR
user_light_cancel(void)
{
	os_printf("user_light_cancel\n");
	light_set_aim(      
		         		20000,
    	                0,
    	                0, 
    	                0,
    	                0,
    	                1000);
}


LOCAL os_timer_t conect_timer;

void ICACHE_FLASH_ATTR
conect_ap_cb(void*p)
{
	light_set_aim(      
		         		0,
    	                0,
    	                0, 
    	                20000,
    	                20000,
    	                1000);
}


void ICACHE_FLASH_ATTR
user_light_connect_ap(void)
{
	light_set_aim(      
		         		0,
    	                0,
    	                20000, 
    	                0,
    	                0,
    	                1000);
	os_timer_disarm(&conect_timer);
	os_timer_setfn(&conect_timer, conect_ap_cb, NULL);
	os_timer_arm(&conect_timer,2000,0);
	os_printf("user_light_connect_ap\n");
}

/******************************************************************************
 * FunctionName : user_light_init
 * Description  : light demo init, mainy init pwm
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_light_init(void)
{

	uint32 io_info[][3] = {   {PWM_0_OUT_IO_MUX,PWM_0_OUT_IO_FUNC,PWM_0_OUT_IO_NUM},
		                      {PWM_1_OUT_IO_MUX,PWM_1_OUT_IO_FUNC,PWM_1_OUT_IO_NUM},
		                      {PWM_2_OUT_IO_MUX,PWM_2_OUT_IO_FUNC,PWM_2_OUT_IO_NUM},
		                      {PWM_3_OUT_IO_MUX,PWM_3_OUT_IO_FUNC,PWM_3_OUT_IO_NUM},
		                      {PWM_4_OUT_IO_MUX,PWM_4_OUT_IO_FUNC,PWM_4_OUT_IO_NUM},
		                      };
	
    uint32 pwm_duty_init[PWM_CHANNEL] = {0};
	os_memcpy(light_param.pwm_duty,pwm_duty_init,sizeof(pwm_duty_init));
	
	light_param.pwm_period = 1000;
	
    /*PIN FUNCTION INIT FOR PWM OUTPUT*/
    pwm_init(light_param.pwm_period,  pwm_duty_init ,PWM_CHANNEL,io_info);
    
    os_printf("LIGHT PARAM: R: %d \r\n",light_param.pwm_duty[LIGHT_RED]);
    os_printf("LIGHT PARAM: G: %d \r\n",light_param.pwm_duty[LIGHT_GREEN]);
    os_printf("LIGHT PARAM: B: %d \r\n",light_param.pwm_duty[LIGHT_BLUE]);
    if(PWM_CHANNEL>LIGHT_COLD_WHITE){
        os_printf("LIGHT PARAM: CW: %d \r\n",light_param.pwm_duty[LIGHT_COLD_WHITE]);
        os_printf("LIGHT PARAM: WW: %d \r\n",light_param.pwm_duty[LIGHT_WARM_WHITE]);
    }
    os_printf("LIGHT PARAM: P: %d \r\n",light_param.pwm_period);

    //uint32 light_init_target[8]={0};
    //os_memcpy(light_init_target,light_param.pwm_duty,sizeof(light_param.pwm_duty));

    light_set_aim(      
		         pwm_duty_init[LIGHT_RED],
    	                pwm_duty_init[LIGHT_GREEN],
    	                pwm_duty_init[LIGHT_BLUE], 
    	                20000,
    	                20000,
    	                light_param.pwm_period);
	
    set_pwm_debug_en(0);//disable debug print in pwm driver
    os_printf("PWM version : %08x \r\n",get_pwm_version());
}
#endif

