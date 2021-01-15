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

#define LIN_MTU sizeof(struct lin_frame)

#define LIN_TYPE_INVALID          ((uint8_t)'I')
#define LIN_TYPE_BREAK            ((uint8_t)'B')
#define LIN_TYPE_SYNC             ((uint8_t)'S')
#define LIN_TYPE_HEADER           ((uint8_t)'H')
#define LIN_TYPE_DATA             ((uint8_t)'D')
#define LIN_TYPE_HEADER_AND_DATA  ((uint8_t)'F')
/* ============================ [ TYPES     ] ====================================================== */
typedef struct
{
	uint32_t busid;
	int s;
} Lin_SocketBusType;

struct lin_frame {
	uint8_t type;
	uint8_t pid;
	uint8_t dlc;
	uint8_t data[8];
	uint8_t checksum;
};
/* ============================ [ DECLARES  ] ====================================================== */
static int socket_open(Lin_DeviceType* dev, const char* option);
static int socket_write(Lin_DeviceType* dev, const char* data, size_t size);
static void socket_close(Lin_DeviceType* dev);
static void * rx_daemon(Lin_DeviceType* dev);
/* ============================ [ DATAS     ] ====================================================== */
const Lin_DeviceOpsType lin_socket_ops =
{
	.name = "socket",
	.open = socket_open,
	.close = socket_close,
	.write = socket_write,
};
/* ============================ [ LOCALS    ] ====================================================== */
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

static int socket_write(Lin_DeviceType* dev, const char* data, size_t size)
{
	int r = size;
	Lin_SocketBusType* bus = dev->param;
	struct lin_frame frame;

	memset(&frame, 0, LIN_MTU);
	if(((uint8_t)data[0] == LIN_TYPE_HEADER) && (size == 2)) {
		frame.type = LIN_TYPE_HEADER;
		frame.pid = (uint8_t)data[1];
	} else if(((uint8_t)data[0] == LIN_TYPE_DATA) && (size > 2)) {
		frame.type = LIN_TYPE_DATA;
		frame.dlc = size - 2;
		memcpy(&frame.data, &data[1], frame.dlc);
		frame.checksum = data[size-1];
	} else if(((uint8_t)data[0] == LIN_TYPE_HEADER_AND_DATA) && (size > 3)) {
		frame.type = LIN_TYPE_HEADER_AND_DATA;
		frame.pid = (uint8_t)data[1];
		frame.dlc = size - 3;
		memcpy(&frame.data, &data[2], frame.dlc);
		frame.checksum = data[size-1];
	} else {
		r = -1;
	}

	if(r == size) {
		if (send(bus->s, (const char*)&frame, LIN_MTU,0) != LIN_MTU) {
			perror("LIN socket write");
			ASWARNING(("LIN Socket %s send message failed!\n", dev->name));
			r = -2;
		}
	}

	return r;
}

static void socket_close(Lin_DeviceType* dev)
{
	Lin_SocketBusType* bus = dev->param;
	closesocket(bus->s);
	free(bus);
}

static void * rx_daemon(Lin_DeviceType* dev)
{

}
/* ============================ [ FUNCTIONS ] ====================================================== */



