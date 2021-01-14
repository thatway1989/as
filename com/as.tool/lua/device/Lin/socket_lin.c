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
/* ============================ [ TYPES     ] ====================================================== */
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
	return r;
}

static int socket_write(Lin_DeviceType* dev, const char* data, size_t size)
{

}

static void socket_close(Lin_DeviceType* dev)
{

}

static void * rx_daemon(Lin_DeviceType* dev)
{

}
/* ============================ [ FUNCTIONS ] ====================================================== */



