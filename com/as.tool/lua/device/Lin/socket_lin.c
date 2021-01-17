/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2021  AS <parai@foxmail.com>
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
#include "laslinlib.h"
#include "asdebug.h"

/* Link with ws2_32.lib */
#ifndef __GNUC__
#pragma comment(lib, "Ws2_32.lib")
#else
/* -lwsock32 */
#endif
/* ============================ [ MACROS    ] ====================================================== */
#define LIN_PORT_MIN  100
/* ============================ [ TYPES     ] ====================================================== */
typedef struct
{
	uint32_t busid;
	int s;
} Lin_SocketBusType;
/* ============================ [ DECLARES  ] ====================================================== */
static int socket_open(Lin_DeviceType* dev, const char* option);
static int socket_write(Lin_DeviceType* dev, Lin_FrameType* frame);
static int socket_read(Lin_DeviceType* dev, Lin_FrameType* frame);
static void socket_close(Lin_DeviceType* dev);
/* ============================ [ DATAS     ] ====================================================== */
const Lin_DeviceOpsType lin_socket_ops =
{
	.name = "socket",
	.open = socket_open,
	.close = socket_close,
	.write = socket_write,
	.read = socket_read,
};
/* ============================ [ LOCALS    ] ====================================================== */
#ifdef __WINDOWS__
#else
static int WSAGetLastError(void) { perror(""); return errno; }
static int closesocket(int s) { return close(s); }
#endif
static int socket_open(Lin_DeviceType* dev, const char* option)
{
	int r = 0;
	uint32_t busid = (uint32_t)atoi(&dev->name[11]);
	int s;
	struct sockaddr_in addr;
	Lin_SocketBusType* bus;

#ifdef __WINDOWS__
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(LIN_PORT_MIN+busid);
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		ASWARNING(("LIN Socket busid=%d open failed!\n",busid));
		r = -1;
	}

	if(0 == r) {
		int ercd = connect(s, (struct sockaddr *) & addr, sizeof (struct sockaddr));
		if (ercd < 0) {
			ASWARNING(("connect function failed with error: %d\n", WSAGetLastError()));
			ercd = closesocket(s);
			if (ercd < 0){
				ASWARNING(("closesocket function failed with error: %d\n", WSAGetLastError()));
			}
			r = -2;
		}
	}

	if(0 == r) {
		#ifdef __WINDOWS__
		/* set to non blocking mode */
		u_long iMode = 1;
		ioctlsocket(s, FIONBIO, &iMode);
		#else
		int iMode = 1;
		ioctl(s, FIONBIO, (char *)&iMode);
		#endif
	}

	if(0 == r) {
		bus = malloc(sizeof(Lin_SocketBusType));
		if(bus != NULL) {
			bus->busid = busid;
			bus->s = s;
			dev->param = (void*)bus;
		} else {
			r = -3;
		}
	}

	return r;
}

static int socket_write(Lin_DeviceType* dev, Lin_FrameType* frame)
{
	int r;
	Lin_SocketBusType* bus = dev->param;

	r = send(bus->s, (const char*)frame, LIN_MTU, 0);
	if(r != LIN_MTU) {
		perror("LIN socket write");
		ASWARNING(("LIN Socket %s send message failed with error %d!\n", dev->name, WSAGetLastError()));
	}

	return r;
}

static int socket_read(Lin_DeviceType* dev, Lin_FrameType* frame)
{
	int r = 0;
	Lin_SocketBusType* bus = dev->param;
	int nbytes,len = sizeof(struct sockaddr_in);
	struct sockaddr_in addr;

	nbytes = recvfrom(bus->s, (char*)frame, LIN_MTU, 0, (struct sockaddr*)&addr, &len);
	if(nbytes == LIN_MTU) {
		r = nbytes;
	} else if(-1 == nbytes) {
		#ifdef __WINDOWS__
		if(10035!= WSAGetLastError()) {
		#else
		if(EAGAIN != errno) {
		#endif
			r = -3;
		} else {
			/* Resource temporarily unavailable. */
		}
	} else {
		r = -2;
	}

	if(r < 0) {
		ASWARNING(("LIN Socket %s read message failed with error %d!\n", dev->name, WSAGetLastError()));
	}

	return r;
}

static void socket_close(Lin_DeviceType* dev)
{
	Lin_SocketBusType* bus = dev->param;
	closesocket(bus->s);
	free(bus);
}
/* ============================ [ FUNCTIONS ] ====================================================== */



