/**@mainpage
 * Copyright (c) 2016 ET iLink.
 * All rights reserved.
 *
 * iLink U-SDK将帮助用户的设备联网，并与Android、IOS、Web、Windows、Linux端
 * 进行IM通信、文件传输。
 * \image html usdk_layout.jpg "USDK 分层结构"
 *
 * - \b 使用流程如下：
 * 	-# 创建上下文： #et_create_context()
 * 	-# 连接iLnk： #et_connect(),连接成功之后将产生 #EVENT_CLOUD_CONNECT 事件
 * 	-# 发送消息： #et_chat_to()
 *
 * - \b 重连流程：
 *  -# 用户调用 #et_disconnect()断连后重连，需要调用 #et_connect()进行重连；
 *  -# SDK在首次连接成功之后，若出现断连将产生 #EVENT_CLOUD_DISCONNECT 事件，用户可调用 #et_reconnect()重连;
 *  -# 用户被踢下线将产生 #EVENT_LOGIN_KICK 事件，用户可针对处理;
 * \image html DevelopFlowchart.jpg "USDK 开发参考流程"
 */

/**
 * @file et_client.h
 * @brief 本文件提供了SDK的所有API接口
 * @date 3/11/2016
 * @author wangjia
 */
#ifndef __ET_CLIENT_H__
#define __ET_CLIENT_H__

#include "et_types.h"
#include "et_config.h"
#include "et_std.h"
#include "et_base.h"
#if ET_CONFIG_RTOS_EN
#include "et_rtos.h"
#endif

/**
 * @def ET_LOG_USER
 * @brief 用户日志打印,由配置项 #ET_LOG_USER_EN 设定
 */
#if ET_LOG_USER_EN
#define ET_LOG_USER(format, ...)    {et_printf("User log: "format, ##__VA_ARGS__);}
#else
#define ET_LOG_GEN(format, ...)     {}
#endif

/**
 * @struct et_cloud_connect_type
 * @brief 外网连接配置结构体
 */
typedef struct{
	et_bool clr_offline_mes;    ///< ET_TRUE-清除离线消息，ET_FALSE-不清除离线消息
    et_uint16 alive_sec;        ///< 外网心跳时间
}et_cloud_connect_type;

/**
 * @brief 获取SDK版本信息
 * @return SDK版本字符串
 * @code
 * //获取SDK版本号示例代码
 * ET_LOG_USER("ET U-SDK ver%s\n",et_get_sdk_version());
 * @endcode
 */
et_char *et_get_sdk_version(void);

/**
 * @brief 创建上下文
 * @attention 该接口不能在回调函数中使用
 * @note 与 #et_destroy_context()配对使用
 * @param userid 指向UID buffer
 * @param app_key 指向AppKey buffer
 * @param secret_key 指向SecretKey buffer
 * @param lb_info 负载均衡地址信息
 *
 * @return 上下文句柄\n
 *         0 表示失败
 * @sa et_destroy_context()
 * @code
 * //iLink创建上下文/连接/销毁示例代码
    et_char *uid = "1234567890123456789012345678901234";
    et_char *app_key = “01234567-1234-123456”;
    et_char *secret_key = “0123456890123456789012345678901”;
    et_cloud_connect_type connect_para ={ET_TRUE, 30};//清除离线消息，心跳30s
    et_net_addr_type i_lb_addr={“lb.kaifakuai.com”, 8085};
    //用户消息回调
    void et_message_process(et_int32 type, et_char *send_userid, et_char *topic_name, et_int32 topic_len, et_context_mes_type *message)
    {
        //用户处理消息
        …
    }
    //用户事件回调
    void et_event_process(et_event_type event)
    {
        //用户处理事件
        …
    }
    //外网处理任务
    void et_ilink_task(void *pvParameters)
    {
        et_uint32 i_count = 0;
        while(1)
        {
            i_ilink_code = et_ilink_loop(pvParameters);
            taskYIELD();
        }
    }
    //内网处理任务
    void et_local_task(void *pvParameters)
    {
        et_int32 i_local_code;
        while(1)
        {
            i_local_code = et_server_loop(pvParameters);
            taskYIELD();
        }
    }
    //消息发送任务
    void et_net_send_task(void *pvParameters)
    {
        et_int32 i_net_send_code;
        while(1)
        {
            i_net_send_code = et_net_send_loop(pvParameters);
            taskYIELD();
        }
    }
    //用户任务
    void user_main_task(void *pvParameters)
    {
        et_int32 rc;
        //用户初始化
        …
        //创建ilink实例
        ilink_handle = et_create_context((const char *)uid, (const char *)app_key, (const char *)secret_key, i_lb_addr);
        if(0 == ilink_handle)
        {
        ET_LOG_USER (“Create ilink context failed\n”);
        return;
        }
        //设置回调函数
        rc = et_set_callback(ilink_handle,et_message_process, et_event_process);
        //创建处理任务
        if(pdPASS == xTaskCreate(et_net_send_task, "Send taks", 512, g_cloud_handle, 7, NULL))
        {
            ET_LOG_USER("Create Net send task success\n");
        }
        else
        {
            ET_LOG_USER("Create Net send task failed\n");
        }
    #if ET_CONFIG_SERVER_EN
        if(pdPASS == xTaskCreate(et_local_task, "Local task", 512, g_cloud_handle, 7, NULL))
        {
            ET_LOG_USER("Create Local task success\n");
        }
        else
        {
            ET_LOG_USER("Create Local task failed\n");
        }
    #endif
    #if ET_CONFIG_CLOUD_EN
        if(pdPASS == xTaskCreate(et_ilink_task, "iLink task", 512, g_cloud_handle, 7, NULL))
        {
            ET_LOG_USER("Create iLink task success\n");
        }
        else
        {
            ET_LOG_USER("Create iLink task failed\n");
        }
    #endif
        //启动本地服务
        rc = et_start_server (ilink_handle);
        if(0 != rc)
            ET_LOG_USER (“Start local service failed\n”);
        //ilink登陆
        rc = et_connect (ilink_handle, connect_para);
        if(0 != rc)
            ET_LOG_USER (“Connect ilink failed\n”);
        //用户处理
        …
        if(user_state)//若用户在某种情况下需要停止SDK服务
        {
            //退出ilink
            rc = et_disconnect (ilink_handle);
            if(0 != rc)
                ET_LOG_USER (“Disconnect ilink failed\n”);
            //停止本地服务
            rc = et_stop_server (ilink_handle);
            if(0 != rc)
                ET_LOG_USER (“Stop local service failed\n”);
            //销毁ilink实例
            rc = et_destroy_context(ilink_handle);
            if(0 != rc)
                ET_LOG_USER (“Destroy context failed\n”);
        }
    }
 * @endcode
 */
et_cloud_handle et_create_context(const et_char *userid, const et_char *app_key,    \
                                  const et_char *secret_key, et_net_addr_type lb_info);

/**
 * @brief 销毁上下文
 * @attention 该接口不能在回调函数中使用
 * @note 与 #et_create_context()配套使用
 * @param cloud_handle 由 #et_create_context()返回
 *
 * @return 0 表示成功\n
 *         <0 表示失败，详见 #et_code
 * @sa et_create_context()
 */
et_int32 et_destroy_context(et_cloud_handle *cloud_handle);

/**
  * @brief 设置回调
  * @note 在非RTOS系统中，回调将占用SDK处理事件；
  *       在RTOS系统中，回调将消耗任务堆栈，请勿在回调中进行过于复杂的操作
  * @param cloud_handle 由 #et_create_context()返回
  * @param mes_call 消息回调,详见 #et_msg_process_Type
  * @param event_call 事件回调,详见 #et_event_process_type
  *
  * @return 0 表示成功\n
  *         <0 表示失败，详见 #et_code
  */
et_int32 et_set_callback(et_cloud_handle cloud_handle, et_msg_process_Type mes_call, et_event_process_type event_call);

/**
 @brief 设置SDK策略
 @note 非必须接口，用户若不调用将采用默认值，详见 #et_option_t
 @param cloud_handle 由 #et_create_context()返回
 @param option 策略设置参数,详见 #et_option_t;

 @return 0 表示成功\n
		 <0 表示失败，详见 #et_code
 */
et_int32 et_set_option(et_cloud_handle cloud_handle, et_option_t *option);

#if ET_CONFIG_SERVER_EN
/**
 * @brief 内网轮询
 * @note 用户需要在用户程序中循环调用该接口
 * @param cloud_handle 由 #et_create_context()返回
 *
 * @return 0 表示成功\n
 *         <0 表示失败，详见 #et_code
 * @sa et_ilink_loop() et_net_send_loop()
 */
et_int32 et_server_loop(et_cloud_handle cloud_handle);

/**
 * @brief 启动内网服务
 * @attention 请勿在回调函数中调用该接口
 * @param cloud_handle 由 #et_create_context()返回
 *
 * @return 0 表示成功\n
 *         <0 表示失败，详见 #et_code
 * @sa et_stop_server()
 */
et_int32 et_start_server(et_cloud_handle cloud_handle);

/**
 * @brief 停止内网服务
 * @attention 请勿在回调函数中调用该接口
 * @param cloud_handle 由 #et_create_context()返回
 *
 * @return 0 表示成功\n
 *         <0 表示失败，详见 #et_code
 * @sa et_start_server()
 */
et_int32 et_stop_server(et_cloud_handle cloud_handle);

/**
 * @brief 获取内网连接用户
 * @param cloud_handle 由 #et_create_context()返回
 * @param local_users_buf 出参，已连接内网用户的UID存储buffer
 * @param users_num local_users_buf允许存储的UID数量，即该二维数组的第一维大小
 *
 * @return 获取到的UID数\n
 *         <0 表示失败，详见 #et_code
 * @code
 * //获取内网连接用户示例代码
   et_int32 i_ret_value;
   et_uint8 g_net_cmd_buf_send[1500] = {0};
   et_char (*i_local_users_buf)[ET_USER_ID_LEN_MAX] = (void *)g_net_cmd_buf_send;
   i_ret_value = et_get_local_users(g_cloud_handle, i_local_users_buf, 5);
   ET_LOG_USER("%d user connect local server\n", i_ret_value);
 * @endcode
 */
et_int32 et_get_local_users(et_cloud_handle cloud_handle, et_char local_users_buf[][ET_USER_ID_LEN_MAX], et_uint32 users_num);

/**
 @brief 内网Discover应答
 @note 当内网接收到一个Discover信息时，将进入 #EVENT_LOCAL_DISCOVER 事件回调,用户通过判断Discover数据决定是否应答，
 需要应答时调用本接口
 @param cloud_handle 由 #et_create_context()返回
 @param dest Discover应答目标地址,由SDK返回至 #EVENT_LOCAL_DISCOVER 的Discover数据结构中

 @return 0 应答成功\n
		 <0 应答失败，详见 #et_code
 */
et_int32 et_server_discover_resp(et_cloud_handle cloud_handle, et_addr_info_t *dest);
#endif

#if ET_CONFIG_CLOUD_EN
/**
 * @brief 外网轮询
 * @note 用户需要在用户程序中循环调用该接口
 * @param cloud_handle 由 #et_create_context()返回
 *
 * @return 0 表示成功\n
 *         <0 表示失败，详见 #et_code
 * @sa et_server_loop() et_net_send_loop()
 */
et_int32  et_ilink_loop(et_cloud_handle cloud_handle);

/**
 * @brief iLink连接
 * @note 与 #et_disconnect()配套使用，若断连重登请调用 #et_reconnect()
 * @attention 请勿在回调函数中调用该接口
 * @param cloud_handle 由 #et_create_context()返回
 * @param connect_para 连接参数，详见 #et_cloud_connect_type
 *
 * @return 0 表示成功\n
 *         <0 表示失败，详见 #et_code
 * @sa et_reconnect() et_disconnect()
 */
et_int32 et_connect(et_cloud_handle cloud_handle, et_cloud_connect_type connect_para);

/**
 * @brief 重连iLink
 * @note 若服务器断连（ #EVENT_CLOUD_DISCONNECT ）或被踢下线（ #EVENT_LOGIN_KICK ）则调用该函数重登
 * @param cloud_handle 由 #et_create_context()返回
 * @param connect_para 连接参数，详见 #et_cloud_connect_type
 *
 * @return 0 表示成功\n
 *        <0 表示失败，详见 #et_code
 * @sa et_connect() et_disconnect()
 * @code
 * //断连重连示例代码
   void et_event_process(et_event_type event)
   {
       et_int32 rc = -1;
       switch(event.event_no)
       {
       case EVENT_CLOUD_CONNECT:
           ET_LOG_USER("You are connect:0x%x\n", event.event_no, event.data);
           break;
       case EVENT_CLOUD_DISCONNECT:
           ET_LOG_USER("You are disconnect:0x%x\n", event.event_no, event.data);
           rc = et_reconnect(g_cloud_handle, g_cloud_con_para);
           if(rc<0)//Reconnect failed
           {
               et_sleep_ms(2000);
           }
           break;
       case EVENT_LOGIN_KICK:
           ET_LOG_USER("Your account was login by others:0x%x\n", event.event_no, event.data);
           break;
       }
   }
 * @endcode
 */
et_int32 et_reconnect(et_cloud_handle cloud_handle, et_cloud_connect_type connect_para);

/**
 * @brief 获取用户状态
 * @note 获取成功后，将在消息回调中收到 #MES_USER_STATUS 类型消息，其中‘1’表示在线，‘0’表示离线
 * @param cloud_handle 由 #et_create_context()返回
 * @param userid 期望获取的用户UID
 *
 * @return 0 表示成功\n
 *         <0 表示失败，详见 #et_code
 * @sa et_sub_user_state() et_unsub_user_state()
 * @code
 * //获取用户状态示例代码
    void et_message_process(et_int32 type, et_char *send_userid, et_char *topic_name,	\
                            et_int32 topic_len, et_context_mes_type *message)
    {
        switch(type)
        {
            case MES_USER_STATUS:
                ET_LOG_USER("%s Status:%s->%s\n", send_userid, topic_name, message->payload);
                break;
        }
    }
    //用户任务
    void user_main_task(void *pvParameters)
    {
        //创建上下文
        ...
        //创建SDK处理任务
        ...
        //连接iLink
        ...
        //获取用户001的状态
        i_ret_value = et_get_user_state(g_cloud_handle, "001");
    }
 * @endcode
 */
et_int32 et_get_user_state(et_cloud_handle cloud_handle, et_char *userid);

/**
 * @brief 状态订阅
 * @note 此处发送成功仅表示进入消息发送队列，订阅用户状态成功/失败将触发事件通知：\n
 * #EVENT_SUB_USER_STATE_SUCCESS / #EVENT_SUB_USER_STATE_FAILED ,详见 #et_event_type
 * @note 订阅成功后，userid用户上下线时将接收到 #MES_USER_ONLINE 、 #MES_USER_OFFLINE 类型消息
 * @param cloud_handle 由 #et_create_context()返回
 * @param userid 期望订阅的用户UID
 *
 * @return 返回消息ID\n
 *         <0 表示失败，详见 #et_code
 * @sa et_unsub_user_state() et_get_user_state()
 * @code
 * //订阅用户状态示例
    //消息回调
    void et_message_process(et_int32 type, et_char *send_userid, et_char *topic_name,	\
                            et_int32 topic_len, et_context_mes_type *message)
    {
        switch(type)
        {
        case MES_USER_OFFLINE:
            ET_LOG_USER("%s Offline:%s\n", send_userid, topic_name);
            break;
        case MES_USER_ONLINE:
            ET_LOG_USER("%s Online:%s\n", send_userid, topic_name);
            break;
        }
    }
    //事件回调
    void et_event_process(et_event_type event)
    {
        switch(event.event_no)
        {
            case EVENT_SUB_USER_STATE_SUCCESS:
                ET_LOG_USER("Sub user state success %d\n", event.data);
                break;
            case EVENT_SUB_USER_STATE_FAILED:
                ET_LOG_USER("Sub user state failed %d\n", event.data);
                break;
        }
    }
    //用户任务
    void user_main_task(void *pvParameters)
    {
        //创建上下文
        ...
        //创建SDK处理任务
        ...
        //连接iLink
        ...
        //订阅用户001的状态
        i_ret_value = et_sub_user_state(g_cloud_handle, “001”);
    }
 * @endcode
 */
et_int32 et_sub_user_state(et_cloud_handle cloud_handle, et_char *userid);

/**
 * @brief 取消状态订阅
 * @note 此处发送成功仅表示进入消息发送队列，取消订阅用户状态成功/失败将触发事件通知：\n
 * #EVENT_UNSUB_USER_STATE_SUCCESS / #EVENT_UNSUB_USER_STATE_FAILED ,详见 #et_event_type
 * @param cloud_handle 由 #et_create_context()返回
 * @param userid 期望取消订阅的用户UID
 *
 * @return 返回消息ID\n
 *         <0 表示失败，详见 #et_code
 * @sa et_sub_user_state() et_get_user_state()
 * @code
 * //取消订阅用户状态示例
    //事件回调
    void et_event_process(et_event_type event)
    {
        switch(event.event_no)
        {
            case EVENT_UNSUB_USER_STATE_SUCCESS:
                ET_LOG_USER("Unsub user state success %d\n", event.data);
                break;
            case EVENT_UNSUB_USER_STATE_FAILED:
                ET_LOG_USER("Unsub user state failed %d\n", event.data);
                break;
        }
    }
    //用户任务
    void user_main_task(void *pvParameters)
    {
        //创建上下文
        ...
        //创建SDK处理任务
        ...
        //连接iLink
        ...
        //取消订阅用户001的状态
        i_ret_value = et_unsub_user_state(g_cloud_handle, “001”);
    }
 * @endcode
 */
et_int32 et_unsub_user_state(et_cloud_handle cloud_handle, et_char *userid);

/**
 * @brief 获取离线消息
 * @note 需要在连接时将 #et_cloud_connect_type->clr_offline_mes设置为ET_FALSE,调用本接口才能获取到离线消息
 * @param cloud_handle 由 #et_create_context()返回
 *
 * @return 0 表示成功\n
 *         <0 表示失败，详见 #et_code
 * @sa et_connect() et_reconnect()
 */
et_int32 et_request_offline_message(et_cloud_handle cloud_handle);

/**
 * @brief 发送群消息
 * @note 此处发送成功仅表示进入消息发送队列，消息发送失败/成功将触发事件通知：\n
 * #EVENT_CLOUD_SEND_FAILED / #EVENT_CLOUD_SEND_SUCCESS ,详见 #et_event_type
 * @param cloud_handle 由 #et_create_context()返回
 * @param group_id 群ID,不能包含 # + 等特殊字符
 * @param data 消息数据buffer
 * @param data_len 消息数据长度
 *
 * @return 返回消息ID\n
 *         <0 表示失败，详见 #et_code
 * @sa et_chat_to() et_publish()
 */
et_int32 et_publish_to_group(et_cloud_handle cloud_handle, et_uint8 *data, et_int32 data_len, const et_char* group_id);
/**
 * @brief 订阅主题
 * @note 此处发送成功仅表示进入消息发送队列，订阅成功/失败将触发事件通知：\n
 * #EVENT_SUBSCRIBE_SUCCESS / #EVENT_SUBSCRIBE_FAILED ,详见 #et_event_type
 * @param cloud_handle 由 #et_create_context()返回
 * @param topic 消息主题,主题中不能包含 # + 等特殊字符
 * @param qos 消息级别，详见 #et_mes_qos_type
 *
 * @return 返回消息ID\n
 *         <0 表示失败，详见 #et_code
 * @sa et_unsubscribe()
 */
et_int32 et_subscribe(et_cloud_handle cloud_handle, const et_char* topic, et_mes_qos_type qos);

/**
 * @brief 取消订阅主题
 * @note 此处发送成功仅表示进入消息发送队列，取消订阅成功/失败将触发事件通知：\n
 * #EVENT_UNSUBSCRIBE_SUCCESS / #EVENT_UNSUBSCRIBE_FAILED,详见 #et_event_type
 * @param cloud_handle 由 #et_create_context()返回
 * @param topic 消息主题,主题中不能包含 # + 等特殊字符
 *
 * @return 返回消息ID\n
 *         <0 表示失败，详见 #et_code
 * @sa et_subscribe()
 */
et_int32 et_unsubscribe(et_cloud_handle cloud_handle, const et_char* topic);
/**
 * @brief 断开iLink连接
 * @note 与 #et_connect()配套使用,调用本接口断连后需再次连接请调用 #et_connect()
 * @attention 请勿在回调函数中调用该接口
 * @param cloud_handle 由 #et_create_context()返回
 *
 * @return 0 表示成功\n
 *         <0 表示失败，详见 #et_code
 * @sa et_connect() et_reconnect()
 */
et_int32 et_disconnect(et_cloud_handle cloud_handle);
#endif
#if ET_CONFIG_CLOUD_EN || ET_CONFIG_SERVER_EN
/**
 * @brief 发送点对点消息
 * @note 此处返回成功只表示成功进入消息队列，根据参数send_method，消息发送成功/失败将触发事件通知：\n
 * #EVENT_CLOUD_SEND_FAILED / #EVENT_CLOUD_SEND_SUCCESS / #EVENT_LOCAL_SEND_FAILED / #EVENT_LOCAL_SEND_SUCCESS
 * @param cloud_handle 由 #et_create_context()返回
 * @param data 待发送的数据
 * @param len 待发送的数据长度，单位byte
 * @param userid 目标UID
 *
 * @return 返回消息ID\n
 *         <0 表示失败，详见 #et_code
 * @sa et_publish() et_publish_to_group()
 */
et_int32 et_chat_to(et_cloud_handle cloud_handle, et_uchar *data, et_uint32 len,     \
					const et_char *userid);

/**
 * @brief 发布主题消息
 * @note 此处返回成功只表示成功进入消息队列，根据参数send_method，消息发布成功/失败将触发事件通知：\n
 * #EVENT_CLOUD_SEND_FAILED / #EVENT_CLOUD_SEND_SUCCESS / #EVENT_LOCAL_SEND_FAILED / #EVENT_LOCAL_SEND_SUCCESS
 * @param cloud_handle 由 #et_create_context()返回
 * @param data 待发送的数据
 * @param len 待发送的数据长度，单位byte
 * @param topic 消息主题,主题中不能包含 # + 等特殊字符
 * @param qos 消息级别
 *
 * @return #ET_QOS0 发送成功时返回0, #ET_QOS1 、 #ET_QOS2 返回消息ID\n
 *         <0 表示失败，详见 #et_code
 * @sa et_chat_to() et_publish_to_group()
*/
et_int32 et_publish(et_cloud_handle cloud_handle, et_uchar *data, et_uint32 len,     \
					const et_char *topic, et_mes_qos_type qos);

/**
 * @brief 数据发送轮询
 * @note 用户需要在用户程序中循环调用该接口
 * @param cloud_handle 由 #et_create_context()返回
 *
 * @return 0 表示成功\n
 *         <0 表示失败，详见 #et_code
 * @sa et_ilink_loop() et_server_loop()
 */
et_int32 et_net_send_loop(et_cloud_handle cloud_handle);
#endif
#if ET_CONFIG_FILE_EN
/**
 * @brief 解析文件信息
 * @param cloud_handle 由 #et_create_context()返回
 * @param file_str 待解析的文件消息
 * @param file_info 出参，解析出的文件信息存储结构，详见 #et_dfs_file_info_type
 *
 * @return 0 表示成功\n
 *         <0 表示失败，详见 #et_code
 */
et_int32 et_file_info(et_cloud_handle cloud_handle, et_char *file_str, et_dfs_file_info_type *file_info);

/**
 * @brief 下载文件
 * @note 仅限iLink外网
 * @param cloud_handle 由 #et_create_context()返回
 * @param file_info 待下载的文件信息
 * @param down_cb 文件下载回调，将在文件下载完成后调用，不需要可传NULL
 *
 * @return 0 表示成功\n
 *         <0 表示失败，详见 #et_code
 */
et_int32 et_download_file(et_cloud_handle cloud_handle, et_dfs_file_info_type *file_info,   \
                      et_file_down_process_type down_cb);
/**
 * @brief 上传文件
 * @note 将文件上传到文件服务器,但无接收方，仅用于存储
 * @attention 仅限iLink外网
 * @param cloud_handle 由 #et_create_context()返回
 * @param file_name 待上传的文件名
 * @param file_size 待上传的文件大小，单位byte,必须大于0
 * @param file_info 出参，发送的文件信息
 * @param upload_cb 发送文件回调,用户在回调中使用TCP发送文件数据,详见 #et_file_uplaod_process_type
 *
 * @return 0 表示成功\n
 *         <0 表示失败，详见 #et_code
 */
et_int32 et_upload_file(et_cloud_handle cloud_handle, et_int8 *file_name, et_uint32 file_size,   \
                        et_dfs_file_info_type *file_info, et_file_uplaod_process_type upload_cb);

/**
 * @brief 主动发送文件给iLink用户
 * @note 将文件上传到文件服务器，指定接收UID，指定的接收方将收到一条文件消息
 * @attention 仅限iLink外网
 * @param cloud_handle 由 #et_create_context()返回
 * @param userid 接收方UID
 * @param file_name 待发送的文件名
 * @param file_size 待发送的文件大小，单位byte,必须大于0
 * @param file_info 出参，发送的文件信息
 * @param upload_cb 发送文件回调,用户在回调中使用TCP发送文件数据,详见 #et_file_uplaod_process_type
 *
 * @return 0 表示成功\n
 *         <0 表示失败，详见 #et_code
 */
et_int32 et_file_to(et_cloud_handle cloud_handle, et_int8 *userid, et_int8 *file_name, et_uint32 file_size, \
                    et_dfs_file_info_type *file_info,et_file_uplaod_process_type upload_cb);
#endif

#if ET_CONFIG_HTTP_EN

/**
 * @brief 添加好友
 * @param cloud_handle 由 #et_create_context()返回
 * @param friendid 好友ID
 *
 * @return 0表示成功\n
 *         <0表示失败，详见 #et_code
 */
et_int32 et_add_buddy(et_cloud_handle cloud_handle, const et_char * friendid);

/**
 * @brief 添加好友并通知
 * @note 操作将发送通知，且要求用户必须在线才能操作成功
 * @param cloud_handle 由 #et_create_context()返回
 * @param friendid 好友ID
 * @param notify 是否通知，ET_TRUE-通知，ET_FALSE-不通知
 *
 * @return 0表示成功\n
 *         <0表示失败，详见 #et_code
 */
et_int32 et_add_buddy_notify(et_cloud_handle cloud_handle, const et_char * friendid, et_bool notify);

/**
 * @brief 删除好友
 * @param cloud_handle 由 #et_create_context()返回
 * @param friendid 好友ID
 *
 * @return 0表示成功\n
 *         <0表示失败，详见 #et_code
 */
et_int32 et_remove_buddy(et_cloud_handle cloud_handle, const et_char * friendid);

/**
 * @brief 删除好友并通知
 * @note 操作将发送通知，且要求用户必须在线才能操作成功
 * @param cloud_handle 由 #et_create_context()返回
 * @param friendid 好友ID
 * @param notify 是否通知，ET_TRUE-通知，ET_FALSE-不通知
 *
 * @return 0表示成功\n
 *         <0表示失败，详见 #et_code
 */
et_int32 et_remove_buddy_notify(et_cloud_handle cloud_handle, const et_char * friendid, et_bool notify);

/**
 * @brief 获取好友列表
 * @param cloud_handle 由 #et_create_context()返回
 * @param friends_list 出参，好友UID存储buffer
 * @param list_num friends_list第一维大小
 * @param buddies_num 出参，存储到friends_list中的UID数量，不需要获取则传NULL
 * @param buddies_num_max 出参，获取到的好友UID数，不需要获取则传NULL
 * @param summary_info 获取简要信息类型设置，ET-TRUE：只获取UID信息；ET-FALSE:获取完整的信息，包括UID、nickname、username
 *
 * @return 0表示成功\n
 *         <0表示失败，详见 #et_code
 */
et_int32 et_get_buddies(et_cloud_handle cloud_handle, et_char friends_list[ ] [ ET_USER_ID_LEN_MAX ],   \
                        et_uint32 list_num, et_int32 * buddies_num, et_int32 *buddies_num_max, et_bool summary_info);


/**
 * @brief 创建群
 * @param cloud_handle 由 #et_create_context()返回
 * @param group_name 群名称
 * @param members_list 加入该群的群成员UID列表
 * @param members_num members_list中存储的UID数量
 * @param group_topic 出参，创建的群topic，系统唯一标识
 * @param group_topic_size group_topic的大小，单位byte
 *
 * @return 0表示成功\n
 *         <0表示失败，详见 #et_code
 */
et_int32 et_create_group(et_cloud_handle cloud_handle, const et_char * group_name,     \
                         const et_char members_list[][ET_USER_ID_LEN_MAX], et_uint32 members_num, 	\
                         et_char *group_topic, et_int32 group_topic_size);

/**
 * @brief 获取群列表
 * @param cloud_handle 由 #et_create_context()返回
 * @param groups_list 出参，群列表buffer
 * @param list_num  groups_list可存储的群Topic数量，即第一维的大小
 * @param groups_num 出参，存储到groups_list中的Topic数量，不需要则传NULL
 * @param groups_num_max 出参，获取到的群Topic数量，不需要则传NULL
 * @param summary_info 获取简要信息设置，ET_TRUE：只获取群Topic；ET_FAISE：获取完整的信息，包括groupname、grouptopic
 *
 * @return 0表示成功\n
 *         <0表示失败，详见 #et_code
 */
et_int32 et_get_groups(et_cloud_handle cloud_handle, et_char groups_list[][ET_TOPIC_LEN_MAX], et_uint32 list_num, 	\
                       et_int32 *groups_num, et_int32 *groups_num_max, et_bool summary_info);
/**
 * @brief 解散群
 * @note 只有群的创建者才可解散群
 * @param cloud_handle 由 #et_create_context()返回
 * @param groups_topic 期望解散的群唯一标识，群topic
 *
 * @return 0表示成功\n
 *         <0表示失败，详见 #et_code
 */
et_int32 et_destroy_group(et_cloud_handle cloud_handle, const et_char *groups_topic);

/**
 * @brief 退出群
 * @param cloud_handle 由 #et_create_context()返回
 * @param groups_topic 期望退出的群唯一标识，群topic
 *
 * @return 0表示成功\n
 *         <0表示失败，详见 #et_code
 */
et_int32 et_exit_group(et_cloud_handle cloud_handle, const et_char *group_topic);

/**
 * @brief 添加群成员
 * @param cloud_handle 由 #et_create_context()返回
 * @param groups_topic 添加到的群唯一标识，群topic
 * @param members_list 待添加的成员UID列表
 * @param members_num members_list中的UID数量
 *
 * @return 0表示成功\n
 *         <0表示失败，详见 #et_code
 */
et_int32 et_add_group_members(et_cloud_handle cloud_handle, const et_char *group_topic,      \
                              const et_char members_list[][ET_USER_ID_LEN_MAX], et_uint32 members_num);

/**
 * @brief 删除群成员
 * @param cloud_handle 由 #et_create_context()返回
 * @param groups_topic 删除成员的群唯一标识，群topic
 * @param members_list 待删除的成员UID列表
 * @param members_num members_list中的UID数量
 *
 * @return 0表示成功\n
 *         <0表示失败，详见 #et_code
 */
et_int32 et_remove_group_members(et_cloud_handle cloud_handle, const et_char *group_topic,   \
                                 const et_char members_list[][ET_USER_ID_LEN_MAX], et_uint32 members_num);

/**
 * @brief 获取群成员列表
 * @param cloud_handle 由 #et_create_context()返回
 * @param groups_topic 期望获取的群唯一标识，群topic
 * @param members_list 出参，群成员UID存储buffer
 * @param list_num members_list中可存储的UID数量
 * @param members_num 出参，存储到members_list中的UID数量，不需要时传NULL
 * @param members_num_max 出参，获取到的群成员数量，不需要时传NULL
 *
 * @return 0表示成功\n
 *         <0表示失败，详见 #et_code
 */
et_int32 et_get_group_members(et_cloud_handle cloud_handle, const et_char *group_topic,      \
                              et_char members_list[][ET_USER_ID_LEN_MAX], et_uint32 list_num, 	\
                              et_int32 *members_num, et_int32 *members_num_max);

/**
 * @brief 添加用户
 * @param cloud_handle 由 #et_create_context()返回
 * @param http_server HTTP服务器地址，应为包含IP及端口的字符串，e.g."192.168.1.1:1000"
 * @note 若http_server参数传入NULL则SDK从iLink获取地址
 * @param app_key 添加用户到该appkey,注册业务时获得
 * @param secret_key 注册业务时获得
 * @param user_name 用户账号
 * @param userid 出参，指向存储UID的buffer
 * @param userid_size userid的大小，单位byte
 *
 * @return 0表示成功\n
 *         <0表示失败，详见 #et_code
 */
et_int32 et_add_user(et_cloud_handle cloud_handle, const et_char *http_server, const et_char *app_key,     \
                     const et_char *secret_key, const et_char *user_name, et_char *userid, et_int32 userid_size);
#endif
#endif

