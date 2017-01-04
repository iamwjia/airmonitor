/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_main.c
 *
 * Description: entry file of user application
 *
 * Modification history:
 *     2014/12/1, v1.0 create this file.
*******************************************************************************/
#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"


#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "driver/uart.h"
#include "espressif/esp_system.h"
#include "et_types.h"

#include "driver/i2c_master.h"
#include "driver/OLED_I2C.h"

#include "driver/RGB_light.h"
#include "driver/delay.h"

#include "user_config.h"
#include "driver/gpio.h"
#include "espressif/smartconfig.h"
#include "driver/i2s.h"
#include "et_client.h"

extern et_cloud_handle g_cloud_handle;
extern void  et_user_main(void *pvParameters);
extern void read_uart_buf_task(void *pvParameters);
extern et_int32 to_stop_app;
extern et_int32 to_stop_ilink;
extern et_int32 to_stop_server;
extern et_int32 to_stop_netsend;

LOCAL os_timer_t test_timer;
//xQueueHandle xQueueSendToMQTT=NULL;

//extern volatile et_int32 toStop;
//extern volatile et_int32 toStopCloud;
//extern volatile et_int32 to_stop_app;

et_int32 audio_stop_record_flag=0;
volatile et_int32 wifi_reconnect_start_flag = 0;


void ICACHE_FLASH_ATTR
smartconfig_done(sc_status status, void *pdata)
{
	switch(status) 
	{
		case SC_STATUS_WAIT:
	    		os_printf("SC_STATUS_WAIT\n");
	    		break;
				
		case SC_STATUS_FIND_CHANNEL:
			user_set_wifi_led_delay(100);
	    		os_printf("SC_STATUS_FIND_CHANNEL\n");
	    		break;
				
		case SC_STATUS_GETTING_SSID_PSWD:
	    		os_printf("SC_STATUS_GETTING_SSID_PSWD\n");
				
	    		sc_type *type = pdata;
		    	if (*type == SC_TYPE_ESPTOUCH) 
			{
		      		os_printf("SC_TYPE:SC_TYPE_ESPTOUCH\n");
		    	} 
			else 
			{
		        	os_printf("SC_TYPE:SC_TYPE_AIRKISS\n");
			}
			break;
			
		case SC_STATUS_LINK: 
		{
			user_set_wifi_led_delay(300);
			printf("SC_STATUS_LINK\n");
			struct station_config *sta_conf = pdata;
			wifi_reconnect_start_flag = 3;
			wifi_station_set_config(sta_conf);
			wifi_station_disconnect();
			wifi_station_connect();
		}
			break;
			
		case SC_STATUS_LINK_OVER: {
			printf("SC_STATUS_LINK_OVER\n");
			user_set_wifi_led_on();
			smartconfig_stop();

			OLED_clear();
			OLED_show_chn(0, 0, 15);   //show Сe:
			OLED_show_str(18, 0, "e:", 2);
			OLED_show_chn(0, 2, 8);    //show 
			OLED_show_chn(18, 2, 9);
			OLED_show_chn(36, 2, 10);
			OLED_show_chn(54, 2, 11);
			OLED_show_chn(72, 2, 13);
			OLED_show_chn(90, 2, 14);
			OLED_show_str(108, 2, "  ", 2);

			delay_s(2);
			system_restart();
			break;
		}
	}

}

LOCAL void 
airkiss_key_init(key_gpio_t*key)
{
	et_uint32 io_reg;

	io_reg = GPIO_PIN_REG(key->key_num);

	PIN_PULLUP_EN(io_reg);
	PIN_FUNC_SELECT(io_reg, 0);
	GPIO_AS_INPUT(key->key_gpio_pin);
}

void ICACHE_FLASH_ATTR
airkiss_key_poll_task(void *pvParameters)
{
	et_uint32 value, i;
	
	while(1) 
	{
		value = gpio_get_value(AIRKISS_KEY_IO_NUM);
		// os_printf("Airkiss check ...\n");
		if(!value) 
		{
			delay_s(1);
			value = gpio_get_value(AIRKISS_KEY_IO_NUM);
			if(!value) 
			{
				os_printf("begin to airkiss\n");
				//toStop = 1;  	//in airkiss mode, stop et_user_main thread
				//toStopCloud = 1;
				//to_stop_app = 1;
				os_printf("Close iLink service....\n");
				et_disconnect(g_cloud_handle);
#if ET_CONFIG_SERVER_EN
				et_stop_server(g_cloud_handle);
				to_stop_server = 1;
#endif
				to_stop_app = 1;
				to_stop_ilink = 1;
				to_stop_netsend = 1;
				// delay_s(1);
				et_destroy_context(&g_cloud_handle);
				wifi_reconnect_start_flag = 0;
				smartconfig_start(smartconfig_done); 	//airkiss start
				user_set_wifi_led_delay(500);
				
				OLED_clear();
				OLED_show_chn(0, 0, 15);    //show Сe:
				OLED_show_str(18, 0, "e:", 2);
				OLED_show_chn(0, 2, 8);    //show ����������...
				OLED_show_chn(18, 2, 9);
				OLED_show_chn(36, 2, 10);
				OLED_show_chn(54, 2, 11);
				OLED_show_chn(72, 2, 12);
				OLED_show_str(90, 2, "...", 2);
				//break;
			}
		}
		else
			vTaskDelay(500);
	}

	os_printf("end airkiss\n");
	vTaskDelete(NULL);
}

void ICACHE_FLASH_ATTR
user_show_logo()
{	
	extern et_uchar BMP1[];
	et_uint32 len = 1024;	// BMP1 member
	
	i2c_master_gpio_init(); // I2C init
	OLED_init(); 			// OLED init
	OLED_clear();

	// show logo
	OLED_show_bmp(0, 0, 128, 8, BMP1, len);
//	delay_s(4);
//	OLED_clear();
}

void et_wifi_event_cb(System_Event_t *event)
{
	switch(event->event_id)
	{
		case EVENT_STAMODE_SCAN_DONE: //ESP8266 station finish scanning AP
			
			break;
		case EVENT_STAMODE_CONNECTED: //ESP8266 station connected to AP
			user_set_wifi_led_on();
			printf("----------------et connect to ssid %s, channel %d\n", event->event_info.connected.ssid, event->event_info.connected.channel);
			break;
		case EVENT_STAMODE_DISCONNECTED: //ESP8266 station disconnected to AP
			user_set_wifi_led_off();
			if(true != wifi_station_get_reconnect_policy())
			{
				os_printf("et wifi set to reconnect\n");
				wifi_station_set_reconnect_policy(true);
			}
			printf("++++++++++++++++++et disconnect from ssid %s, reason %d\n", event->event_info.disconnected.ssid, event->event_info.disconnected.reason);
			if(wifi_reconnect_start_flag != 1)
			{
				os_printf("airkiss start or start first don't restart %d\n",wifi_reconnect_start_flag);
				if(wifi_reconnect_start_flag == 3)
				{
					smartconfig_stop();
					os_printf("airkiss can't connect , stop it\n");
				}
			}
			else
			{
				os_printf("et wifi station connect status %d, restart system\n",wifi_station_get_connect_status());
				system_restart();
			}
			break;
		case EVENT_STAMODE_AUTHMODE_CHANGE: //the auth mode of AP connected by ESP8266 station changed
			printf("mode: %d -> %d\n", event->event_info.auth_change.old_mode, event->event_info.auth_change.new_mode);
			break;
		case EVENT_STAMODE_GOT_IP: //ESP8266 station got IP from connected AP
			//printf("ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR, IP2STR(&event->event_info.got_ip.ip), IP2STR(&event->event_info.got_ip.mask), 
			//	IP2STR(&event->event_info.got_ip.gw));
			break;
//		case EVENT_STAMODE_DHCP_TIMEOUT: //ESP8266 station dhcp client got IP timeout
//			break;
		case EVENT_SOFTAPMODE_STACONNECTED: //a station connected to ESP8266 soft-AP
			printf("et station: " MACSTR "join, AID = %d\n", MAC2STR(event->event_info.sta_connected.mac), event->event_info.sta_connected.aid);
			break;
		case EVENT_SOFTAPMODE_STADISCONNECTED: //a station disconnected to ESP8266 soft-AP
			printf("et station: " MACSTR "leave, AID = %d\n", MAC2STR(event->event_info.sta_disconnected.mac), event->event_info.sta_disconnected.aid);
			break;
//		case EVENT_SOFTAPMODE_PROBEREQRECVED:
//			break;
//		case EVENT_MAX:
//			break;
		default:
			break;
	}
}
/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 user_rf_cal_sector_set(void)
{
    flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}
/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void)
{	
	key_gpio_t key;
	struct station_config config;
//	struct ip_info info;
	
    os_printf("now SDK version:%s\n", system_get_sdk_version());
	///< Init hardware
	RGB_light_init();		///< RGB init
	DHT11_init();
	i2c_master_gpio_init(); ///< I2C init
	OLED_init(); 			///< OLED init
	OLED_clear();
	///< show logo
	user_show_logo();

	///< wifi led control
	if(pdTRUE != xTaskCreate(user_wifi_led_control, "wifi_led_control", 256, NULL, 8, NULL))
		os_printf("Create user wifi led task failed\n");
	///< wifi event handle
	wifi_set_event_handler_cb(et_wifi_event_cb);
//	wifi_status_led_install(WIFI_STATUS_IO_NUM, WIFI_STATUS_IO_MUX, WIFI_STATUS_IO_FUNC);
	wifi_set_opmode(STATION_MODE);

	memset(&config, 0, sizeof(struct station_config));
	if(wifi_station_get_config_default(&config) == true)///< Read wifi station parameters which store in falsh para space
	{
		os_printf("SSID:%s \n",config.ssid);
		os_printf("AP_MAC:"MACSTR"\n", MAC2STR(config.bssid));
		wifi_station_set_config_current(&config);
	}
	
	wifi_reconnect_start_flag = 0;
	memset(&key, 0, sizeof(key_gpio_t));
	key.key_gpio_pin = AIRKISS_KEY_IO_PIN;
	key.key_num = AIRKISS_KEY_IO_NUM;
	airkiss_key_init(&key);
	if(pdTRUE != xTaskCreate(airkiss_key_poll_task, "smartconfig_task", 256, NULL, 8, NULL))
		os_printf("Create airkiss task failed\n");

	if(pdTRUE != xTaskCreate(et_user_main, "et_user_main", 512, (void *)&wifi_reconnect_start_flag, 7, NULL))
		os_printf("Create user main task failed\n");

    //   xQueueReadBuf = xQueueCreate(32, sizeof(os_event_t));	
//	xTaskCreate(read_uart_buf_task, (uint8 const *)"rbTask", 512, NULL, tskIDLE_PRIORITY + 2, &xReadBufTaskHandle);
}
