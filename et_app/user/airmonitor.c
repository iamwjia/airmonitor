/*******************************************************************************
 * Copyright (c) 2012, 2013 Beidouapp Corp.
 *
 * All rights reserved. 
 *
 * Contributors:
 *    Peter Peng - initial contribution
 *******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "esp_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "driver/uart.h"

#include "user_config.h"
#include "et_client.h"
#include "lwip/netdb.h"
#include "et_api_compatible.h"
#include "json/cJSON.h"

#define MAX_USER_ID ET_USER_ID_LEN_MAX
#define UART_MAX_READ_BUFFER  512
//#define UART_MAX_SEND_BUFFER  1024
#define MAX_ILINK_CONNECT  5
#define DEBUG_PRINT_PAYLOAD 0
#define PORT 8085

#define ET_ISDIGIT(a)       ((a) >= '0' && (a) <= '9')          ///< 检查是否为数字
#define ET_ISHEXLOWER(a)	((a) >= 'a' && (a) <= 'f')			///< 检查是否为小写Hex字符
#define ET_ISHEXUPPER(a)	((a) >= 'A' && (a) <= 'F')			///< 检查是否为大写Hex字符

typedef struct{
	et_int8 uid[MAX_USER_ID];
	et_int8 appkey[32];
	et_int8 secretkey[64];
	et_int8 server[32];
}hardware_info_type;

typedef struct{
	et_int32 start;
	et_uint32 count;
	et_uint32 num;
	et_uint32 send_num;
	et_uint32 recv_num;
	et_uint32 error_num;
	et_int8 *id;
	et_uint8 *data;
	et_uint32 data_len;
	et_uint8 hex;
	et_uint8 method;
	et_uint8 sendtype;
	et_uint8 qos;
	et_uint32 time;
}et_loop_send_config_t;

struct ilink_connect_client
{
	et_int32 state;
	et_char user_id[MAX_USER_ID];

};

static struct ilink_connect_client g_ilink_connect_clients[MAX_ILINK_CONNECT];
static et_uint8 g_net_cmd_buf_recv[1500] = {0};
static et_uint8 g_net_cmd_buf_send[1500] = {0};
static et_uint8 g_data_buf[1500] = {0};
static et_uint32 g_data_buf_size = 0;
static et_addr_info_t remote_info = {0};
static et_dfs_file_info_type g_file_info = {0};

et_char g_user_id[ET_USER_ID_LEN_MAX] = {0};
et_char g_group_id[ET_USER_ID_LEN_MAX] = {0};
et_char g_topic[ET_TOPIC_LEN_MAX] = {0};
et_uint32 g_group_message = 0;
et_cloud_handle g_cloud_handle = NULL;
et_cloud_connect_type g_cloud_con_para = {ET_TRUE, 10};
hardware_info_type hardware_info = {0};
et_loop_send_config_t et_loop_test = {0};
et_uint32 write_flash_end=0;
et_int64 file_total_size=0;
et_uint32 audio_voice_data=0;
et_uint16 sector=AUDIO_FLASH_START_SECTOR;

volatile et_int32 to_stop_app = 1;
volatile et_int32 to_stop_ilink = 1;
volatile et_int32 to_stop_server = 1;
volatile et_int32 to_stop_netsend = 1;

extern et_uint32 user_get_run_mode();
extern et_int32 read_uart_data_flag;
extern xQueueHandle xQueueUart;

void print_hex(et_uchar *label, et_uchar *str, et_int32 len)
{
	et_int32 i;
	
	// ET_LOG_USER("%s ", label);
	os_printf("%s ", label);
	for(i = 0; i < len; i++)
		os_printf("%02x ", str[i]);
	os_printf("\n");
}

void print_string_len(et_char *label, et_char *string, et_uint32 len)
{
	et_char *string_buf = NULL;
	string_buf = malloc(len+1);
	if(NULL != string_buf)
	{
		memset(string_buf, 0, len+1);
		strncpy(string_buf, string, len);
		// ET_LOG_USER("%s %s\n", label, string_buf);
		os_printf("%s %s\n", label, string_buf);
		free(string_buf);
	}
}

//接收消息时间戳，fromuid，topic/群号，length，消息前25个字节
et_uint32 data_print_format = 0;	//0-Ascii格式打印数据， 1-Hex格式打印数据
#define TEST_DATA_PRINT_LEN	25
void test_log_print(et_mes_type msg_type, et_int8 *from_uid, et_int8 *from_topic_group,	\
					et_uint32 msg_leng, et_uint8 *msg_data)
{
	switch(msg_type)
	{
		case MES_CHAT_TO_CHAT:
		case MES_FROM_LOCAL:
		case MES_CHAT_TO_CHAT_EX:
			os_printf("%013d\t%s\t%d\t", et_system_time_ms(), from_uid, msg_leng);
			if(data_print_format)	// Data Formating
				print_hex("", msg_data, msg_leng>TEST_DATA_PRINT_LEN ? TEST_DATA_PRINT_LEN : msg_leng);
			else
				print_string_len("", msg_data, msg_leng>TEST_DATA_PRINT_LEN ? TEST_DATA_PRINT_LEN : msg_leng);
		break;
		case MES_FROM_SUBTOPIC:
		case MES_FROM_GROUP:
			os_printf("%013d\t%s\t%d\t", et_system_time_ms(), from_topic_group, msg_leng);
			if(data_print_format)	// Data Formating
				print_hex("", msg_data, msg_leng>TEST_DATA_PRINT_LEN ? TEST_DATA_PRINT_LEN : msg_leng);
			else
				print_string_len("", msg_data, msg_leng>TEST_DATA_PRINT_LEN ? TEST_DATA_PRINT_LEN : msg_leng);
		break;
		default:
		break;
	}
}
// ascii to 8421bcd,say 0x32 = 50
static et_uint32 ascii_2_dec(et_int32 ascii)
{
	et_int32 n =1, dec=0;
	
	while(ascii > 0) 
	{
   		dec += (ascii % 10)  * n;
   		ascii /=10;  
   		n *= 16;
	}
	
	return dec;
}

static et_uint8 et_atoh(et_int8 ascii)
{
	et_uint8 hex_4bit = 0;
	if(ET_ISDIGIT(ascii))	///< 数字
	{
		hex_4bit = ascii - '0';
	}
	else if(ET_ISHEXLOWER(ascii))	///< 小写Hex数
	{
		hex_4bit = ascii - 'a' + 10;
	}
	else if(ET_ISHEXUPPER(ascii))	///< 大写Hex数
	{
		hex_4bit = ascii - 'A' + 10;
	}
	
	return hex_4bit;
}

static et_uint32 et_string_2_hex(et_int8 *string, et_uint8 *buf, et_uint32 buf_size)
{
	et_int8 i_temp[2] = {0};
	et_uint32 i_num = (strlen(string)>>1)+(strlen(string)%2 > 0 ? 1:0);
	et_uint i_index = 0;
	
	if(strlen(string) % 2)//存在单数
	{
		i_temp[0] = string[0];
		buf[i_index] = et_atoh(i_temp[0]);
		i_index++;
		string++;
		buf++;
	}
	for(i_index=0; i_index < i_num; i_index++)
	{
		if(i_index > buf_size)
			break;
		i_temp[0] = string[i_index<<1];
		buf[i_index] = et_atoh(i_temp[0])<<4;
		i_temp[0] = string[(i_index<<1) + 1];
		buf[i_index] += et_atoh(i_temp[0]);
	}
	return i_index;
}

static et_int32 et_set_hardware_info(hardware_info_type *info)
{
	et_int8 i_ret_value;
	i_ret_value = spi_flash_erase_sector(APPKEY_UID_FASH_SECTOR);
	if(i_ret_value != SPI_FLASH_RESULT_OK)
	{
		i_ret_value = -1;
	}
	else
	{
		i_ret_value = spi_flash_write(APPKEY_UID_FASH_SECTOR * SPI_FLASH_SEC_SIZE, (et_uint32 *)info, sizeof(hardware_info_type));
		if(i_ret_value == SPI_FLASH_RESULT_OK)
		{
			i_ret_value = 0;
		}
		else
		{
			i_ret_value = -1;
		}
	}
	return i_ret_value;
}

static et_int32 et_get_hardware_info(hardware_info_type *info_buf)
{
	et_int8 i_ret_value;
	i_ret_value = spi_flash_read(APPKEY_UID_FASH_SECTOR * SPI_FLASH_SEC_SIZE, (et_uint32 *)info_buf, sizeof(hardware_info_type));
	if(i_ret_value == SPI_FLASH_RESULT_OK)
		i_ret_value = 0;
	else
		i_ret_value = -1;
	return i_ret_value;
}

static et_parse_hardware_info(et_int8 *data, et_int32 data_len, hardware_info_type *info)
{
	et_int8 *i_temp = NULL;
	et_int32 i_pose = 0;
	i_temp = strstr(&data[i_pose], "uid:");
	if(NULL == i_temp || NULL == strchr(&data[i_pose], ';'))
		return -1;
	i_pose += strlen("uid:");
	memcpy(info->uid, &data[i_pose], strchr(&data[i_pose], ';')-(char *)&data[i_pose]);
	i_pose += strchr(&data[i_pose], ';')-(char *)&data[i_pose]+1;
	i_temp = strstr(&data[i_pose], "appkey:");
	if(NULL == i_temp || NULL == strchr(&data[i_pose], ';'))
		return -1;
	i_pose += strlen("appkey:");
	memcpy(info->appkey, &data[i_pose], strchr(&data[i_pose], ';')-(char *)&data[i_pose]);
	i_pose += strchr(&data[i_pose], ';')-(char *)&data[i_pose]+1;
	i_temp = strstr(&data[i_pose], "secretkey:");
	if(NULL == i_temp || NULL == strchr(&data[i_pose], ';'))
		return -1;
	i_pose += strlen("secretkey:");
	memcpy(info->secretkey, &data[i_pose], strchr(&data[i_pose], ';')-(char *)&data[i_pose]);
	i_pose += strchr(&data[i_pose], ';')-(char *)&data[i_pose]+1;
	i_temp = strstr(&data[i_pose], "server:");
	if(NULL == i_temp || NULL == strchr(&data[i_pose], ';'))
		return -1;
	i_pose += strlen("server:");
	memcpy(info->server, &data[i_pose], strchr(&data[i_pose], ';')-(char *)&data[i_pose]);
//	ET_LOG_USER("Parse server:%s\n", info.server);
	return 0;
	
}

et_int32 et_get_int_from_json(cJSON *object, const char *string)
{
	cJSON *i_json = NULL;
	i_json = cJSON_GetObjectItem(object, string);
	if(i_json)
	{
		if(cJSON_Number == i_json->type)
			return i_json->valueint;
	}
	return ET_FAILURE;
}

et_int8 *et_get_string_from_json(cJSON *object, const char *string)
{
	cJSON *i_json = NULL;
	i_json = cJSON_GetObjectItem(object, string);
	if(i_json)
	{
		if(cJSON_String == i_json->type)
			return i_json->valuestring;
	}
	return NULL;
}

et_int32 et_loop_test_config(et_loop_send_config_t *config, cJSON *json_root, et_int32 sendtype)
{
	et_int32 i_net_cmd_length = 0;
	et_int8 *p_data = et_get_string_from_json(json_root, "message");
	et_int8 *p_key = NULL;
	
	config->hex = et_get_int_from_json(json_root, "hex");
	if(config->hex)// Send data by hex formate
	{
		i_net_cmd_length = et_string_2_hex(p_data, g_net_cmd_buf_send, sizeof(g_net_cmd_buf_send));
	}
	else
	{
		i_net_cmd_length = strlen(p_data);
		strncpy(g_net_cmd_buf_send, p_data, sizeof(g_net_cmd_buf_send));
		i_net_cmd_length = i_net_cmd_length > sizeof(g_net_cmd_buf_send) ?	\
							sizeof(g_net_cmd_buf_send) : i_net_cmd_length;
	}
	config->data = g_net_cmd_buf_send;
	config->data_len = i_net_cmd_length;
	ET_LOG_USER("Loop send data length %d\n", config->data_len);
	config->count = et_get_int_from_json(json_root, "count");
	config->num = 0;
	config->send_num = 0;
	config->recv_num = 0;
	config->error_num = 0;
	config->method = et_get_int_from_json(json_root, "method");
	config->sendtype = sendtype;
	switch(config->sendtype)
	{
		case 0:	// chat to chat
			p_key = "uid";
		break;
		case 1:	// publish
			p_key = "topic";
			config->qos = et_get_int_from_json(json_root, "qos");
		break;
		case 2:	// publish to group
			p_key = "groupid";
		break;
	}
	strncpy(g_user_id, et_get_string_from_json(json_root, p_key), sizeof(g_user_id));
	config->id = g_user_id;
	config->time = et_get_int_from_json(json_root, "time");
	config->start = et_get_int_from_json(json_root, "start");
	
	return 0;
}

et_socket_t et_net_cmd_init(et_int16 port)
{
	et_socket_t fd = ET_SOCKET_NULL;

    fd = et_socket_create(ET_SOCKET_UDP);
    if(ET_SOCKET_NULL == fd)
        ET_LOG_USER("Create local UDP socket failed\n");
    if(ET_SUCCESS != et_socket_bind(fd, port))
    {
        ET_LOG_USER("Bind local port %d failed\n", port);
        et_socket_close(fd);    ///< Close TCP socket
        fd = ET_SOCKET_NULL;
    }
    return fd;
}

et_int32 et_net_cmd_response(et_socket_t socket,et_addr_info_t *remote_info, et_int32 cmd, et_int32 result, cJSON *info)
{
	cJSON *i_json_root = NULL;
	et_int8 *i_json_print = NULL;
	et_int32 i_ret_value;
	
	i_json_root = cJSON_CreateObject();
	if(!i_json_root)
		return -1;
	cJSON_AddNumberToObject(i_json_root, "cmd", cmd);
	cJSON_AddNumberToObject(i_json_root, "result", result);
	if(info)
		cJSON_AddItemToObject(i_json_root, "info", info);
	i_json_print = cJSON_Print(i_json_root);
	i_ret_value = et_socket_send_to(socket, i_json_print, strlen(i_json_print), remote_info, 1000);
	free(i_json_print);
	cJSON_Delete(i_json_root);
	return i_ret_value;
}
et_int32 et_net_cmd_process(et_socket_t socket, et_uint8 *buf, et_uint32 buf_size,	\
							et_addr_info_t *remote_info)
{
	et_int32 i_net_cmd_length = 0;
	et_int32 i_cmd = 0;
	cJSON *i_json_root = NULL;
	cJSON *i_json_info = NULL;
	cJSON *i_json_arry = NULL;
	cJSON *i_json = NULL;
	et_int32 i_ret_value;
	
	i_net_cmd_length = et_socket_recv_from(socket, buf, buf_size, remote_info, 0);
	if(i_net_cmd_length > 0)
	{
		//Parse packet by json
		i_json_root = cJSON_Parse(buf);
		if(!i_json_root)
		{
			i_ret_value = -10001;
			goto quit;
		}
		i_cmd = et_get_int_from_json(i_json_root, "cmd");
		if(i_cmd < 0)
		{
			i_ret_value = -10002;
			goto quit;
		}
		switch(i_cmd)
		{
			//Power Restart
			case 0:
				system_restart();
			break;
			//Set Config
			case 1:
				strncpy(hardware_info.uid, et_get_string_from_json(i_json_root, "uid"),	\
						sizeof(hardware_info.uid));
				strncpy(hardware_info.appkey, et_get_string_from_json(i_json_root, "appkey"),	\
						sizeof(hardware_info.appkey));
				strncpy(hardware_info.secretkey, et_get_string_from_json(i_json_root, "secretkey"),	\
						sizeof(hardware_info.secretkey));
				strncpy(hardware_info.server, et_get_string_from_json(i_json_root, "server"),	\
						sizeof(hardware_info.server));
				i_ret_value = et_set_hardware_info(&hardware_info) ;
			break;
			//Get Config
			case 2:
				i_ret_value = et_get_hardware_info(&hardware_info);
				if(ET_SUCCESS == i_ret_value)
				{
					i_json_info = cJSON_CreateObject();
					if(i_json_info)
					{
						cJSON_AddStringToObject(i_json_info, "uid", hardware_info.uid);
						cJSON_AddStringToObject(i_json_info, "appkey", hardware_info.appkey);
						cJSON_AddStringToObject(i_json_info, "secretkey", hardware_info.secretkey);
						cJSON_AddStringToObject(i_json_info, "server", hardware_info.server);
					}
					else
						i_ret_value = -0x10003;
				}
			break;
			//Connect
			case 3:
				g_cloud_con_para.clr_offline_mes = et_get_int_from_json(i_json_root,"clrofflinemsg");
				g_cloud_con_para.alive_sec = et_get_int_from_json(i_json_root,"alive");
				i_ret_value = et_connect(g_cloud_handle, g_cloud_con_para);
				ET_LOG_USER("Head size: %u\n", system_get_free_heap_size());
				system_print_meminfo();
			break;
			//Disconnect
			case 4:
				i_ret_value = et_disconnect(g_cloud_handle);
			break;
			//Reconnect
			case 5:
				g_cloud_con_para.clr_offline_mes=et_get_int_from_json(i_json_root,"clrofflinemsg");
				i_ret_value = et_reconnect(g_cloud_handle, g_cloud_con_para);
			break;
			//Subscribe
			case 6:
				i_ret_value = et_subscribe(g_cloud_handle, et_get_string_from_json(i_json_root, "topic"),	\
										   et_get_int_from_json(i_json_root, "qos"));
			break;
			//Unsubscribe
			case 7:
				i_ret_value = et_unsubscribe(g_cloud_handle, et_get_string_from_json(i_json_root, "topic"));
			break;
			//Get user state
			case 8:
				i_ret_value = et_get_user_state(g_cloud_handle, et_get_string_from_json(i_json_root, "uid"));
			break;
			//Sub user state
			case 9:
				i_ret_value = et_sub_user_state(g_cloud_handle, et_get_string_from_json(i_json_root, "uid"));
			break;
			//Unsub user state
			case 10:
				i_ret_value = et_unsub_user_state(g_cloud_handle, et_get_string_from_json(i_json_root, "uid"));
			break;
			//Publish to group
			case 11:
			{
				et_int8 *p_data;
				if(et_get_int_from_json(i_json_root, "hex"))// Send data by hex formate
				{
					i_net_cmd_length = et_string_2_hex(et_get_string_from_json(i_json_root, "message"),	\
													   g_net_cmd_buf_send, sizeof(g_net_cmd_buf_send));
					p_data = g_net_cmd_buf_send;
				}
				else
				{
					p_data = et_get_string_from_json(i_json_root, "message");
					i_net_cmd_length = strlen(p_data);
				}
				i_ret_value = et_publish_to_group(g_cloud_handle, p_data, i_net_cmd_length,	\
												  et_get_string_from_json(i_json_root, "groupid"));
			}
			break;
			//Chat to
			case 12:
			{
				et_int8 *p_data;
				if(et_get_int_from_json(i_json_root, "hex"))// Send data by hex formate
				{
					i_net_cmd_length = et_string_2_hex(et_get_string_from_json(i_json_root, "message"),	\
													   g_net_cmd_buf_send, sizeof(g_net_cmd_buf_send));
					p_data = g_net_cmd_buf_send;
				}
				else
				{
					p_data = et_get_string_from_json(i_json_root, "message");
					i_net_cmd_length = strlen(p_data);
				}
				i_ret_value = et_chat_to(g_cloud_handle, p_data, i_net_cmd_length,     	\
										 et_get_string_from_json(i_json_root, "uid"));
			}
			break;
			//Publish
			case 13:
			{
				et_int8 *p_data;
				if(et_get_int_from_json(i_json_root, "hex"))// Send data by hex formate
				{
					i_net_cmd_length = et_string_2_hex(et_get_string_from_json(i_json_root, "message"),	\
													   g_net_cmd_buf_send, sizeof(g_net_cmd_buf_send));
					p_data = g_net_cmd_buf_send;
				}
				else
				{
					p_data = et_get_string_from_json(i_json_root, "message");
					i_net_cmd_length = strlen(p_data);
				}
				i_ret_value = et_publish(g_cloud_handle, p_data, i_net_cmd_length,     	\
										 et_get_string_from_json(i_json_root, "topic"),	\
										 et_get_int_from_json(i_json_root, "qos"));
			}
			break;
			//File to
			case 14:
			{
#if ET_CONFIG_FILE_EN
				et_int8 *p_data = et_get_string_from_json(i_json_root, "data");
				strncpy(g_data_buf, p_data, sizeof(g_data_buf));
				g_remain_size = et_get_int_from_json(i_json_root, "filesize");
				g_data_buf_size = strlen(p_data)>=sizeof(g_data_buf)?sizeof(g_data_buf) : strlen(p_data);
				i_ret_value = et_file_to(g_cloud_handle, et_get_string_from_json(i_json_root, "uid"),	\
										 et_get_string_from_json(i_json_root, "filename"),	\
										 g_remain_size, &g_file_info, file_uplaod_process);
#endif
			}
			break;
			//Start server
			case 15:
#if ET_CONFIG_SERVER_EN
				i_ret_value = et_start_server(g_cloud_handle);
#endif
			break;
			//Stop server
			case 16:
#if ET_CONFIG_SERVER_EN
				i_ret_value = et_stop_server(g_cloud_handle);
#endif
			break;
			//Request offline message
			case 17:
				i_ret_value = et_request_offline_message(g_cloud_handle);
			break;
			//Chat to loop
			case 18:
				i_ret_value = et_loop_test_config(&et_loop_test, i_json_root, 0);
			break;
			//Publish loop
			case 19:
				i_ret_value = et_loop_test_config(&et_loop_test,i_json_root, 1);
			break;
			//Publish to group loop
			case 20:
				i_ret_value = et_loop_test_config(&et_loop_test,i_json_root, 2);
			break;
			//Get loop num
			case 21:
				i_ret_value = 0;
				i_json_info = cJSON_CreateObject();
				if(i_json_info)
				{
					
					cJSON_AddNumberToObject(i_json_info, "count", et_loop_test.count);
					cJSON_AddNumberToObject(i_json_info, "send_num", et_loop_test.send_num);
					cJSON_AddNumberToObject(i_json_info, "recv_num", et_loop_test.recv_num);
					cJSON_AddNumberToObject(i_json_info, "error_num", et_loop_test.error_num);
					cJSON_AddNumberToObject(i_json_info, "data_len", et_loop_test.data_len);
					cJSON_AddNumberToObject(i_json_info, "hex", et_loop_test.hex);
					cJSON_AddNumberToObject(i_json_info, "method", et_loop_test.method);
					cJSON_AddNumberToObject(i_json_info, "time", et_loop_test.time);
				}
				else
					i_ret_value = -0x10003;
			break;
			//Get local user
			case 22:
			{
#if ET_CONFIG_SERVER_EN
				et_int32 i_temp;
				et_char (*i_local_users_buf)[ET_USER_ID_LEN_MAX] = (void *)g_net_cmd_buf_send;

				i_ret_value = et_get_local_users(g_cloud_handle, i_local_users_buf, 5);
				ET_LOG_USER("Get local user %d->%s\n", i_ret_value, i_local_users_buf[0]);
				if(i_ret_value)
				{
					i_json_info = cJSON_CreateObject();
					i_json_arry = cJSON_CreateArray();
					for(i_temp = 0; i_temp < i_ret_value; i_temp++)
					{
						i_json = cJSON_CreateObject();
						cJSON_AddStringToObject(i_json, "uid", i_local_users_buf[i_temp]);
						cJSON_AddItemToArray(i_json_arry, i_json);
					}
					if(i_json_info && i_json_arry)
					{
						cJSON_AddItemToObject(i_json_info, "info", i_json_arry);
					}
					else
					{
						if(i_json_info)
							cJSON_Delete(i_json_info);
						if(i_json_arry)
							cJSON_Delete(i_json_arry);
					}
				}
#endif
			}
			break;
			//Display hex
			case 23:
				data_print_format = et_get_int_from_json(i_json_root,"displayhex");
				i_ret_value = data_print_format;
			break;
			//Set option
			case 24:
			{
				et_option_t ioption;
				ioption.send_method = et_get_int_from_json(i_json_root, "sendMethod");
				i_ret_value = et_set_option(g_cloud_handle, &ioption);
			}
			break;
			//Do not support
			default:
			i_ret_value = -1;
			i_json_info = cJSON_CreateObject();
			if(i_json_info)
			{
				cJSON_AddStringToObject(i_json_info, "info", "Do not support");
			}
			break;
		}
quit:
		et_net_cmd_response(socket, remote_info, i_cmd, i_ret_value, i_json_info);
		if(i_json_root)
			cJSON_Delete(i_json_root);
		if(i_json_info)
			cJSON_Delete(i_json_info);
	}
}

static et_uint32 g_end_time = 0;
void et_send_loop_test(et_loop_send_config_t *config)
{
	et_int32 i_ret_value = ET_FAILURE;
	et_uint32 i_length = 0;

	if(config->start && config->count != config->num)	// Start send
	{
		if(et_system_time_ms() - g_end_time >= config->time)
		{
			config->num++;
			if(config->hex)
			{
				memcpy(g_data_buf, &config->num, 4);
				memcpy(g_data_buf+4, g_net_cmd_buf_send, config->data_len);
				i_length = config->data_len + 4;
			}
			else
			{
				snprintf(g_data_buf, sizeof(g_data_buf), "%d,%s", config->num, g_net_cmd_buf_send);
				i_length = strlen(g_data_buf);
			}
			config->data = g_data_buf;
			switch(config->sendtype)
			{
				case 0: //Chat to chat
					i_ret_value = et_chat_to(g_cloud_handle, config->data, i_length,     \
											 config->id);
				break;
				case 1:	//Publish
					i_ret_value = et_publish(g_cloud_handle, config->data, i_length,     \
											 config->id, config->qos);
				break;
				case 2:	//Publish to group
					i_ret_value = et_publish_to_group(g_cloud_handle, config->data,	\
													  i_length, config->id);
				break;
			}
			if(i_ret_value < 0)
				config->error_num++;
			g_end_time = et_system_time_ms();
		}
	}
}
/**
 * 消息回调
 */
et_dfs_file_info_type i_file_info;
et_int32 g_file_from_flag = 0;
et_int32 g_count = 0;
et_int32 g_send_test_flag = 0;
et_int32 g_send_test_num = 0;
#define MSG_SENT_TEST_NUM	2000
void et_message_process(et_int32 type, et_char *send_userid, et_char *topic_name,	\
						et_int32 topic_len, et_context_mes_type *message)
{
	struct hostent *ip_addr = NULL;
	et_char server_ip[32];
	int rc = -1;
	switch(type)
	{
		case MES_CHAT_TO_CHAT:
			ET_LOG_USER("Receive a Chat to chat message\n");
			et_loop_test.recv_num++;
			break;
		case MES_FILE_TRANSFERS:
		{
			et_int8 *i_file_name;
			ET_LOG_USER("File trans mes from %s:%s->%s\n", send_userid, topic_name, message->payload);
			memset(&i_file_info, 0, sizeof(et_dfs_file_info_type));
#if ET_CONFIG_FILE_EN
			rc = et_file_info(g_cloud_handle,message->payload, &i_file_info);
			if(rc == -1)
				ET_LOG_USER("file info parse failed\n");
			
			ip_addr = gethostbyname(i_file_info.source_ip_addr);
			if(ip_addr == NULL)
			{
				ET_LOG_USER("lb gethostbyname fail\n");
				return;
			}
			ET_LOG_USER("File:%s, size:%u, from:%s:%s\n", i_file_info.file_name, i_file_info.file_size,
						i_file_info.source_ip_addr, i_file_info.port);
			i_file_name = strrchr((const char *)i_file_info.file_name, '.');
			memset(i_file_info.source_ip_addr, 0, sizeof(i_file_info.source_ip_addr));
			strcpy(i_file_info.source_ip_addr, inet_ntoa(*(ip_addr->h_addr)));
			if(i_file_name > i_file_info.file_name && 0 == et_strncmp(i_file_name, ".txt", et_strlen(".txt")))
			{
				et_download_file(g_cloud_handle, &i_file_info, file_download_process);
			}
			else
			{
				ET_LOG_USER("File type is not .txt, don't download\n");
			}
#endif
		}
			break;
		case MES_FROM_SUBTOPIC:
			ET_LOG_USER("Receive a Topic message\n");
			et_loop_test.recv_num++;
			break;
		case MES_FROM_LOCAL:
			ET_LOG_USER("Receive a Local message\n");
			et_loop_test.recv_num++;
			break;
		case MES_NOTICE_ADD_BUDDY:
			ET_LOG_USER("You are be add buddy by %s:%s\n", send_userid, topic_name);
			break;
		case MES_NOTICE_REMOVE_BUDDY:
			ET_LOG_USER("You are be remove buddy by %s:%s\n", send_userid, topic_name);
			break;
		case MES_USER_OFFLINE:
			ET_LOG_USER("%s Offline:%s\n", send_userid, topic_name);
			break;
		case MES_USER_ONLINE:
			ET_LOG_USER("%s Online:%s\n", send_userid, topic_name);
			break;
		case MES_USER_STATUS:
			ET_LOG_USER("%s Status:%s->%s\n", send_userid, topic_name, message->payload);
			break;
		case MES_CHAT_TO_CHAT_EX:
			ET_LOG_USER("Receive a Chat_ex message\n");
			break;
		case MES_FROM_GROUP:
			ET_LOG_USER("Receive a Group message\n");
			et_loop_test.recv_num++;
			break;
	}
	test_log_print(type, send_userid, topic_name, message->payload_len, message->payload);
}
volatile et_uint32 g_account_kick = 0;
et_int32 g_reconnect_num = 0;
void et_event_process(et_event_type event)
{
	et_int32 rc = -1;
	struct hostent *ip_addr = NULL;
	switch(event.event_no)
	{
		case EVENT_CLOUD_CONNECT:
			ET_LOG_USER("You are connect:0x%x\n", event.event_no, event.data);
			break;
		case EVENT_CLOUD_DISCONNECT:
			ET_LOG_USER("You are disconnect:0x%x\n", event.event_no, event.data);
			rc = et_reconnect(g_cloud_handle, g_cloud_con_para);
			if(rc<0)
			{
				ET_LOG_USER("Reconnect failed, sleep a moment\n");
				et_sleep_ms(2000);
			}
			g_reconnect_num++;
			ET_LOG_USER("Manual relogin %d\n", g_reconnect_num);
			break;
		case EVENT_LOGIN_KICK:
			ET_LOG_USER("Your account was login by others:0x%x\n", event.event_no, event.data);
			g_account_kick = 1;
			break;
		case EVENT_CLOUD_SEND_FAILED:
			ET_LOG_USER("Cloud send failed %d\n", event.data);
			et_loop_test.error_num++;
			break;
		case EVENT_CLOUD_SEND_SUCCESS:
			ET_LOG_USER("Cloud send success %d\n", event.data);
			et_loop_test.send_num++;
			break;
		case EVENT_LOCAL_SEND_FAILED:
			ET_LOG_USER("Local send failed %d\n", event.data);
			// et_loop_test.error_num++;
			break;
		case EVENT_LOCAL_SEND_SUCCESS:
			ET_LOG_USER("Local send success %d\n", event.data);
			// et_loop_test.send_num++;
			break;
		case EVENT_LOCAL_CONNECT:
			ET_LOG_USER("Local %s connect\n", (et_char *)event.data);
			break;
		case EVENT_LOCAL_DISCONNECT:
			ET_LOG_USER("Local %s disconnect\n", (et_char *)event.data);
			break;
		case EVENT_SUBSCRIBE_SUCCESS:
			ET_LOG_USER("Subscribe success %d\n", event.data);
			break;
		case EVENT_SUBSCRIBE_FAILED:
			ET_LOG_USER("Subscribe failed %d\n", event.data);
			break;
		case EVENT_UNSUBSCRIBE_SUCCESS:
			ET_LOG_USER("Unsubscribe success %d\n", event.data);
			break;
		case EVENT_UNSUBSCRIBE_FAILED:
			ET_LOG_USER("Unsubscribe failed %d\n", event.data);
			break;
		case EVENT_SUB_USER_STATE_SUCCESS:
			ET_LOG_USER("Sub user state success %d\n", event.data);
			break;
		case EVENT_SUB_USER_STATE_FAILED:
			ET_LOG_USER("Sub user state failed %d\n", event.data);
			break;
		case EVENT_UNSUB_USER_STATE_SUCCESS:
			ET_LOG_USER("Unsub user state success %d\n", event.data);
			break;
		case EVENT_UNSUB_USER_STATE_FAILED:
			ET_LOG_USER("Unsub user state failed %d\n", event.data);
			break;
		case EVENT_LOCAL_DISCOVER:
		{
			et_discover_type *pdiscover = (et_discover_type *)event.data;
#if ET_CONFIG_SERVER_EN
			et_server_discover_resp(g_cloud_handle, &pdiscover->addr);
#endif
			ET_LOG_USER("Local discover from %s:%d, data len %d\n", pdiscover->addr.ip_str, pdiscover->addr.port, pdiscover->data.len);
		}
			break;
	}
}

void et_ilink_task(void *pvParameters)
{
	et_int32 i_ilink_code;
	et_uint32 i_count = 0; 
	while(0 == to_stop_ilink)
	{
		i_count++;
		i_ilink_code = et_ilink_loop(pvParameters);
		if(((i_ilink_code == ET_ERR_IM_ILINK_LOOP_STOPPED || ET_SUCCESS == i_ilink_code) && 0 == i_count%70000)	\
		 || (i_ilink_code != ET_ERR_IM_ILINK_LOOP_STOPPED))
			ET_LOG_USER("iLink task loop %d\n", i_ilink_code);
		taskYIELD();
	}
	vTaskDelete(NULL);
}

void et_local_task(void *pvParameters)
{
	et_int32 i_local_code;
	et_uint32 i_count = 0; 
	while(0 == to_stop_server)
	{
		i_count++;
		i_local_code = et_server_loop(pvParameters);
		if(((i_local_code == ET_ERR_IM_LOCAL_LOOP_STOPPED || ET_SUCCESS == i_local_code) && 0 == i_count%70000)	\
		 || (i_local_code < 0 && i_local_code != ET_ERR_IM_LOCAL_LOOP_STOPPED))
			ET_LOG_USER("Local task loop %d\n", i_local_code);
		taskYIELD();
	}
	vTaskDelete(NULL);
}

void et_net_send_task(void *pvParameters)
{
	et_int32 i_net_send_code;
	et_uint32 i_count = 0;
	while(0 == to_stop_netsend)
	{
		i_count++;
		i_net_send_code = et_net_send_loop(pvParameters);
		if(0 == i_count%70000)
			ET_LOG_USER("Net send loop %d\n", i_net_send_code);
		taskYIELD();
	}
	vTaskDelete(NULL);
}

void et_wait_wifi_connect(et_int32 *p_wifi_reconnect_start_flag)
{
	struct ip_info ipconfig;
	
	while(1 != *p_wifi_reconnect_start_flag)
	{
		wifi_get_ip_info(STATION_IF, &ipconfig);
		if (wifi_station_get_connect_status() == STATION_GOT_IP && ipconfig.ip.addr != 0) 
		{
			ET_LOG_USER("Wifi connect success, got ip !!! \r\n");
			*p_wifi_reconnect_start_flag = 1;
		}
		else 
		{ 
			if ((wifi_station_get_connect_status() == STATION_WRONG_PASSWORD ||
			      wifi_station_get_connect_status() == STATION_NO_AP_FOUND ||
			      wifi_station_get_connect_status() == STATION_CONNECT_FAIL)) 
			{
				ET_LOG_USER("connect fail wrong password or ssid wrong!!! \r\n");
		    }
			else
			{
				ET_LOG_USER("Wait wifi connect...\r\n");
			}
			vTaskDelay(1000/portTICK_RATE_MS);
		}
	}
}

void  et_user_main(void *pvParameters)
{
	et_int32 rc = -1;
	// et_int32 num = 0;
	et_uint32 i_send_count = 0;
	// struct ip_info ipconfig;
	et_char (*i_local_user_list)[ET_USER_ID_LEN_MAX] = NULL;//[5][ET_USER_ID_LEN_MAX] = {0};
	et_int32 i_state = 0;
	et_net_addr_type i_lb_addr;
	et_int32 i_len = 0;
	et_int32 i_end_time = 0;
	// et_int32 *p_wifi_reconnect_start_flag = (et_int32 *)pvParameters;
	et_socket_t i_udp_cmd_socket = ET_SOCKET_NULL;
	struct rst_info *rtc_info = system_get_rst_info();
	os_printf("uaer main task start... %u\n", system_get_free_heap_size());
	system_print_meminfo();
	os_printf("USDK reset reason: %x\n", rtc_info->reason);
	if (rtc_info->reason == REASON_WDT_RST || rtc_info->reason == REASON_EXCEPTION_RST ||	\
		rtc_info->reason == REASON_SOFT_WDT_RST)
	{
		if (rtc_info->reason == REASON_EXCEPTION_RST)
		{
			os_printf("Fatal exception (%d):\n", rtc_info->exccause);
		}
		os_printf("epc1=0x%08x, epc2=0x%08x, epc3=0x%08x,excvaddr=0x%08x, depc=0x%08x\n",	\
					rtc_info->epc1, rtc_info->epc2, rtc_info->epc3, rtc_info->excvaddr, rtc_info->depc);
	}
	et_wait_wifi_connect((et_int32 *)pvParameters);
#if ET_CONFIG_SERVER_EN
	i_local_user_list = malloc(5*ET_USER_ID_LEN_MAX);
	if(NULL == i_local_user_list)
	{
		ET_LOG_USER("Create local user list buf failed\n");
	}
	else
		memset(i_local_user_list, 0, 5*ET_USER_ID_LEN_MAX);
#endif
	ET_LOG_USER("ET U-SDK ver%s\n",et_get_sdk_version());
	to_stop_app = 0;

	i_state = et_get_hardware_info(&hardware_info);
	OLED_clear();
	OLED_show_str(0, 2, hardware_info.uid, 2);
	OLED_show_str(0, 4, hardware_info.server, 2);
	OLED_show_str(0, 6, hardware_info.appkey, 2);
	ET_LOG_USER("UID=%s\nAppkey=%s\nSecretKey=%s\nServer=%s\n", hardware_info.uid, hardware_info.appkey, hardware_info.secretkey, hardware_info.server);
	// g_cloud_con_para.server_name = hardware_info.server;
	i_lb_addr.name_ip = hardware_info.server;
	i_lb_addr.port = PORT;
	g_cloud_handle = et_create_context(hardware_info.uid, hardware_info.appkey, hardware_info.secretkey, i_lb_addr);
	if(NULL == g_cloud_handle)
		ET_LOG_USER("Init et account failed\n");
	ET_LOG_USER("Get hardware info state %d\n", i_state);
	et_set_callback(g_cloud_handle,et_message_process, et_event_process);
	i_udp_cmd_socket = et_net_cmd_init(8266);
	if(i_udp_cmd_socket >= 0)
	{
		ET_LOG_USER("Net cmd init success %d\n", i_udp_cmd_socket);
	}
	else
		ET_LOG_USER("Net cmd init failed %d\n", i_udp_cmd_socket);
	if(ET_SUCCESS == i_state)
	{	
		to_stop_netsend = 0;
		if(pdPASS == xTaskCreate(et_net_send_task, "Send taks", 384, g_cloud_handle, 7, NULL))
		{
			ET_LOG_USER("============================\nCreate Net send task success\n============================\n");
		}
		else
		{
			to_stop_netsend = 1;
			ET_LOG_USER("****************************\nCreate Net send task failed\n****************************\n");
		}
#if ET_CONFIG_SERVER_EN
		to_stop_server = 0;
		if(pdPASS == xTaskCreate(et_local_task, "Local task", 384, g_cloud_handle, 7, NULL))
		{
			ET_LOG_USER("============================\nCreate Local task success\n============================\n");
		}
		else
		{
			to_stop_server = 1;
			ET_LOG_USER("****************************\nCreate Local task failed\n****************************\n");
		}
#endif
#if ET_CONFIG_CLOUD_EN	
		to_stop_ilink = 0;
		if(pdPASS == xTaskCreate(et_ilink_task, "iLink task", 512, g_cloud_handle, 7, NULL))
		{
			ET_LOG_USER("============================\nCreate iLink task success\n============================\n");
		}
		else
		{
			to_stop_ilink = 1;
			ET_LOG_USER("****************************\nCreate iLink task failed\n****************************\n");
		}
#endif
	}
	ET_LOG_USER("Head size: %u\n", system_get_free_heap_size());
	system_print_meminfo();
	et_sleep_ms(1000);
	while(0 == to_stop_app)
	{
		et_net_cmd_process(i_udp_cmd_socket, g_net_cmd_buf_recv, sizeof(g_net_cmd_buf_recv), &remote_info);
		et_send_loop_test(&et_loop_test);
#if ET_CONFIG_FILE_EN
		if(g_file_from_flag)
		{
			g_file_from_flag = 0;
			rc = et_file_to(g_cloud_handle, g_user_id, "usdk.txt", et_strlen("usdk esp8266 file test\r\n") * 1000,	\
							&i_file_info, file_uplaod_process);
			ET_LOG_USER("File to %s:%d\n", g_user_id, rc);
		}
#endif
		taskYIELD();
	}
	ET_LOG_USER("User main task stopped\n");
	vTaskDelete(NULL);
	return ;
}


