#include "et_types.h"
#include "et_base.h"
#include "esp_common.h"
#include "freertos/FreeRTOS.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "et_client.h"

et_uint32 et_system_time_ms(void)
{
	return system_get_time()/1000;
}


void et_sleep_ms(et_int32 milliseconds)
{
	vTaskDelay(milliseconds/portTICK_RATE_MS);
}


et_socket_t et_socket_create(et_socket_proto_t type)
{
	et_socket_t fd;
	et_int32 i_type;
	et_int32 tmp;
	et_int32 flags;
	et_int32 udpbufsize=2;
	et_int32 mode = 1;
	
	switch(type)
	{
	case ET_SOCKET_TCP:
		i_type = SOCK_STREAM;
		break;
	case ET_SOCKET_UDP:
		i_type = SOCK_DGRAM;
		break;
	}
	fd = socket(AF_INET, i_type, 0);
	if(fd < 0)
	{
		ET_LOG_USER("Create socket failed\n");
		return ET_SOCKET_NULL;
	}
	
	switch(type)
	{
	case ET_SOCKET_TCP:
		tmp = 1;
		if(setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &tmp, sizeof(tmp)) < 0)
		{
			ET_LOG_USER("set SO_KEEPALIVE fail\n");
		}
		tmp = 60;//60s
		if(setsockopt(fd, IPPROTO_TCP,TCP_KEEPIDLE,&tmp,sizeof(tmp))<0)
		{
			ET_LOG_USER("set TCP_KEEPIDLE fail\n");
		}
		tmp = 6;
		if(setsockopt(fd, IPPROTO_TCP,TCP_KEEPINTVL,&tmp,sizeof(tmp))<0)
		{
			ET_LOG_USER("set TCP_KEEPINTVL fail\n");
		}
		tmp = 5;
		if(setsockopt(fd, IPPROTO_TCP,TCP_KEEPCNT,&tmp,sizeof(tmp))<0)
		{
			ET_LOG_USER("set TCP_KEEPCNT fail\n");
		}

		flags = fcntl(fd, F_GETFL, 0);

		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
		break;
	case ET_SOCKET_UDP:
		ioctlsocket(fd,FIONBIO,&mode);
		if(setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &udpbufsize,sizeof(et_int32)) != 0)
		{
			ET_LOG_USER("UDP BC Server setsockopt error,errno:%d", errno);
			close(fd);
			fd = ET_SOCKET_NULL;
		}
		break;
	}

	return fd;
}

et_int32 et_socket_close(et_socket_t socket)
{
	return close(socket);
}

et_int32 et_socket_connect(et_socket_t socket, et_int8 *ip, et_uint16 port, et_uint32 time_out_ms)
{
	struct sockaddr_in s_addr;
	et_int32 rc;
	fd_set wret;
	struct timeval interval = {time_out_ms/1000, time_out_ms%1000*1000};
	
	s_addr.sin_family = AF_INET;
	s_addr.sin_addr.s_addr = inet_addr(ip);
	s_addr.sin_port = htons(port);
//	s_addr.sin_addr.s_addr = inet_addr("192.168.31.244");
//	s_addr.sin_port = htons(8000);

	rc = connect(socket, (struct sockaddr*)&s_addr, sizeof(s_addr));
	if(EINPROGRESS != errno)
	{
		ET_LOG_USER("Connect failed %d\n", errno);
		return ET_FAILURE;
	}
	FD_ZERO(&wret);
	FD_SET(socket,&wret);
	rc = select(socket+1, NULL, &wret, NULL, &interval);
	if(rc < 0 || FD_ISSET(socket, &wret) == 0)
	{
		ET_LOG_USER("Connect select failed %d\n", errno);
		return ET_FAILURE;
	}
	
	return ET_SUCCESS;
}

// static et_uint32 g_count = 0;
et_int32 et_socket_send(et_socket_t socket, et_uint8 *send_buf, et_uint32 buf_len, et_uint32 time_out_ms)
{
	struct timeval timer = {time_out_ms/1000, time_out_ms%1000*1000};
	fd_set write_set;
	et_int32 rc = 0;
	et_int32 i_test_flag = 0;
	// g_count += buf_len;
	// ET_LOG_USER("socket send %d\n", g_count);
	FD_ZERO(&write_set);
	FD_SET(socket, &write_set);
	// if(buf_len > 1024)
	// {
		// i_test_flag = buf_len;
		// buf_len = 850;
		// ET_LOG_USER("Socket failed test %d\n", buf_len);
	// }
//	ET_LOG_USER("send timeout %d->%d:%d\n", time_out_ms, timer.tv_sec, timer.tv_usec);
	if((rc = select(socket+1, NULL, &write_set, NULL, &timer)) > 0)
		rc = send(socket, send_buf, buf_len, 0);
	else
		ET_LOG_USER("Send select failed %d errno->%d == %ds:%dus\n", rc, errno, timer.tv_sec, timer.tv_usec);
	if(rc < 0 && !(errno == 0 || errno == EINPROGRESS || errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
	{
		ET_LOG_USER("Socket %d can't send now errno->%d == %d->%d:%d:%d\n", socket, errno, send_buf, buf_len, rc, time_out_ms);
		rc = ET_FAILURE;
	}
	else if(rc == 0)
	{
		ET_LOG_USER("Send none this time %d\n", socket);
	}
	else if(rc < 0)
	{
		ET_LOG_USER("Socket %d can't send now errno->%d == %d->%d:%d:%d\n", socket, errno, send_buf, buf_len, rc, time_out_ms);
		rc = 0;
	}

	// if(i_test_flag && rc > 0)
		// rc = i_test_flag;
	else
	{
		ET_LOG_USER("Socket %d send %d\n", socket, rc);
		// print_hex("socket data:", send_buf, buf_len);
	}
	return rc;
}

// et_int32 g_recv_count = 0;
et_int32 et_socket_recv(et_socket_t socket, et_uint8 *recv_buf, et_uint32 buf_len, et_uint32 time_out_ms)
{
	struct timeval timer = {0, 0};
	fd_set read_set;
	et_int32 rc =0;
	timer.tv_sec = time_out_ms/1000;
	timer.tv_usec = time_out_ms%1000*1000;
	FD_ZERO(&read_set);
	FD_SET(socket, &read_set);
//	ET_LOG_USER("recv timeout %d-> %d:%d\n", time_out_ms, timer.tv_sec, timer.tv_usec);
	if((rc = select(socket+1, &read_set, NULL, NULL, &timer)) > 0)
		rc = recv(socket, recv_buf, buf_len, 0);
	if(rc <= 0 && !(errno == 0 || errno == EINTR || errno == EAGAIN || errno == EINPROGRESS || errno == EWOULDBLOCK))	///< Socket error
	{
		ET_LOG_USER("Socket %d can't receive now errno->%d == %d->%d:%d:%d\n", socket, errno, recv_buf, buf_len, rc, time_out_ms);
		rc = ET_FAILURE;
	}
	else if(rc < 0)
	{
		rc = 0;
	}
	// else if(rc >= 512)
	// {
		// g_recv_count++;
		// ET_LOG_USER("Socket recv %d packets:%d\n", g_recv_count, rc);
	// }
	return rc;
}

// #if ET_CONFIG_SERVER_EN
et_int32 et_socket_bind(et_socket_t socket, et_uint16 port)
{
	struct sockaddr_in addr;
	memset((et_char*)&addr,0,sizeof(addr));
	
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr=INADDR_ANY;
	if(0 != bind(socket, (struct sockaddr *)&addr, sizeof(addr)))
		return ET_FAILURE;
	return ET_SUCCESS;
}

et_int32 et_socket_listen(et_socket_t socket)
{
	if(0 != listen(socket, 0))
		return ET_FAILURE;
	return ET_SUCCESS;
}

et_socket_t et_socket_accept(et_socket_t socket, et_addr_info_t *remote_info)
{
	et_socket_t fd = ET_SOCKET_NULL;
	struct sockaddr_in addr;
	et_int32 addr_len = sizeof(struct sockaddr_in);
	
	if(ET_SOCKET_NULL != (fd = accept(socket, (struct sockaddr*)&addr, &addr_len)))
	{
		et_strncpy(remote_info->ip_str, inet_ntoa(addr.sin_addr.s_addr), sizeof(remote_info->ip_str));
		remote_info->port = ntohs(addr.sin_port);
		ET_LOG_USER("Acccept connect %s:%d\n", remote_info->ip_str, remote_info->port);
	}
	return fd;
}

et_int32 et_socket_send_to(et_socket_t socket, et_uint8 *send_buf, et_uint32 buf_len, et_addr_info_t *remote_info,
                                  et_uint32 time_out_ms)
{
	struct sockaddr_in toaddr;
	et_int32 rc;
	fd_set write_set;
	struct timeval timer = {0, 0};

	timer.tv_sec = time_out_ms/1000;
	timer.tv_usec = time_out_ms%1000*1000;
	
	FD_ZERO(&write_set);
	FD_SET(socket, &write_set);
	
	toaddr.sin_family = AF_INET;
	inet_aton(remote_info->ip_str, &(toaddr.sin_addr.s_addr));
	toaddr.sin_port = htons(remote_info->port);
	toaddr.sin_len = sizeof(struct sockaddr_in);
	ET_LOG_USER("Send to %s->0x%x...\n", remote_info->ip_str, toaddr.sin_addr.s_addr);

	if(select(socket+1, NULL, &write_set, NULL, &timer) > 0)
		rc = sendto(socket, send_buf, buf_len, 0, (struct sockaddr *)&toaddr, sizeof(struct sockaddr_in));
	else if(rc < 0 && !(errno == 0 || errno == EINTR || errno == EAGAIN || errno == EINPROGRESS || errno == EWOULDBLOCK))	///< Socket error
		return ET_FAILURE;
	return rc;
}

et_int32 et_socket_recv_from(et_socket_t socket, et_uint8 *recv_buf, et_uint32 buf_len, et_addr_info_t *remote_info,
                                    et_uint32 time_out_ms)
{
	et_int32 rc;
	struct sockaddr_in addr;
	et_int32 addr_len = sizeof(struct sockaddr_in);
	fd_set read_set;
	struct timeval timer = {0, 0};

	timer.tv_sec = time_out_ms/1000;
	timer.tv_usec = time_out_ms%1000*1000;
	
	FD_ZERO(&read_set);
	FD_SET(socket, &read_set);
	if((rc = select(socket+1, &read_set, NULL, NULL, &timer)) > 0)
	{
		rc = recvfrom(socket, recv_buf, buf_len, 0, (struct sockaddr*)&addr,&addr_len);
		et_strncpy(remote_info->ip_str, inet_ntoa(addr.sin_addr.s_addr), sizeof(remote_info->ip_str));
		remote_info->port = ntohs(addr.sin_port);
		ET_LOG_USER("UDP packet 0x%x:0x%x\n", addr.sin_addr.s_addr, addr.sin_port);
	}
	else if(rc < 0 && !(errno == 0 || errno == EINTR || errno == EAGAIN || errno == EINPROGRESS || errno == EWOULDBLOCK))	///< Socket error
	{
		ET_LOG_USER("recv from failed\n");
		return ET_FAILURE;
	}
	return rc;
}

et_int32 et_get_localip(et_int8 *local_ip, et_uint32 size)
{
	struct ip_info ipconfig;
	if(wifi_get_ip_info(STATION_IF, &ipconfig))
	{
		et_strncpy(local_ip, inet_ntoa(ipconfig.ip.addr), size);
		ET_LOG_USER("Local ip %s\n", local_ip);
		return ET_SUCCESS;
	}
	return ET_FAILURE;
}

// #endif

et_int32 et_gethostbyname(et_int8 *name, et_addr_info_t *addr, et_uint32 time_out_ms)
{
	struct hostent *i_host_info = NULL;
	i_host_info = gethostbyname((const char *)name);
	if(NULL == i_host_info)
		return ET_FAILURE;
	else
	{
		strncpy(addr->ip_str,  inet_ntoa(*(i_host_info->h_addr)), sizeof(addr->ip_str));
		return ET_SUCCESS;
	}
}
