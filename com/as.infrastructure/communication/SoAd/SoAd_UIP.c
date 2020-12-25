/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2013, ArcCore AB, Sweden, www.arccore.com.
 * Copyright (C) 2018  AS <parai@foxmail.com>
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by the
 * Free Software Foundation; See <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */
#if defined(USE_SOAD) && defined(USE_UIP)
/* ============================ [ INCLUDES] ====================================================== */
#include "Bsd.h"
#include "SoAd_Internal.h"
#include "asdebug.h"
/* ============================ [ MACROS] ====================================================== */
#define AS_LOG_SOAD  0
#define AS_LOG_SOADE 1

#ifndef SOAD_TCP_SOCKET_NUM
#define SOAD_TCP_SOCKET_NUM (2*SOAD_SOCKET_COUNT)
#endif
#ifndef SOAD_TX_BUFFER_SIZE
#define SOAD_TX_BUFFER_SIZE SOAD_RX_BUFFER_SIZE
#endif
/* ============================ [ TYPES] ====================================================== */
/* ============================ [ DECLARES] ====================================================== */
extern int  ffs(int v);
/* ============================ [ DATAS] ====================================================== */
static struct tcp_socket tcpSocket[SOAD_TCP_SOCKET_NUM];
static uint8  tcpBufRx[SOAD_TCP_SOCKET_NUM][SOAD_RX_BUFFER_SIZE];
static uint8  tcpBufTx[SOAD_TCP_SOCKET_NUM][SOAD_TX_BUFFER_SIZE];
static uint32 tcpSocketFlag = 0;
static uint32 tcpSocketConnectFlag = 0;
static uint32 tcpSocketErrorFlag = 0;
static const uint8* volatile tcpSocketDatatPtr[SOAD_TCP_SOCKET_NUM];
static int volatile tcpSocketDatatLen[SOAD_TCP_SOCKET_NUM];
/* ============================ [ LOCALS] ====================================================== */
/* Note: for this implementation, a socket frame should fit into a signle MTU(1500) frame */
int tcp_socket_data_callback(struct tcp_socket *s,
							 void *ptr,
							 const uint8_t *input_data_ptr,
							 int input_data_len)
{
	int slot = (int)(long)ptr;
	asAssert(slot == (s-tcpSocket));
	ASLOG(SOAD, ("%s(%d) message %d@%p\n",  __func__,  slot,  input_data_len, input_data_ptr));

	if(NULL == tcpSocketDatatPtr[slot])
	{
		int sc = 5;
		tcpSocketDatatPtr[slot] = input_data_ptr;
		tcpSocketDatatLen[slot] = input_data_len;

		while(--sc > 0)
		{   /* dispatch this message by SoAd */
			SoAd_MainFunction();
			#ifdef USE_DOIP
			extern void Dcm_MainFunction(void);
			Dcm_MainFunction();
			#endif
		}

		if(tcpSocketDatatPtr[slot] != NULL)
		{
			tcpSocketDatatPtr[slot] = NULL;
			ASLOG(SOADE, ("%s(%d) message %d@%p is not fully consumed, "
				  " maybe UIP buffer is too small,"
				  " or upper layer has some errors\n",
				  __func__, slot, input_data_len, input_data_ptr));
		}
	}
	else
	{
		ASLOG(SOADE, ("%s(%d) data coming too fast!\n",  __func__, slot));
	}

	return input_data_len;
}

void tcp_socket_event_callback(struct tcp_socket *s,
							   void *ptr,
							   tcp_socket_event_t event)
{
	int slot = (int)(long)ptr;
	asAssert(slot == (s-tcpSocket));
	ASLOG(SOAD, ("%s(%d,  %d)\n",  __func__,  slot, event));
	switch(event) {
		case TCP_SOCKET_CONNECTED:
			tcpSocketConnectFlag |= 1<<slot;
			break;
		case TCP_SOCKET_CLOSED:
			break;
		case TCP_SOCKET_TIMEDOUT:
			tcpSocketErrorFlag |= 1<<slot;
			break;
		case TCP_SOCKET_ABORTED:
			tcpSocketErrorFlag |= 1<<slot;
			break;
		case TCP_SOCKET_DATA_SENT:
			/* do nothing */
			break;
		default:
			break;
	}
}
/* ============================ [ FUNCTIONS ] ====================================================== */
int SoAd_SocketCloseImpl(int s)
{
	int r = -1;

	ASLOG(SOAD, ("%s(%d)\n", __func__, s));
	
	if((s < 32) && ((1<<s)&tcpSocketFlag))
	{
		tcp_socket_close(&tcpSocket[s]);
		tcp_socket_unregister(&tcpSocket[s]);
		tcpSocketFlag &= ~(1<<s);
		tcpSocketErrorFlag &= ~(1<<s);
		r = 0;
	}
	else
	{
		s = s - 32;
		/* TODO: for UDP */
	}
	return r;
}

int SoAd_SocketStatusCheckImpl(int s)
{
	int r = -1;

	if((s < 32) && (0 == ((1<<s)&tcpSocketErrorFlag)))
	{
		r = 0;
	}
	else
	{
		s = s - 32;
		/* TODO: for UDP */
	}
	return r;
}

int SoAd_SendImpl(int s, const void *data, size_t size, int flags)
{
	int r = 0;

	if((s < 32) && ((1<<s)&tcpSocketFlag))
	{
		r = tcp_socket_send(&tcpSocket[s], data, size);
	}

	return r;
}

int SoAd_SendToImpl(int s, const void *data, size_t size, uint32 RemoteIpAddress, uint16 RemotePort)
{
	return SoAd_SendImpl(s, data, size, 0);
}

int SoAd_CreateSocketImpl(int domain, int type, int protocol)
{
	int r = -1;
	int slot;
	if((SOCK_STREAM == type) && (0xFFFFFFFFUL != tcpSocketFlag))
	{
		slot = ffs(~tcpSocketFlag) - 1;
		ASLOG(SOAD, ("create uip socket on slot %d\n", slot));
		if(slot < SOAD_TCP_SOCKET_NUM)
		{
			tcpSocket[slot].ptr = (void*)(long)slot;
			tcp_socket_register(&tcpSocket[slot], NULL,
							tcpBufRx[slot], sizeof(tcpBufRx[slot]),
							tcpBufTx[slot], sizeof(tcpBufTx[slot]),
							tcp_socket_data_callback, tcp_socket_event_callback);
			tcpSocketFlag |= 1<<slot;
			tcpSocketConnectFlag &= ~(1<<slot);
			tcpSocketErrorFlag &= ~(1<<slot);
			tcpSocketDatatPtr[slot] = NULL;
			r = slot;
		}

	}
	else
	{
		ASLOG(SOADE, ("socket on UIP UDP not supported!\n"));
	}
	return r;
}

int SoAd_BindImpl(int s, uint16 SocketLocalPort, char* SocketLocalIpAddress)
{
	int r = -1;
	if((s < 32) && ((1<<s)&tcpSocketFlag))
	{
		tcp_socket_listen(&tcpSocket[s], SocketLocalPort);
		r = 0;
	}
	return r;
}

uint32 SoAd_GetLocalIp(void)
{
	return ((uint32)uip_hostaddr.u8[3]<<24)+((uint32)uip_hostaddr.u8[2]<<16)+((uint32)uip_hostaddr.u8[1]<<8)+(uint32)uip_hostaddr.u8[0];
}

int SoAd_ListenImpl(int s, int backlog)
{
	return 0;
}

int SoAd_AcceptImpl(int s, uint32 *RemoteIpAddress, uint16 *RemotePort)
{
	int r = -1;

	if((s < 32) && ((1<<s)&tcpSocketConnectFlag))
	{
		tcpSocketConnectFlag &= ~(1<<s);
		r = s;
	}

	return r;
}

int SoAd_RecvFromImpl(int s, void *mem, size_t len, int flags,
					  uint32 *RemoteIpAddress, uint16 *RemotePort)
{
	return SoAd_RecvImpl(s, mem, len, flags);
}

int SoAd_RecvImpl(int s, void *mem, size_t len, int flags)
{
	int r = 0;
	if((s<32) && (NULL != tcpSocketDatatPtr[s]))
	{
		r = len;
		if(len > tcpSocketDatatLen[s])
		{
			r = tcpSocketDatatLen[s];
		}

		memcpy(mem, tcpSocketDatatPtr[s], r);

		if(0u == (flags&MSG_PEEK))
		{
			tcpSocketDatatLen[s] -= r;

			if(0u == tcpSocketDatatLen[s])
			{
				tcpSocketDatatPtr[s] = NULL;
			}
			else
			{
				tcpSocketDatatPtr[s] += r;
			}
		}

		if(0u == tcpSocketDatatLen[s])
		{
			tcpSocketDatatPtr[s] = NULL;
		}

		ASLOG(SOAD, ("uip socket[%d]: %s %d bytes,  left %d bytes.\n", 
				s, (flags&MSG_PEEK)?"peek":"recv", r, tcpSocketDatatLen[s]));
		ASMEM(SOAD, "", mem, r);
	}

	return r;
}
#endif
