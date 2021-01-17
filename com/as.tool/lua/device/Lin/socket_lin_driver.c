/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2021  AS <parai@foxmail.com>
 *
 * This source code is free software; you lin redistribute it and/or modify it
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
#include <sys/ioctl.h>
#include <sys/socket.h>
#endif
#include <sys/queue.h>
#include <sys/time.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#ifdef __WINDOWS__
/* Link with ws2_32.lib */
#ifndef __GNUC__
#pragma comment(lib, "Ws2_32.lib")
#else
/* -lwsock32 */
#endif
#endif
/* ============================ [ MACROS    ] ====================================================== */
#define LIN_PORT_MIN  100
#define LIN_BUS_NODE_MAX 32	/* maximum node on the bus port */

#define in_range(c, lo, up)  ((uint8_t)c >= lo && (uint8_t)c <= up)
#define isprint(c)           in_range(c, 0x20, 0x7f)

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define LIN_MTU sizeof(Lin_FrameType)
#define mLINID(frame)  (frame->pid)
#define mLINDLC(frame) (frame->dlc)

#define LIN_TYPE_INVALID          ((uint8_t)'I')
#define LIN_TYPE_BREAK            ((uint8_t)'B')
#define LIN_TYPE_SYNC             ((uint8_t)'S')
#define LIN_TYPE_HEADER           ((uint8_t)'H')
#define LIN_TYPE_DATA             ((uint8_t)'D')
#define LIN_TYPE_HEADER_AND_DATA  ((uint8_t)'F')
/* ============================ [ TYPES     ] ====================================================== */
typedef struct
{
	uint8_t type;
	uint8_t pid;
	uint8_t dlc;
	uint8_t data[8];
	uint8_t checksum;
} Lin_FrameType;

struct Lin_SocketHandle_s
{
	int s; /* lin raw socket: accept */
	int error_counter;
	STAILQ_ENTRY(Lin_SocketHandle_s) entry;
};
struct Lin_SocketHandleList_s
{
	int s; /* lin raw socket: listen */
	Lin_FrameType frame;
	struct timeval tmv;
	STAILQ_HEAD(,Lin_SocketHandle_s) head;
};

struct Lin_Filter_s {
	uint32_t mask;
	uint32_t code;
	STAILQ_ENTRY(Lin_Filter_s) entry;
};

struct Lin_FilterList_s
{
	STAILQ_HEAD(,Lin_Filter_s) head;
};
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
static struct Lin_SocketHandleList_s* socketH = NULL;
static struct timeval m0;
static struct Lin_FilterList_s* linFilterH = NULL;
/* ============================ [ LOCALS    ] ====================================================== */
#ifdef __WINDOWS__
#else
static int WSAGetLastError(void) { perror(""); return errno; }
static int closesocket(int s) { return close(s); }
#endif

static float get_elapsed_time(struct timeval* stop, struct timeval* start)
{
	float rtim = stop->tv_sec-start->tv_sec;

	if(stop->tv_usec > start->tv_usec)
	{
		rtim += (float)(stop->tv_usec-start->tv_usec)/1000000.0;
	}
	else
	{
		rtim = rtim - 1 + (float)(1000000.0+stop->tv_usec-start->tv_usec)/1000000.0;
	}

	return rtim;
}

static int init_socket(int port)
{
	int ercd;
	int s;
	struct sockaddr_in service;

#ifdef __WINDOWS__
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0) {
		printf("socket function failed with error: %d\n", WSAGetLastError());
		return FALSE;;
	}

	service.sin_family = AF_INET;
	service.sin_addr.s_addr = inet_addr("127.0.0.1");
	service.sin_port = (u_short)htons(LIN_PORT_MIN+port);
	ercd = bind(s, (struct sockaddr *) &(service), sizeof (struct sockaddr));
	if (ercd < 0) {
		printf("bind to port %d failed with error: %d\n", port, WSAGetLastError());
		closesocket(s);
		return FALSE;
	}

	if (listen(s, LIN_BUS_NODE_MAX) < 0) {
		printf("listen failed with error: %d\n", WSAGetLastError());
		closesocket(s);
		return FALSE;
	}

	#ifdef __WINDOWS__
	/* set to non blocking mode */
	u_long iMode = 1;
	ioctlsocket(s, FIONBIO, &iMode);
	#else
	int iMode = 1;
	ioctl(s, FIONBIO, (char *)&iMode);
	#endif

	printf("lin(%d) socket driver on-line!\n",port);

	socketH = malloc(sizeof(struct Lin_SocketHandleList_s));
	assert(socketH);
	STAILQ_INIT(&socketH->head);
	socketH->s = s;
	socketH->frame.type = LIN_TYPE_INVALID;

	return TRUE;
}
static void try_accept(void)
{
	struct Lin_SocketHandle_s* handle;
	int s = accept(socketH->s, NULL, NULL);

	if(s >= 0)
	{
		handle = malloc(sizeof(struct Lin_SocketHandle_s));
		assert(handle);
		handle->s = s;
		handle->error_counter = 0;
		#ifdef __WINDOWS__
		/* set to non blocking mode */
		u_long iMode = 1;
		ioctlsocket(s, FIONBIO, &iMode);
		#else
		int iMode = 1;
		ioctl(s, FIONBIO, (char *)&iMode);
		#endif
		STAILQ_INSERT_TAIL(&socketH->head,handle,entry);
		printf("lin socket %X on-line!\n",s);
	}
}

static void remove_socket(struct Lin_SocketHandle_s* h)
{
	STAILQ_REMOVE(&socketH->head,h,Lin_SocketHandle_s,entry);
	closesocket(h->s);
	free(h);
}
static void log_msg(Lin_FrameType* frame,float rtim)
{
	int bOut = FALSE;

	struct Lin_Filter_s* filter;

	if(NULL == linFilterH)
	{
		bOut = TRUE;
	}
	else
	{
		STAILQ_FOREACH(filter,&linFilterH->head,entry)
		{
			if((mLINID(frame)&filter->mask) == (filter->code&filter->mask))
			{
				bOut = TRUE;
			}
		}
	}

	if(bOut)
	{
		int i;
		int dlc;
		printf("pid=%02X,dlc=%02d,data=[",mLINID(frame),mLINDLC(frame));
		dlc = mLINDLC(frame);
		if((dlc < 8) || (dlc > 8))
		{
			dlc = 8;
		}
		for(i=0; i<dlc; i++)
		{
			printf("%02X,", frame->data[i]);
		}

		printf("] checksum=%02X [", frame->checksum);

		for(i=0; i<dlc; i++)
		{
			if(isprint(frame->data[i]))
			{
				printf("%c",frame->data[i]);
			}
			else
			{
				printf(".");
			}
		}

		printf("] @ %f s\n", rtim);
	}
}

static void try_recv_forward(void)
{
	int len;
	Lin_FrameType frame;
	struct Lin_SocketHandle_s* h;
	struct Lin_SocketHandle_s* h2;
	STAILQ_FOREACH(h,&socketH->head,entry)
	{
		len = recv(h->s, (void*)&frame, LIN_MTU, 0);
		if(LIN_MTU == len)
		{
			struct timeval m1;
			gettimeofday(&m1,NULL);
			float rtim = get_elapsed_time(&m1, &m0);

			if(frame.type == LIN_TYPE_HEADER)
			{
				printf("%c: ", (char)frame.type);
				if(socketH->frame.type != LIN_TYPE_INVALID)
				{
					printf("Lin Error: type %c pid=0x%02X\n", (char)socketH->frame.type, socketH->frame.pid);
				}
				else
				{
					memcpy(&socketH->frame, &frame, LIN_MTU);
				}
				gettimeofday(&socketH->tmv,NULL);
			}
			else if(frame.type == LIN_TYPE_DATA)
			{
				if(socketH->frame.type != LIN_TYPE_HEADER)
				{
					printf("Lin Error: type %c pid=0x%02X\n", (char)socketH->frame.type, socketH->frame.pid);
				}
				else
				{
					memcpy(&socketH->frame.dlc, &frame.dlc, 10);
					log_msg(&socketH->frame,rtim);
				}
				socketH->frame.type = LIN_TYPE_INVALID;
			}
			else if(frame.type == LIN_TYPE_HEADER_AND_DATA)
			{
				printf("%c: ", (char)frame.type);
				log_msg(&frame,rtim);
				socketH->frame.type = LIN_TYPE_INVALID;
			}
			else
			{
				printf("Lin Error: type %c(%02X) is invalid\n", (char)frame.type, frame.type);
			}
			h->error_counter = 0;

			STAILQ_FOREACH(h2,&socketH->head,entry)
			{
				if(h != h2)
				{
					if (send(h2->s, (const char*)&frame, LIN_MTU,0) != LIN_MTU) {
						printf("send failed with error: %d, remove this node %X!\n", WSAGetLastError(),h2->s);
						remove_socket(h2);
						break;
					}
				}
			}
		}
		else if(-1 == len)
		{
#ifdef __WINDOWS__
			if(10035!= WSAGetLastError())
#else
			if(EAGAIN != errno)
#endif
			{
				printf("recv failed with error: %d, remove this node %X!\n", WSAGetLastError(),h->s);
				remove_socket(h);
				break;
			}
			else
			{
				/* Resource temporarily unavailable. */
			}
		}
		else
		{
			#ifdef __LINUX__
			printf("recv failed with error: %d, remove this node %X!\n", WSAGetLastError(),h->s);
			remove_socket(h);
			break;
			#else
			h->error_counter ++;
			if(h->error_counter > 10) {
				printf("recv failed with error: %d, remove this node %X!\n", WSAGetLastError(),h->s);
				remove_socket(h);
				break;
			}
			#endif
		}
	}
}
static void schedule(void)
{
	try_accept();
	try_recv_forward();

	if(socketH->frame.type != LIN_TYPE_INVALID)
	{
		struct timeval m1;
		gettimeofday(&m1,NULL);
		float rtim = get_elapsed_time(&m1, &socketH->tmv);
		if(rtim > 1) {
			printf("Lin Error: timeout type %c pid=0x%02X\n", (char)socketH->frame.type,  socketH->frame.pid);
			socketH->frame.type = LIN_TYPE_INVALID;
		}
	}
}

static void arg_filter(char* s)
{
	char *code;
	struct Lin_Filter_s* filter = malloc(sizeof(struct Lin_Filter_s));
	assert(filter);

	code = strchr(s,'#');
	assert(code);
	code = &code[1];

	filter->mask = strtoul(s,NULL,16);
	filter->code = strtoul(code,NULL,16);

	if (NULL == linFilterH)
	{
		linFilterH = malloc(sizeof(struct Lin_FilterList_s));
		assert(linFilterH);
		STAILQ_INIT(&linFilterH->head);
	}

	STAILQ_INSERT_TAIL(&linFilterH->head,filter,entry);
}
/* ============================ [ FUNCTIONS ] ====================================================== */
int main(int argc,char* argv[])
{
	if(argc < 2)
	{
		printf( "Usage:%s <port> : 'port' is a number start from 0\n"
			    "  -f <Mask>#<Code> : optional parameter for LIN log Mask and Code, in hex\n"
			    "Example:\n"
				"  %s 0 -f 30#10",argv[0],argv[0]);
		return -1;
	}
	gettimeofday(&m0,NULL);
	if(FALSE==init_socket(atoi(argv[1])))
	{
		return -1;
	}

	argc = argc - 2;
	argv = argv + 2;
	while(argc >= 2)
	{
		if( 0 == strcmp(argv[0],"-f") )
		{
			arg_filter(argv[1]);
		}

		argc = argc - 2;
		argv = argv + 2;
	}

	for(;;)
	{
		schedule();
		usleep(100);
	}

	return 0;
}
