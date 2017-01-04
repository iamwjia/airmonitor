/**
 * Copyright (c) 2016 ET iLink.
 * All rights reserved.
 */
/**
 *
 * @file et_config.h
 * @brief 本文件提供SDK功能配置、裁剪相关配置项
 * @date 3/10/2016
 * @author wangjia
 *
 */

#ifndef ET_CONFIG_H
#define ET_CONFIG_H
/**
 * @name 功能裁剪
 * 1 表示启用该功能，0 表示禁用该功能
 */
/**@{*/
#define ET_CONFIG_RTOS_EN	1	/**< RTOS接口，用户在RTOS下使用SDK应启用，否则禁用 */
#define ET_CONFIG_HTTP_EN	0	/**< HTTP功能 */
#define ET_CONFIG_FILE_EN	0	/**< 文件功能 */
#define ET_CONFIG_SERVER_EN	1	/**< 内网通信功能 */
#define ET_CONFIG_CLOUD_EN	1	/**< 外网通信功能 */
/**@}*/

/**
 * @name 日志打印配置
 * 1 表示启用该功能的日志打印，0 表示停用该功能的日志打印
 */
/**@{*/
#define ET_LOG_IM_EN		1	///< IM消息日志
#define ET_LOG_HTTP_EN		1	///< HTTP功能日志
#define ET_LOG_FILE_EN		1	///< 文件功能日志
#define ET_LOG_SOCKET_EN	1	///< Socket日志
#define ET_LOG_GEN_EN		1	///< 其他通用日志打印
#define ET_LOG_USER_EN      1   ///< 用户日志打印
/**@}*/

/**
 * @name 缓冲区配置
 * @attention ET_FILE_DOWNLOAD_BUFF_MAX 与 ET_FILE_UPLOAD_BUFF_MAX 设置不能小于256
 */
/**@{*/
#define ET_NET_IP_LEN_MAX			32      ///< IP缓冲大小
#define ET_NET_PORT_LEN_MAX			8       ///< Port缓冲大小
#define ET_FILE_NAME_LEN_MAX        128     ///< 文件名缓冲大小
#define ET_FILE_ID_LEN_MAX			128     ///< 文件ID缓冲大小
#define ET_FILE_DESCN_LEN_MAX       32      ///< 文件DESCN缓冲大小
#define ET_USER_ID_LEN_MAX			36      ///< UID缓冲大小
#define ET_LOCAL_CONN_NUM_MAX       5       ///< 本地通信允许的最大连接数
#define ET_MSG_LEN_MAX				1024    ///< 最大消息长度
#define ET_TOPIC_LEN_MAX			64      ///< Topic缓冲大小
#define ET_MSG_QUEUE_NUM            4       ///< 消息队列大小
#define ET_FILE_DOWNLOAD_BUFF_MAX   4096    ///< 文件下载的buffer大小,该设置不能小于256
#define ET_FILE_UPLOAD_BUFF_MAX     512     ///< 文件上传的buffer大小,该设置不能小于256
/**@}*/

/**
 * @name 网络超时设定
 * @note Socket接收超时视使用情况定，取值范围0~ET_ILINK_DEF_TIMEOUT_MS之间
 */
///@{
#define ET_ILINK_DEF_TIMEOUT_MS     3000    ///< iLink 默认的超时时间,单位ms
#define ET_ILINK_SEND_TIMEOUT_MS    1000    ///< iLink 消息超时,包含等待ack,单位ms
#define ET_ILINK_RECV_TIMEOUT_MS    5000    ///< iLink 消息接收超时时间,单位ms
#define ET_ILINK_ACK_TIMEOUT_MS     3000    ///< iLink 消息等待ACK超时时间,单位ms,仅对ET_QOS1、ET_QOS2有效
#define ET_SOCKET_CONN_TIMEOUT_MS   3000    ///< Socket 连接超时时间,单位ms
#define ET_SOCKET_SEND_TIMEOUT_MS   500     ///< Socekt 发送超时时间,单位ms
#define ET_GET_HOST_TIMEOUT_MS      500     ///< 域名解析超时时间,单位ms
#define ET_LOCAL_RECV_TIMEOUT_MS    50      ///< 内网接收超时时间,单位ms
///@}

/**
 * @name 模式设定
 */
///@{
#define ET_MODE_RECV_UNBLOCKING     0   ///< 1-接收非阻塞 0-接收阻塞
#if !ET_CONFIG_RTOS_EN
#define ET_MODE_RECV_UNBLOCKING     1   ///< 无RTOS情况下采用非阻塞
#endif
///@}
#endif
