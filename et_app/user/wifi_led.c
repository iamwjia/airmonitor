#include "esp_common.h"
#include "driver/gpio.h"
#include "driver/delay.h"
#include "user_config.h"


LOCAL et_uint8 wifi_led_thread = 1;
LOCAL et_uint8 wifi_led_mode = 0;
LOCAL et_uint16 wifi_led_delay = 0;


void user_wifi_led_thread_exit()
{
	printf("set wifi led thread exit\n");
	wifi_led_thread = 0;
}

void user_set_wifi_led_delay(et_uint16 ms)
{
	printf("set wifi led delay = %d ms\n", ms);
	if (wifi_led_delay != ms)
	{
		wifi_led_delay = ms;
	}	
	wifi_led_mode = WIFI_LED_DELAY_Y;
}

void user_set_wifi_led_on()
{
	GPIO_OUTPUT_SET(WIFI_STATUS_IO_NUM, LOGIC_LL);
	wifi_led_mode = WIFI_LED_DELAY_N;
}


void user_set_wifi_led_off()
{
	GPIO_OUTPUT_SET(WIFI_STATUS_IO_NUM, LOGIC_HL);	
	wifi_led_mode = WIFI_LED_DELAY_N;
}


void user_wifi_led_control(void *param)
{
	printf("wifi led thread will be start\n");
	gpio_set_output(WIFI_STATUS_IO_PIN, WIFI_STATUS_IO_NUM);
	GPIO_OUTPUT_SET(WIFI_STATUS_IO_NUM, LOGIC_HL);	
	
	while (wifi_led_thread)
	{
		if (WIFI_LED_DELAY_Y != wifi_led_mode)
		{
			vTaskDelay(100);
			continue;
		}
		
		GPIO_OUTPUT_SET(WIFI_STATUS_IO_NUM, LOGIC_LL);
		vTaskDelay(wifi_led_delay);
		GPIO_OUTPUT_SET(WIFI_STATUS_IO_NUM, LOGIC_HL);
		vTaskDelay(wifi_led_delay);
	}

	printf("wifi led thread will be stop\n");
	vTaskDelete(NULL);
}

