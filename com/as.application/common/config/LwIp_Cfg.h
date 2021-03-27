/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2016  AS <parai@foxmail.com>
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
#ifndef COM_AS_APPLICATION_COMMON_CONFIG_LWIP_CFG_H_
#define COM_AS_APPLICATION_COMMON_CONFIG_LWIP_CFG_H_
/* ============================ [ INCLUDES  ] ====================================================== */
#include "asdebug.h"
#include <sys/time.h>
/* ============================ [ MACROS    ] ====================================================== */
#define LWIP_ICMP 1
#define LWIP_IGMP 1
#ifndef LWIP_DHCP
#define LWIP_DHCP 0
#endif
#define ETH_MAC_ADDR {0xde,0xed,0xbe,0xef,0xaa,0xbb}
/* On Windows, by default use the VirtualBox Host-Only Ethernet Adapter #2,
 * And please configure its IPv4 address to 172.18.0.100. */
#ifndef LWIP_AS_LOCAL_IP_ADDR
#define LWIP_AS_LOCAL_IP_ADDR    "172.18.0.200"
#endif
#define LWIP_AS_LOCAL_IP_NETMASK "255.255.255.0"
#define LWIP_AS_LOCAL_IP_GATEWAY "172.18.0.1"
#define GET_BOOT_IPADDR ipaddr.addr = ipaddr_addr(LWIP_AS_LOCAL_IP_ADDR)
#define GET_BOOT_NETMASK netmask.addr = ipaddr_addr(LWIP_AS_LOCAL_IP_NETMASK)
#define GET_BOOT_GW gw.addr = ipaddr_addr(LWIP_AS_LOCAL_IP_GATEWAY)

#define EVENT_MASK_SLEEP_TCPIP EVENT_MASK_TaskLwip_Event22
#define EVENT_MASK_START_TCPIP EVENT_MASK_TaskLwip_Event23
#define TASK_ID_tcpip_task TASK_ID_TaskLwip

#define LWIP_RAND() rand()

#ifndef MEM_ALIGNMENT
#define MEM_ALIGNMENT 4
#endif

#ifndef ETH_PAD_SIZE
#define ETH_PAD_SIZE 2
#endif

#ifndef LWIP_DEBUG
#define LWIP_DEBUG 0
#endif
#if LWIP_DEBUG == 1
#define LWIP_DBG_MIN_LEVEL              LWIP_DBG_LEVEL_ALL
#define LWIP_DBG_TYPES_ON               LWIP_DBG_ON
#define ETHARP_DEBUG                    LWIP_DBG_ON
#define NETIF_DEBUG                     LWIP_DBG_ON
#define PBUF_DEBUG                      LWIP_DBG_OFF
#define API_LIB_DEBUG                   LWIP_DBG_ON
#define API_MSG_DEBUG                   LWIP_DBG_ON
#define SOCKETS_DEBUG                   LWIP_DBG_ON
#define ICMP_DEBUG                      LWIP_DBG_ON
#define IGMP_DEBUG                      LWIP_DBG_ON
#define INET_DEBUG                      LWIP_DBG_ON
#define IP_DEBUG                        LWIP_DBG_ON
#define IP_REASS_DEBUG                  LWIP_DBG_ON
#define RAW_DEBUG                       LWIP_DBG_ON
#define MEM_DEBUG                       LWIP_DBG_ON
#define MEMP_DEBUG                      LWIP_DBG_ON
#define SYS_DEBUG                       LWIP_DBG_OFF
#define TIMERS_DEBUG                    LWIP_DBG_OFF
#define TCP_DEBUG                       LWIP_DBG_ON
#define TCP_INPUT_DEBUG                 LWIP_DBG_ON
#define TCP_FR_DEBUG                    LWIP_DBG_ON
#define TCP_RTO_DEBUG                   LWIP_DBG_ON
#define TCP_CWND_DEBUG                  LWIP_DBG_ON
#define TCP_WND_DEBUG                   LWIP_DBG_ON
#define TCP_OUTPUT_DEBUG                LWIP_DBG_ON
#define TCP_RST_DEBUG                   LWIP_DBG_ON
#define TCP_QLEN_DEBUG                  LWIP_DBG_ON
#define UDP_DEBUG                       LWIP_DBG_ON
#define TCPIP_DEBUG                     LWIP_DBG_ON
#define PPP_DEBUG                       LWIP_DBG_ON
#define SLIP_DEBUG                      LWIP_DBG_ON
#define DHCP_DEBUG                      LWIP_DBG_ON
#define AUTOIP_DEBUG                    LWIP_DBG_ON
#define SNMP_MSG_DEBUG                  LWIP_DBG_ON
#define SNMP_MIB_DEBUG                  LWIP_DBG_ON
#define DNS_DEBUG                       LWIP_DBG_ON
#endif

#define SYS_LIGHTWEIGHT_PROT 1

#define LWIP_COMPAT_SOCKETS 0
/* ============================ [ TYPES     ] ====================================================== */
#ifdef __WINDOWS__
#include <errno.h>
typedef enum {
#ifndef EWOULDBLOCK
	EWOULDBLOCK = 0x0043,		// Operation would block
	EINPROGRESS = 0x0044,		// Operation now in progress
	EALREADY = 0x0045,			// Operation already in progress
	ENOTSOCK = 0x0046,			// Socket operation on non-socket
	EDESTADDRREQ = 0x0047,		// Destination address required
	EMSGSIZE = 0x0048,			// Message too long
	EPROTOTYPE = 0x0049,		// Protocol wrong type for socket
	ENOPROTOOPT = 0x004a,		// Protocol not available
	EPROTONOSUPPORT = 0x004b,	// Protocol not supported
	EOPNOTSUPP = 0x004d,		// Operation not supported
	// ENOTSUP  				// EOPNOTSUPP: Operation not supported
	EAFNOSUPPORT = 0x004f,		// Address family not supported by protocol family
	EADDRINUSE = 0x0050,		// Address already in use
	EADDRNOTAVAIL = 0x0051,		// Can't assign requested address
	ENETDOWN = 0x0052,			// Network is down
	ENETUNREACH = 0x0053,		// Network is unreachable
	ENETRESET = 0x0054,			// Network dropped connection on reset
	ECONNABORTED = 0x0055,		// Software caused connection abort
	ECONNRESET = 0x0056,		// Connection reset by peer
	ENOBUFS = 0x0057,			// No buffer space available
	EISCONN = 0x0058,			// Socket is already connected
	ENOTCONN = 0x0059,			// Socket is not connected
	ETIMEDOUT = 0x005c,			// Operation timed out
	ECONNREFUSED = 0x005d,		// Connection refused
	ELOOP = 0x005e,				// Too many levels of symbolic links
	EHOSTUNREACH = 0x0061,		// No route to host
#else
	SoAd_TcpIpErrorType_dummy=0
#endif
} SoAd_TcpIpErrorType; /** @req SOAD117 */
#endif
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
#endif /* COM_AS_APPLICATION_COMMON_CONFIG_LWIP_CFG_H_ */
