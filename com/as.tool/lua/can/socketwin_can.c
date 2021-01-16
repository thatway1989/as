/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2015  AS <parai@foxmail.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */
/* ============================ [ INCLUDES  ] ====================================================== */
/* most of the code copy from https://github.com/linux-can/can-utils */
#ifdef __WINDOWS__
#include <winsock2.h>
#include <windows.h>
#include <Ws2tcpip.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef SLIST_ENTRY
#undef SLIST_ENTRY
#endif

#include <sys/queue.h>
#include <pthread.h>

#include "Std_Types.h"
#include "lascanlib.h"
#include "asdebug.h"

/* Link with ws2_32.lib */
#ifndef __GNUC__
#pragma comment(lib, "Ws2_32.lib")
#else
/* -lwsock32 */
#endif
/* ============================ [ MACROS    ] ====================================================== */
/* http://www.cs.ubbcluj.ro/~dadi/compnet/labs/lab3/udp-broadcast.html
 * https://www.tack.ch/multicast/ */
//#define USE_CAN_UDP

#ifdef USE_CAN_UDP
#define CAN_SOCKET_TYPE SOCK_DGRAM
#define CAN_SOCKET_IP   INADDR_BROADCAST
#else
#define CAN_SOCKET_TYPE SOCK_STREAM
#define CAN_SOCKET_IP   inet_addr("127.0.0.1")
#endif

#define CAN_MAX_DLEN 64 /* 64 for CANFD */
#define CAN_MTU sizeof(struct can_frame)
#define CAN_PORT_MIN  80

#define CAN_FRAME_TYPE_RAW 0
#define CAN_FRAME_TYPE_MTU 1
#define CAN_FRAME_TYPE CAN_FRAME_TYPE_RAW
#if (CAN_FRAME_TYPE == CAN_FRAME_TYPE_RAW)
#define mCANID(frame) ( ((uint32_t)frame.data[CAN_MAX_DLEN+0]<<24)+((uint32_t)frame.data[CAN_MAX_DLEN+1]<<16)	\
					   +((uint32_t)frame.data[CAN_MAX_DLEN+2]<< 8)+((uint32_t)frame.data[CAN_MAX_DLEN+3]) )

#define mSetCANID(frame,canid) do {	frame.data[CAN_MAX_DLEN+0] = (uint8_t)(canid>>24);	\
									frame.data[CAN_MAX_DLEN+1] = (uint8_t)(canid>>16);	\
									frame.data[CAN_MAX_DLEN+2] = (uint8_t)(canid>> 8);	\
									frame.data[CAN_MAX_DLEN+3] = (uint8_t)(canid); } while(0)

#define mCANDLC(frame) ( (uint8_t) frame.data[CAN_MAX_DLEN+4] )
#define mSetCANDLC(frame,dlc) do { frame.data[CAN_MAX_DLEN+4] = dlc; } while(0)
#else
#define mCANID(frame) frame.can_id

#define mSetCANID(frame,canid) do {	frame.can_id = canid; } while(0)

#define mCANDLC(frame) ( frame->can_dlc )
#define mSetCANDLC(frame,dlc) do { frame.can_dlc = dlc; } while(0)
#endif

/* ============================ [ TYPES     ] ====================================================== */
/**
 * struct can_frame - basic CAN frame structure
 * @can_id:  CAN ID of the frame and CAN_*_FLAG flags, see canid_t definition
 * @can_dlc: frame payload length in byte (0 .. 8) aka data length code
 *           N.B. the DLC field from ISO 11898-1 Chapter 8.4.2.3 has a 1:1
 *           mapping of the 'data length code' to the real payload length
 * @data:    CAN frame payload (up to 8 byte)
 */
struct can_frame {
#if (CAN_FRAME_TYPE == CAN_FRAME_TYPE_RAW)
	uint8_t    data[CAN_MAX_DLEN + 5];
#else
	uint32_t can_id;  /* 32 bit CAN_ID + EFF/RTR/ERR flags */
	uint8_t    can_dlc; /* frame payload length in byte (0 .. CAN_MAX_DLEN) */
	uint8_t    data[CAN_MAX_DLEN] __attribute__((aligned(8)));
#endif
};
struct Can_SocketHandle_s
{
	uint32_t busid;
	uint32_t port;
	uint32_t baudrate;
	can_device_rx_notification_t rx_notification;
	int s; /* can raw socket */
	struct sockaddr_in addr;
	STAILQ_ENTRY(Can_SocketHandle_s) entry;
};
struct Can_SocketHandleList_s
{
	pthread_t rx_thread;
	volatile boolean   terminated;
	STAILQ_HEAD(,Can_SocketHandle_s) head;
};
/* ============================ [ DECLARES  ] ====================================================== */
static boolean socket_probe(uint32_t busid,uint32_t port,uint32_t baudrate,can_device_rx_notification_t rx_notification);
static boolean socket_write(uint32_t busid,uint32_t port,uint32_t canid,uint32_t dlc,uint8_t* data);
static void socket_close(uint32_t busid,uint32_t port);
static void * rx_daemon(void *);
/* ============================ [ DATAS     ] ====================================================== */
const Can_DeviceOpsType can_socketwin_ops =
{
#ifdef __LINUX__
	.name = "socketwin",
#else
	.name = "socket",
#endif
	.probe = socket_probe,
	.close = socket_close,
	.write = socket_write,
};
static struct Can_SocketHandleList_s* socketH = NULL;
/* ============================ [ LOCALS    ] ====================================================== */
#ifdef __WINDOWS__
#else
static int WSAGetLastError(void) { perror(""); return errno; }
static int closesocket(int s) { return close(s); }
#endif

static struct Can_SocketHandle_s* getHandle(uint32_t port)
{
	struct Can_SocketHandle_s *handle,*h;
	handle = NULL;
	if(NULL != socketH)
	{
		STAILQ_FOREACH(h,&socketH->head,entry)
		{
			if(h->port == port)
			{
				handle = h;
				break;
			}
		}
	}
	return handle;
}

static boolean socket_probe(uint32_t busid,uint32_t port,uint32_t baudrate,can_device_rx_notification_t rx_notification)
{
	boolean rv = TRUE;;
	struct Can_SocketHandle_s* handle;
	if(NULL == socketH)
	{
		socketH = malloc(sizeof(struct Can_SocketHandleList_s));
		asAssert(socketH);
		STAILQ_INIT(&socketH->head);

		socketH->terminated = TRUE;
#ifdef __WINDOWS__
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
	}

	handle = getHandle(port);

	if(handle)
	{
		ASWARNING(("CAN socket port=%d is already on-line, no need to probe it again!\n",port));
		rv = FALSE;
	}
	else
	{
		int s;
		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(struct sockaddr_in));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = CAN_SOCKET_IP;
		addr.sin_port = htons(CAN_PORT_MIN+port);
		/* open socket */
		if ((s = socket(AF_INET, CAN_SOCKET_TYPE, 0)) < 0) {
			ASWARNING(("CAN Socket port=%d open failed!\n",port));
			rv = FALSE;
		}
#ifdef USE_CAN_UDP
#if 0
		if ( rv ) {
			char broadcast = '1';
			if(setsockopt(s, SOL_SOCKET, SO_BROADCAST,&broadcast, sizeof(broadcast)) < 0) {
				ASWARNING(("Error in setting broadcast option: %d\n", WSAGetLastError()));
				(void)closesocket(s);
				rv = FALSE;
			}
		}
#else
		if ( rv ) {
			struct ip_mreq imreq;
			memset(&imreq, 0, sizeof(struct ip_mreq));
			imreq.imr_multiaddr.s_addr = inet_addr("226.0.0.1");
			imreq.imr_interface.s_addr = INADDR_ANY;
			if(setsockopt(s, IPPROTO_IP, IP_ADD_MEMBERSHIP,
					  (const void *)&imreq, sizeof(struct ip_mreq)) < 0) {
				ASWARNING(("Error in setting multicast option: %d\n", WSAGetLastError()));
				(void)closesocket(s);
				rv = FALSE;
			}
		}
#endif
		if ( rv ) {
			addr.sin_addr.s_addr = INADDR_ANY;
			if (bind(s, (struct sockaddr*)&addr, sizeof(struct sockaddr)) < 0) {
				ASWARNING(("Error in binding: %d\n", WSAGetLastError()));
				(void)closesocket(s);
				rv = FALSE;
			}
		}


#else
		if( rv )
		{
			/* Connect to server. */
			int ercd = connect(s, (struct sockaddr *) & addr, sizeof (struct sockaddr));
			if (ercd < 0) {
				ASWARNING(("connect function failed with error: %d\n", WSAGetLastError()));
				ercd = closesocket(s);
				if (ercd < 0){
					ASWARNING(("closesocket function failed with error: %d\n", WSAGetLastError()));
				}
				rv = FALSE;
			}
		}
#endif
		if( rv ) {
			#ifdef __WINDOWS__
			/* set to non blocking mode */
			u_long iMode = 1;
			ioctlsocket(s, FIONBIO, &iMode);
			#else
			int iMode = 1;
			ioctl(s, FIONBIO, (char *)&iMode);
			#endif
		}

		if( rv )
		{	/* open port OK */
			handle = malloc(sizeof(struct Can_SocketHandle_s));
			asAssert(handle);
			handle->busid = busid;
			handle->port = port;
			handle->baudrate = baudrate;
			handle->rx_notification = rx_notification;
			handle->s = s;
			memcpy(&(handle->addr),&addr,sizeof(addr));
			STAILQ_INSERT_TAIL(&socketH->head,handle,entry);
		}
		else
		{
			rv = FALSE;
		}
	}

	if( (TRUE == socketH->terminated) &&
		(FALSE == STAILQ_EMPTY(&socketH->head)) )
	{
		if( 0 == pthread_create(&(socketH->rx_thread),NULL,rx_daemon,NULL))
		{
			socketH->terminated = FALSE;
		}
		else
		{
			asAssert(0);
		}
	}

	return rv;
}
static boolean socket_write(uint32_t busid,uint32_t port,uint32_t canid,uint32_t dlc,uint8_t* data)
{
	boolean rv = TRUE;
	struct Can_SocketHandle_s* handle = getHandle(port);
	(void)busid;
	if(handle != NULL)
	{
		struct can_frame frame;
		mSetCANID(frame , canid);
		mSetCANDLC(frame , dlc);
		asAssert(dlc <= CAN_MAX_DLEN);
		memcpy(frame.data,data,dlc);
#ifdef USE_CAN_UDP
		if (sendto(handle->s, (const char*)&frame, CAN_MTU, 0, (struct sockaddr*)&handle->addr, sizeof(struct sockaddr)) != CAN_MTU) {
#else
		if (send(handle->s, (const char*)&frame, CAN_MTU,0) != CAN_MTU) {
#endif
			perror("CAN socket write");
			ASWARNING(("CAN Socket port=%d send message failed!\n",port));
			rv = FALSE;
		}
	}
	else
	{
		rv = FALSE;
		ASWARNING(("CAN Socket port=%d is not on-line, not able to send message!\n",port));
	}

	return rv;
}
static void socket_close(uint32_t busid,uint32_t port)
{
	struct Can_SocketHandle_s* handle = getHandle(port);
	(void)busid;
	if(NULL != handle)
	{
		closesocket(handle->s);
		STAILQ_REMOVE(&socketH->head,handle,Can_SocketHandle_s,entry);

		free(handle);

		if(TRUE == STAILQ_EMPTY(&socketH->head))
		{
			socketH->terminated = TRUE;
		}
	}
}

static void rx_notifiy(struct Can_SocketHandle_s* handle)
{
	int nbytes,len = sizeof(struct sockaddr_in);
	struct can_frame frame;
	struct sockaddr_in addr;
	nbytes = recvfrom(handle->s, (char*)&frame, sizeof(frame), 0, (struct sockaddr*)&addr, &len);
	if (nbytes < 0) {
		perror("CAN socket read");
		ASWARNING(("CAN Socket port=%d read message failed!\n",handle->port));
	}
	else if(nbytes==sizeof(frame))
	{
		handle->rx_notification(handle->busid,mCANID(frame),mCANDLC(frame),frame.data);
	}
	else
	{
		/* read failed with invalid length if the remote set to non-blocking */
	}

}
static void * rx_daemon(void * param)
{
	(void)param;
	struct Can_SocketHandle_s* handle;
	while(FALSE == socketH->terminated)
	{
		STAILQ_FOREACH(handle,&socketH->head,entry)
		{
			rx_notifiy(handle);
		}
	}

	return NULL;
}

/* ============================ [ FUNCTIONS ] ====================================================== */

