/*-------------------------------- Arctic Core ------------------------------
 * Copyright (C) 2013, ArcCore AB, Sweden, www.arccore.com.
 * Contact: <contact@arccore.com>
 * Copyright (C) 2018  AS <parai@foxmail.com>
 *
 * You may ONLY use this file:
 * 1)if you have a valid commercial ArcCore license and then in accordance with
 * the terms contained in the written license agreement between you and ArcCore,
 * or alternatively
 * 2)if you follow the terms found in GNU General Public License version 2 as
 * published by the Free Software Foundation and appearing in the file
 * LICENSE.GPL included in the packaging of this file or here
 * <http://www.gnu.org/licenses/old-licenses/gpl-2.0.txt>
 *-------------------------------- Arctic Core -----------------------------*/
#ifdef USE_SOAD
#include <string.h>
#include "Os.h"
#include "Mcu.h"
#include "SoAd.h"
#include "SoAd_Internal.h"
#include "PduR_SoAd.h"
#include "TcpIp.h"
#include "Bsd.h"
#include "SchM_cfg.h"
#include "MemMap.h"

#ifdef USE_SD
#include "SD.h"
#endif

#include "Det.h"
#if defined(USE_DEM)
#include "Dem.h"
#endif
#if defined(USE_UDPNM)
#include "UdpNm.h"
#endif
#include "asdebug.h"
#ifdef USE_SHELL
#include "shell.h"
#endif

#define AS_LOG_SOAD 1
#define AS_LOG_SOADE 1

typedef enum {
  SOAD_UNINITIALIZED = 0,
  SOAD_INITIALIZED
} SoadStateType;

#ifdef USE_SHELL
static int shellIfconfig(int argc, char* argv[])
{
#ifdef USE_LWIP
	extern struct netif* sys_get_netif(void);
	struct netif* netif = sys_get_netif();

	SHELL_printf("Ethernet Interface     : %02X:%02X:%02X:%02X:%02X:%02X, mtu=%d\n",
				netif->hwaddr[0],netif->hwaddr[1],netif->hwaddr[2],
				netif->hwaddr[3],netif->hwaddr[4],netif->hwaddr[5],
				netif->mtu);
	SHELL_printf("IP addr of interface   : %"U16_F".%"U16_F".%"U16_F".%"U16_F"\n",
		ip4_addr1_16(&netif->ip_addr), ip4_addr2_16(&netif->ip_addr),
		ip4_addr3_16(&netif->ip_addr), ip4_addr4_16(&netif->ip_addr));
	SHELL_printf("Netmask of interface   : %"U16_F".%"U16_F".%"U16_F".%"U16_F"\n",
		ip4_addr1_16(&netif->netmask), ip4_addr2_16(&netif->netmask),
		ip4_addr3_16(&netif->netmask), ip4_addr4_16(&netif->netmask));
	SHELL_printf("Gateway of interface   : %"U16_F".%"U16_F".%"U16_F".%"U16_F"\n",
		ip4_addr1_16(&netif->gw), ip4_addr2_16(&netif->gw),
		ip4_addr3_16(&netif->gw), ip4_addr4_16(&netif->gw));
#endif
#ifdef USE_UIP
	SHELL_printf("Ethernet Interface     : %02X:%02X:%02X:%02X:%02X:%02X\n",
				uip_lladdr.addr[0],uip_lladdr.addr[1],uip_lladdr.addr[2],
				uip_lladdr.addr[3],uip_lladdr.addr[4],uip_lladdr.addr[5]);
	SHELL_printf("IP addr of interface   : %d.%d.%d.%d\n",
			uip_hostaddr.u8[0], uip_hostaddr.u8[1],
			uip_hostaddr.u8[2], uip_hostaddr.u8[3]);
	SHELL_printf("Netmask of interface   : %d.%d.%d.%d\n",
			uip_netmask.u8[0], uip_netmask.u8[1],
			uip_netmask.u8[2], uip_netmask.u8[3]);
	SHELL_printf("Gateway of interface   : %d.%d.%d.%d\n",
			uip_draddr.u8[0], uip_draddr.u8[1],
			uip_draddr.u8[2], uip_draddr.u8[3]);
#endif
	return 0;
}
static SHELL_CONST ShellCmdT cmdIfconfig  = {
		shellIfconfig,
		0,1,
		"ifconfig",
		"ifconfig",
		"show network information\n",
		{NULL,NULL}
};
SHELL_CMD_EXPORT(cmdIfconfig)
#endif

static SoadStateType ModuleStatus = SOAD_UNINITIALIZED;
static SoadArcLinkStatusType LinkStatus = SOAD_ARC_LINKDOWN;

SocketAdminType SocketAdminList[SOAD_SOCKET_COUNT];

PduAdminListType PduAdminList[SOAD_PDU_ROUTE_COUNT];

#ifdef USE_CLIB_ASHEAP
boolean SoAd_BufferGet(uint32 size, uint8** buffPtr)
{
	boolean res;

	*buffPtr = malloc(size);
	res = (*buffPtr != NULL) ? TRUE : FALSE;

	return res;
}
void SoAd_BufferFree(uint8* buffPtr)
{
	free(buffPtr);
}
#else
typedef struct  {
	uint8*	bufferPtr;
	uint16	bufferLen;
	boolean	bufferInUse;
} AdminBufferType;

static uint8 buffer1[42];
static uint8 buffer2[SOAD_RX_BUFFER_SIZE];
static uint8 buffer3[SOAD_RX_BUFFER_SIZE];
static uint8 buffer4[SOAD_RX_BUFFER_SIZE];
#define BUFFER_COUNT	4
static AdminBufferType	adminBuffer[BUFFER_COUNT] = {
		{
				.bufferInUse = FALSE,
				.bufferLen = sizeof(buffer1),
				.bufferPtr = buffer1,
		},
		{
				.bufferInUse = FALSE,
				.bufferLen = sizeof(buffer2),
				.bufferPtr = buffer2,
		},
		{
				.bufferInUse = FALSE,
				.bufferLen = sizeof(buffer3),
				.bufferPtr = buffer3,
		},
		{
				.bufferInUse = FALSE,
				.bufferLen = sizeof(buffer4),
				.bufferPtr = buffer4,
		},
};


boolean SoAd_BufferGet(uint32 size, uint8** buffPtr)
{
	boolean returnCode = FALSE;
	uint16 i;
	imask_t state;
	Irq_Save(state);

	*buffPtr = NULL;
	for (i = 0; i < BUFFER_COUNT; i++) {
		if ((!adminBuffer[i].bufferInUse) && (adminBuffer[i].bufferLen >= size)) {
			adminBuffer[i].bufferInUse = TRUE;
			*buffPtr = adminBuffer[i].bufferPtr;
			returnCode = TRUE;
			break;
		}
	}

	Irq_Restore(state);
	return returnCode;
}

void SoAd_BufferFree(uint8* buffPtr)
{
	uint16 i;
	imask_t state;
	Irq_Save(state);

	for (i = 0; i < BUFFER_COUNT; i++) {
		if (adminBuffer[i].bufferPtr == buffPtr) {
			adminBuffer[i].bufferInUse = FALSE;
			uint16 j;

			for (j = 0; j < adminBuffer[i].bufferLen; j++) {
				adminBuffer[i].bufferPtr[j] = 0xaa;
			}
			break;
		}
	}

    Irq_Restore(state);
}
#endif

void SoAd_SocketClose(uint16 sockNr)
{
	uint16 i;

	ASLOG(SOAD, ("close(%d) from state %d\n", sockNr, SocketAdminList[sockNr].SocketState));
	switch (SocketAdminList[sockNr].SocketState) {
	case SOCKET_UDP_READY:
	case SOCKET_TCP_LISTENING:
		SoAd_SocketCloseImpl(SocketAdminList[sockNr].SocketHandle);
		SocketAdminList[sockNr].SocketHandle = -1;
		SocketAdminList[sockNr].ConnectionHandle = -1;
		SocketAdminList[sockNr].SocketState = SOCKET_INIT;
		break;


	case SOCKET_TCP_READY:
		SoAd_SocketCloseImpl(SocketAdminList[sockNr].ConnectionHandle);
		SocketAdminList[sockNr].ConnectionHandle = -1;
		SocketAdminList[sockNr].RemoteIpAddress = inet_addr(SoAd_Config.SocketConnection[sockNr].SocketRemoteIpAddress);
		SocketAdminList[sockNr].RemotePort = htons(SoAd_Config.SocketConnection[sockNr].SocketRemotePort);

		SocketAdminList[sockNr].SocketState = SOCKET_TCP_LISTENING;
		for (i = 0; i < SOAD_SOCKET_COUNT; i++) {
			if (i == sockNr) continue;

			if (SocketAdminList[sockNr].SocketHandle == SocketAdminList[i].SocketHandle) {
				if (SocketAdminList[i].SocketState == SOCKET_TCP_LISTENING) {
					SocketAdminList[sockNr].SocketState = SOCKET_DUPLICATE;
					break;
				}
			}
		}

		break;

	default:
		/* This should never happen! */
		DET_REPORTERROR(MODULE_ID_SOAD, 0, SOAD_SOCKET_CLOSE_ID, SOAD_E_SHALL_NOT_HAPPEN);
		break;

	}

}

void SoAd_SocketStatusCheck(uint16 sockNr)
{
	int sockErr;

	if (SocketAdminList[sockNr].SocketProtocolIsTcp) {
		sockErr = SoAd_SocketStatusCheckImpl(SocketAdminList[sockNr].ConnectionHandle);
	} else {
		sockErr = SoAd_SocketStatusCheckImpl(SocketAdminList[sockNr].SocketHandle);
	}

	if (sockErr != 0) {
		ASLOG(SOADE, ("socket[%d] status not okay, closed!\n", sockNr));
		SoAd_SocketClose(sockNr);
	}
}

int SoAd_RecvIpMessage(uint16 sockNr, uint8* buff, uint32 msgLen, int flags) {
	int nBytes = 0;
	uint32 RemoteIpAddress;
	uint16 RemotePort;

	if (SocketAdminList[sockNr].SocketProtocolIsTcp) {
		nBytes = SoAd_RecvImpl(SocketAdminList[sockNr].ConnectionHandle, buff, msgLen, flags);
		
		if (nBytes <= 0) {
			SoAd_SocketStatusCheck(sockNr);
		}
	} else {
		nBytes = SoAd_RecvFromImpl(SocketAdminList[sockNr].SocketHandle, buff, msgLen, flags, &RemoteIpAddress, &RemotePort);
		if (nBytes > 0) {
			SocketAdminList[sockNr].RemotePort = RemotePort;
			SocketAdminList[sockNr].RemoteIpAddress = RemoteIpAddress;
		} else {
			SoAd_SocketStatusCheck(sockNr);
		}
	}

	return nBytes;
}

uint16 SoAd_SendIpMessage(uint16 sockNr, uint8* buff, uint32 msgLen)
{
	uint16 bytesSent = 0;
	int rv;
	ASMEM(SOAD,"TX",buff,msgLen);
	if (SocketAdminList[sockNr].SocketProtocolIsTcp) {
		rv = SoAd_SendImpl(SocketAdminList[sockNr].ConnectionHandle, buff, msgLen, 0);
	} else {
		rv = SoAd_SendToImpl(SocketAdminList[sockNr].SocketHandle, buff, msgLen,
									SocketAdminList[sockNr].RemoteIpAddress,
									SocketAdminList[sockNr].RemotePort);
	}

	if(rv > 0) {
		bytesSent = (uint16) rv;
	}

	return bytesSent;
}


static void socketCreate(uint16 sockNr)
{
    int sockFd;
    int sockType;
	int r;
	ASLOG(SOAD, ("!!!soad:%s begin sockNr=%d\n",__FUNCTION__,sockNr));

    if (SocketAdminList[sockNr].SocketProtocolIsTcp) {
    	sockType = SOCK_STREAM;
    } else {
    	sockType = SOCK_DGRAM;
    }

    sockFd = SoAd_CreateSocketImpl(AF_INET, sockType, 0);
    if (sockFd >= 0) {
        ASLOG(SOAD, ("SoAd create socket[%d] okay.\n", sockNr));
		r = SoAd_BindImpl(sockFd, SocketAdminList[sockNr].SocketConnectionRef->SocketLocalPort,
				SocketAdminList[sockNr].SocketConnectionRef->SocketLocalIpAddress);
		if(r >= 0) {
			ASLOG(SOAD, ("SoAd bind socket[%d] on port %d okay.\n", sockNr, 
						SocketAdminList[sockNr].SocketConnectionRef->SocketLocalPort));
            if (!SocketAdminList[sockNr].SocketProtocolIsTcp) {
            	// Now the UDP socket is ready for receive/transmit
            	SocketAdminList[sockNr].SocketHandle = sockFd;
            	SocketAdminList[sockNr].SocketState = SOCKET_UDP_READY;
            } else {
                if  ( SoAd_ListenImpl(sockFd, 20) == 0 ){	// NOTE: What number of the backlog?
                	// Now the TCP socket is ready for receive/transmit
                    ASLOG(SOAD, ("SoAd listen socket[%d] okay.\n", sockNr));
                	SocketAdminList[sockNr].SocketHandle = sockFd;
                	SocketAdminList[sockNr].SocketState = SOCKET_TCP_LISTENING;
                } else {
                    ASLOG(SOADE,("SoAd listen socket[%d] failed.\n",sockNr));
                	SoAd_SocketCloseImpl(sockFd);
                }
            }
    	} else {
            ASLOG(SOADE,("SoAd bind socket[%d] failed.\n",sockNr));
    		SoAd_SocketCloseImpl(sockFd);
    	}
    } else {
    	// Socket creation failed
    	// Do nothing, try again later
        ASLOG(SOADE, ("SoAd create socket[%d] failed! try again later.\n", sockNr));
    }
}


static void socketAccept(uint16 sockNr)
{
	uint16 i;
	int clientFd;
	uint32 RemoteIpAddress;
	uint16 RemotePort;
	ASLOG(SOAD, ("!!!soad:%s begin\n",__FUNCTION__));

	clientFd = SoAd_AcceptImpl(SocketAdminList[sockNr].SocketHandle, &RemoteIpAddress, &RemotePort);

	if( clientFd != (-1))
	{
    	SocketAdminList[sockNr].ConnectionHandle = clientFd;
		SocketAdminList[sockNr].RemotePort = RemotePort;
		SocketAdminList[sockNr].RemoteIpAddress = RemoteIpAddress;
		SocketAdminList[sockNr].SocketState = SOCKET_TCP_READY;

		// Check if there is any free duplicate of this socket
		for (i = 0; i < SOAD_SOCKET_COUNT; i++) {
			if ((SocketAdminList[i].SocketState == SOCKET_DUPLICATE)
				&& (SoAd_Config.SocketConnection[i].SocketProtocol == SoAd_Config.SocketConnection[sockNr].SocketProtocol)
				&& (SoAd_Config.SocketConnection[i].SocketLocalPort == SoAd_Config.SocketConnection[sockNr].SocketLocalPort)) {
				// Yes, move the old socket to this
				SocketAdminList[i].SocketHandle = SocketAdminList[sockNr].SocketHandle;
				SocketAdminList[i].SocketState = SOCKET_TCP_LISTENING;

				// SocketAdminList[sockNr].SocketHandle = -1;
				break;
			}
		}
	}
}

uint8 SoAd_GetNofCurrentlyUsedTcpSockets() {
	uint8 count = 0;
	uint16 i;

	for (i = 0; i < SOAD_SOCKET_COUNT; i++) {
		if ((SOCKET_TCP_READY == SocketAdminList[i].SocketState) && SocketAdminList[i].SocketProtocolIsTcp) {
			count++;
		}
	}

	return count;
}

static void socketTcpRead(uint16 sockNr)
{
	ASLOG(SOAD, ("!!!soad:%s begin\n",__FUNCTION__));
#ifdef USE_PDUR
    BufReq_ReturnType result;

	switch (SocketAdminList[sockNr].SocketConnectionRef->AutosarConnectorType) {
	case SOAD_AUTOSAR_CONNECTOR_PDUR:
		if (SocketAdminList[sockNr].SocketRouteRef != NULL) {
		    int nBytes;
		    PduInfoType pduInfo;

			if (SoAd_BufferGet(SOAD_RX_BUFFER_SIZE, &pduInfo.SduDataPtr)) {
				nBytes = SoAd_RecvImpl(SocketAdminList[sockNr].ConnectionHandle, pduInfo.SduDataPtr, SOAD_RX_BUFFER_SIZE, MSG_PEEK);
				SoAd_SocketStatusCheck(sockNr);

				if ((nBytes > 0) && (nBytes >= SocketAdminList[sockNr].SocketRouteRef->DestinationSduLength)) {
					if  (!SocketAdminList[sockNr].SocketConnectionRef->PduProvideBufferEnable) {
						// IF-type
						pduInfo.SduLength = SoAd_RecvImpl(SocketAdminList[sockNr].ConnectionHandle, pduInfo.SduDataPtr, SocketAdminList[sockNr].SocketRouteRef->DestinationSduLength,0);

						(void)PduR_SoAdIfRxIndication(SocketAdminList[sockNr].SocketRouteRef->DestinationPduId, &pduInfo);
					} else {
						// TP-type
					    PduLengthType len;
					    result = PduR_SoAdTpStartOfReception(SocketAdminList[sockNr].SocketRouteRef->DestinationPduId, SocketAdminList[sockNr].SocketRouteRef->DestinationSduLength, &len);
						if (result == BUFREQ_OK && len>0) {
							pduInfo.SduLength = SocketAdminList[sockNr].SocketRouteRef->DestinationSduLength;
							nBytes = SoAd_RecvImpl(SocketAdminList[sockNr].SocketHandle, pduInfo.SduDataPtr, SocketAdminList[sockNr].SocketRouteRef->DestinationSduLength,0);

							/* Let pdur copy received data */
							if(len < SocketAdminList[sockNr].SocketRouteRef->DestinationSduLength)
							{
							    PduInfoType pduInfoChunk;
							    PduLengthType lenToSend = SocketAdminList[sockNr].SocketRouteRef->DestinationSduLength;
								/* We need to copy in smaller parts */
							    pduInfoChunk.SduDataPtr = pduInfo.SduDataPtr;

							    while(lenToSend > 0)
							    {
							    	if(lenToSend >= len){
							    		PduR_SoAdTpCopyRxData(SocketAdminList[sockNr].SocketRouteRef->DestinationPduId, &pduInfoChunk, &len);
							    		lenToSend -= len;
							    	}else{
							    		PduR_SoAdTpCopyRxData(SocketAdminList[sockNr].SocketRouteRef->DestinationPduId, &pduInfoChunk, &lenToSend);
							    		lenToSend = 0;
							    	}
							    }
							}else{
								PduR_SoAdTpCopyRxData(SocketAdminList[sockNr].SocketRouteRef->DestinationPduId, &pduInfo, &len);
							}
							/* Finished reception */
							(void)PduR_SoAdTpRxIndication(SocketAdminList[sockNr].SocketRouteRef->DestinationPduId, NTFRSLT_OK);
						} else if (result != BUFREQ_BUSY) {
							DET_REPORTERROR(MODULE_ID_SOAD, 0, SOAD_SOCKET_TCP_READ_ID, SOAD_E_UL_RXBUFF);
						}
					}
				}

				SoAd_BufferFree(pduInfo.SduDataPtr);
			} else {
				// No buffer available: continue poll() Report error SOAD_E_UPPERBUFF to DEM
				DET_REPORTERROR(MODULE_ID_SOAD, 0, SOAD_SOCKET_TCP_READ_ID, SOAD_E_UPPERBUFF);
				(void)PduR_SoAdTpRxIndication(SocketAdminList[sockNr].SocketRouteRef->DestinationPduId, NTFRSLT_E_NO_BUFFER);
			}
		} else {
			DET_REPORTERROR(MODULE_ID_SOAD, 0, SOAD_SOCKET_TCP_READ_ID, SOAD_E_CONFIG_INVALID);
		}
		break;	// SOAD_AUTOSAR_CONNECTOR_PDUR
#ifdef USE_DOIP
	case SOAD_AUTOSAR_CONNECTOR_DOIP:
		ASLOG(SOAD, ("!!!soad:%s begin, DoIP_HandleTcpRx\n",__FUNCTION__));
		DoIp_HandleTcpRx(sockNr);
		break;	// SOAD_AUTOSAR_CONNECTOR_DOIP
#endif
	default:
		DET_REPORTERROR(MODULE_ID_SOAD, 0, SOAD_SOCKET_TCP_READ_ID, SOAD_E_NOCONNECTOR);
		break;
	}
#endif /* USE_PDUR */
}


static void socketUdpRead(uint16 sockNr)
{
#ifdef USE_PDUR
    BufReq_ReturnType result;
	int nBytes;
	PduInfoType pduInfo;

	switch (SocketAdminList[sockNr].SocketConnectionRef->AutosarConnectorType) {
	case SOAD_AUTOSAR_CONNECTOR_PDUR:
		if (SocketAdminList[sockNr].SocketRouteRef != NULL) {
			if (SoAd_BufferGet(SOAD_RX_BUFFER_SIZE, &pduInfo.SduDataPtr)) {
				nBytes = SoAd_RecvFromImpl(SocketAdminList[sockNr].SocketHandle, pduInfo.SduDataPtr, SOAD_RX_BUFFER_SIZE, MSG_PEEK,
							&SocketAdminList[sockNr].RemoteIpAddress, &SocketAdminList[sockNr].RemotePort);
				SoAd_SocketStatusCheck(sockNr);

				if (nBytes > 0){
					if(nBytes >= SocketAdminList[sockNr].SocketRouteRef->DestinationSduLength) {
						if  (!SocketAdminList[sockNr].SocketConnectionRef->PduProvideBufferEnable) {
							// IF-type
							pduInfo.SduLength = SoAd_RecvFromImpl(SocketAdminList[sockNr].SocketHandle, pduInfo.SduDataPtr, nBytes, 0,
										&SocketAdminList[sockNr].RemoteIpAddress, &SocketAdminList[sockNr].RemotePort);
							/* NOTE Find out how autosar connector and user really shall be used. This is just one interpretation
							 * support for XCP, CDD will have to be added later when supported */
							switch(SocketAdminList[sockNr].SocketRouteRef->UserRxIndicationUL){
							#if defined(USE_UDPNM)
							case SOAD_UL_UDPNM:
								(void)UdpNm_SoAdIfRxIndication(SocketAdminList[sockNr].SocketRouteRef->DestinationPduId, &pduInfo);
								break;
							#endif
							#ifdef USE_SD
							case SOAD_UL_SD:
								Sd_RxIndication(SocketAdminList[sockNr].SocketRouteRef->DestinationPduId, &pduInfo);
								break;
							#endif
							default:
								(void)PduR_SoAdIfRxIndication(SocketAdminList[sockNr].SocketRouteRef->DestinationPduId, &pduInfo);
								break;
							}
						} else {
							// TP-type
							PduLengthType len;
							result = PduR_SoAdTpStartOfReception(SocketAdminList[sockNr].SocketRouteRef->DestinationPduId, SocketAdminList[sockNr].SocketRouteRef->DestinationSduLength, &len);
							if (result == BUFREQ_OK && len > 0) {
								pduInfo.SduLength = SocketAdminList[sockNr].SocketRouteRef->DestinationSduLength;
								nBytes = SoAd_RecvFromImpl(SocketAdminList[sockNr].SocketHandle, pduInfo.SduDataPtr, SocketAdminList[sockNr].SocketRouteRef->DestinationSduLength, 0,
											&SocketAdminList[sockNr].RemoteIpAddress, &SocketAdminList[sockNr].RemotePort);

								/* Let pdur copy received data */
								if(len < SocketAdminList[sockNr].SocketRouteRef->DestinationSduLength)
								{
									PduInfoType pduInfoChunk;
									PduLengthType lenToSend = SocketAdminList[sockNr].SocketRouteRef->DestinationSduLength;
									/* We need to copy in smaller parts */
									pduInfoChunk.SduDataPtr = pduInfo.SduDataPtr;

									while(lenToSend > 0)
									{
										if(lenToSend >= len){
											PduR_SoAdTpCopyRxData(SocketAdminList[sockNr].SocketRouteRef->DestinationPduId, &pduInfoChunk, &len);
											lenToSend -= len;
										}else{
											PduR_SoAdTpCopyRxData(SocketAdminList[sockNr].SocketRouteRef->DestinationPduId, &pduInfoChunk, &lenToSend);
											lenToSend = 0;
										}
									}
								}else{
									PduR_SoAdTpCopyRxData(SocketAdminList[sockNr].SocketRouteRef->DestinationPduId, &pduInfo, &len);
								}
								/* Finished reception */
								(void)PduR_SoAdTpRxIndication(SocketAdminList[sockNr].SocketRouteRef->DestinationPduId, NTFRSLT_OK);
							} else if (result != BUFREQ_BUSY) {
								DET_REPORTERROR(MODULE_ID_SOAD, 0, SOAD_SOCKET_UDP_READ_ID, SOAD_E_UL_RXBUFF);
								(void)PduR_SoAdTpRxIndication(SocketAdminList[sockNr].SocketRouteRef->DestinationPduId, NTFRSLT_E_NO_BUFFER);
							}
						}
					}
				}

				SoAd_BufferFree(pduInfo.SduDataPtr);
			} else {
				// No buffer available: continue poll() Report error SOAD_E_UPPERBUFF to DEM
				DET_REPORTERROR(MODULE_ID_SOAD, 0, SOAD_SOCKET_UDP_READ_ID, SOAD_E_UPPERBUFF);
				(void)PduR_SoAdTpRxIndication(SocketAdminList[sockNr].SocketRouteRef->DestinationPduId, NTFRSLT_E_NO_BUFFER);
			}
		} else {
			DET_REPORTERROR(MODULE_ID_SOAD, 0, SOAD_SOCKET_UDP_READ_ID, SOAD_E_CONFIG_INVALID);
		}
		break;	// SOAD_AUTOSAR_CONNECTOR_PDUR
#ifdef USE_DOIP
	case SOAD_AUTOSAR_CONNECTOR_DOIP:
		ASLOG(SOAD, ("!!!soad:%s begin, DoIP_HandleUdpRx\n",__FUNCTION__));
		DoIp_HandleUdpRx(sockNr);
		break;	// SOAD_AUTOSAR_CONNECTOR_DOIP
#endif
	default:
		DET_REPORTERROR(MODULE_ID_SOAD, 0, SOAD_SOCKET_UDP_READ_ID, SOAD_E_NOCONNECTOR);
		break;
	}
#endif /* USE_PDUR */
}


static void scanSockets(void)
{
	uint16 i;

	for (i = 0; i < SOAD_SOCKET_COUNT; i++) {
		switch (SocketAdminList[i].SocketState) {
		case SOCKET_INIT:
			ASLOG(SOAD, ("!!!soad:%s socket %d create\n",__FUNCTION__, i));
			socketCreate(i);
			break;

		case SOCKET_TCP_LISTENING:
			ASLOG(SOAD, ("!!!soad:%s tcp listening\n",__FUNCTION__));
			socketAccept(i);
			break;

		case SOCKET_TCP_READY:
			ASLOG(SOAD, ("!!!soad:%s tcp id:%d read\n",__FUNCTION__, i));
			socketTcpRead(i);
			break;

		case SOCKET_UDP_READY:
			ASLOG(SOAD, ("!!!soad:%s udp id:%d read\n",__FUNCTION__, i));
			socketUdpRead(i);
			break;

		case SOCKET_DUPLICATE:
			/* Do nothing */
			break;

		default:
			/* This should never happen! */
			DET_REPORTERROR(MODULE_ID_SOAD, 0, SOAD_SCAN_SOCKETS_ID, SOAD_E_SHALL_NOT_HAPPEN);
			break;
		} /* End of switch */
	} /* End of for */


}


static void handleTx()
{

}

/** @req SOAD193 */
void SoAd_TcpIp_Init(void)
{
	ASLOG(SOAD,("!!!Tcpip init\n"));
#if defined(USE_SHELL) && !defined(USE_SHELL_SYMTAB)
	SHELL_AddCmd(&cmdIfconfig);
#endif

	#ifdef USE_LWIP
	LwIP_Init();
	#endif
	#ifdef USE_UIP
	/* contiki uip socket is started up by EcuM */
	#endif

#ifdef USE_SD
	Sd_Init(NULL);
#endif

}


/** @req SOAD093 */
void SoAd_Init(void)
{
	ASLOG(SOAD, ("!!!soad init SOAD_SOCKET_COUNT=%d\n",SOAD_SOCKET_COUNT));
	sint16 i, j;

	// Initiate the socket administration list
	for (i = 0; i < SOAD_SOCKET_COUNT; i++) {
		SocketAdminList[i].SocketNr = i;
		SocketAdminList[i].SocketState = SOCKET_INIT;
		SocketAdminList[i].SocketConnectionRef = &SoAd_Config.SocketConnection[i];
		if(NULL != SoAd_Config.SocketConnection[i].SocketRemoteIpAddress) {
			SocketAdminList[i].RemoteIpAddress = inet_addr(SoAd_Config.SocketConnection[i].SocketRemoteIpAddress);
		}
		SocketAdminList[i].RemotePort = htons(SoAd_Config.SocketConnection[i].SocketRemotePort);
		SocketAdminList[i].SocketHandle = -1;
		SocketAdminList[i].ConnectionHandle = -1;

		SocketAdminList[i].SocketProtocolIsTcp = FALSE;
		if (SoAd_Config.SocketConnection[i].SocketProtocol == SOAD_SOCKET_PROT_TCP) {
			SocketAdminList[i].SocketProtocolIsTcp = TRUE;
		} else if (SoAd_Config.SocketConnection[i].SocketProtocol != SOAD_SOCKET_PROT_UDP) {
			// Configuration error!!!
			DET_REPORTERROR(MODULE_ID_SOAD, 0, SOAD_INIT_ID, SOAD_E_CONFIG_INVALID);
		}

		// Check if several connections are expected on this port.
		for (j = 0; j < i; j++) {
			if ((SoAd_Config.SocketConnection[i].SocketProtocol == SoAd_Config.SocketConnection[j].SocketProtocol)
				&& (SoAd_Config.SocketConnection[i].SocketLocalPort == SoAd_Config.SocketConnection[j].SocketLocalPort)) {
				SocketAdminList[i].SocketState = SOCKET_DUPLICATE;
				break;
			}
		}
	}

	// Cross reference from SocketAdminList to SocketRoute
	for (i = 0; i < SOAD_SOCKET_ROUTE_COUNT; i++) {
		if (SoAd_Config.SocketRoute[i].SourceSocketRef != NULL) {
			if (SoAd_Config.SocketRoute[i].SourceSocketRef->SocketId < SOAD_SOCKET_COUNT) {
				SocketAdminList[SoAd_Config.SocketRoute[i].SourceSocketRef->SocketId].SocketRouteRef = &SoAd_Config.SocketRoute[i];
			} else {
				DET_REPORTERROR(MODULE_ID_SOAD, 0, SOAD_INIT_ID, SOAD_E_CONFIG_INVALID);
			}
		} else {
			DET_REPORTERROR(MODULE_ID_SOAD, 0, SOAD_INIT_ID, SOAD_E_CONFIG_INVALID);
		}
	}
	
	// Initialize PduStatus of PduAdminList
	for (i = 0; i < SOAD_PDU_ROUTE_COUNT; i++) {
		PduAdminList[i].PduStatus = PDU_IDLE;
	}

#ifdef USE_DOIP
	ASLOG(SOAD, ("!!!soad:%s DoIp init\n",__FUNCTION__));
	DoIp_Init();
#endif
#ifdef USE_TCPIP

#else
	SoAd_TcpIp_Init();
#endif

	ModuleStatus = SOAD_INITIALIZED;
}


/** @req SOAD092 */
Std_ReturnType SoAd_Shutdown(void)
{
	return E_OK;
}


/** @req SOAD121 */
void SoAd_MainFunction(void)
{
	if (ModuleStatus != SOAD_UNINITIALIZED) {
		scanSockets();
		handleTx();
	}
}

/** @req SOAD209 */
void SoAd_Cbk_LocalIpAssignmentChg( uint8 Index, boolean Valid, SoAd_SockAddrType Address ) {
	// NOTE: use Index (and make sure it's valid)
	if (TRUE == Valid) {
		// Link is up.
		// Start timer so that we can send out vehicle announcements.
		LinkStatus = SOAD_ARC_LINKUP;
#ifdef USE_DOIP
		DoIp_LocalIpAddrAssignmentChg((SoAd_SoConIdType)Index, TCPIP_IPADDR_STATE_ASSIGNED);

#endif

	} else {
		// Stop timer so that we can stop sending out vehicle announcements.
		LinkStatus = SOAD_ARC_LINKDOWN;

#ifdef USE_DOIP
		DoIp_LocalIpAddrAssignmentChg((SoAd_SoConIdType)Index, TCPIP_IPADDR_STATE_ASSIGNED);
#endif

	}
}

/** @req SOAD127 */
void SoAd_SocketReset(void)
{

}


/** @req SOAD091 */
Std_ReturnType SoAdIf_Transmit(PduIdType SoAdSrcPduId, const PduInfoType* SoAdSrcPduInfoPtr)
{
#ifdef USE_PDUR
	Std_ReturnType returnCode = E_OK;
	uint16 socketNr;

	VALIDATE_RV(ModuleStatus != SOAD_UNINITIALIZED, SOAD_IF_TRANSMIT_ID, SOAD_E_NOTINIT, E_NOT_OK);	/** @req SOAD213 *//** @req SOAD217 */
	VALIDATE_RV(SoAdSrcPduInfoPtr != NULL, SOAD_IF_TRANSMIT_ID, SOAD_E_NULL_PTR, E_NOT_OK);	/** @req SOAD235 */
	VALIDATE_RV(SoAdSrcPduId < SOAD_PDU_ROUTE_COUNT, SOAD_IF_TRANSMIT_ID, SOAD_E_INVAL, E_NOT_OK);	/** @req SOAD214 */


	if (SoAd_Config.PduRoute[SoAdSrcPduId].SourceSduLength <= SoAdSrcPduInfoPtr->SduLength) {
		if (PduAdminList[SoAdSrcPduId].PduStatus == PDU_IDLE ) {
			if (SoAd_Config.PduRoute[SoAdSrcPduId].DestinationSocketRef != NULL ) {
				socketNr = SoAd_Config.PduRoute[SoAdSrcPduId].DestinationSocketRef->SocketId;
				if ((SocketAdminList[socketNr].SocketState == SOCKET_TCP_LISTENING)
					|| (SocketAdminList[socketNr].SocketState == SOCKET_TCP_READY)
					|| (SocketAdminList[socketNr].SocketState == SOCKET_UDP_READY)) {
						PduAdminList[SoAdSrcPduId].PduStatus = PDU_IF_SENDING;
						SoAd_SendIpMessage(socketNr, SoAdSrcPduInfoPtr->SduDataPtr, SoAdSrcPduInfoPtr->SduLength);
						switch (SoAd_Config.PduRoute[SoAdSrcPduId].UserTxConfirmationUL)
						{
#if defined(USE_UDPNM)
						case SOAD_UL_UDPNM:
							UdpNm_SoAdIfTxConfirmation(SoAd_Config.PduRoute[SoAdSrcPduId].SourcePduId);
							break;
#endif
						case SOAD_UL_SD:
							break;
						default:
							PduR_SoAdIfTxConfirmation(SoAd_Config.PduRoute[SoAdSrcPduId].SourcePduId);
							break;
						}
						PduAdminList[SoAdSrcPduId].PduStatus = PDU_IDLE;
				} else {
					/* Socket not ready */
					returnCode = E_NOT_OK;
				}
			} else {
				/* SocketAdminRef is NULL */
				returnCode = E_NOT_OK;
			}
		} else {
			/* A PDU is already in progress */
			returnCode = E_NOT_OK;
		}
	} else {
		/* Attached length and length from PduRoutingTable does not match */
		returnCode = E_NOT_OK;
	}

	return returnCode;
#else
	return E_NOT_OK;
#endif /* USE_PDUR */
}


/** @req SOAD105 */
Std_ReturnType SoAdTp_Transmit(PduIdType SoAdSrcPduId, const PduInfoType* SoAdSrcPduInfoPtr)
{
	Std_ReturnType returnCode = E_OK;
#ifdef USE_PDUR
	PduInfoType txPduInfo;
	uint16 socketNr;

	VALIDATE_RV(ModuleStatus != SOAD_UNINITIALIZED, SOAD_TP_TRANSMIT_ID, SOAD_E_NOTINIT, E_NOT_OK);	/** @req SOAD224 *//** @req SOAD223 */
	VALIDATE_RV(SoAdSrcPduInfoPtr != NULL, SOAD_TP_TRANSMIT_ID, SOAD_E_NULL_PTR, E_NOT_OK);	/** @req SOAD225 */
	VALIDATE_RV(SoAdSrcPduId < SOAD_PDU_ROUTE_COUNT, SOAD_TP_TRANSMIT_ID, SOAD_E_INVAL, E_NOT_OK);	/** @req SOAD237 */

	if (SoAd_Config.PduRoute[SoAdSrcPduId].DestinationSocketRef != NULL ) {
		switch (SoAd_Config.PduRoute[SoAdSrcPduId].DestinationSocketRef->AutosarConnectorType) {
		case SOAD_AUTOSAR_CONNECTOR_PDUR:
			if (SoAd_Config.PduRoute[SoAdSrcPduId].SourceSduLength == SoAdSrcPduInfoPtr->SduLength) {	// NOTE: Shall this test be done or all length be allowed?
				if (PduAdminList[SoAdSrcPduId].PduStatus == PDU_IDLE ) {
					socketNr = SoAd_Config.PduRoute[SoAdSrcPduId].DestinationSocketRef->SocketId;
					if ((SocketAdminList[socketNr].SocketState == SOCKET_TCP_LISTENING)
						|| (SocketAdminList[socketNr].SocketState == SOCKET_TCP_READY)
						|| (SocketAdminList[socketNr].SocketState == SOCKET_UDP_READY)) {
							PduAdminList[SoAdSrcPduId].PduStatus = PDU_TP_REQ_BUFFER;

							txPduInfo.SduLength = SoAdSrcPduInfoPtr->SduLength;

							if(SoAd_BufferGet(txPduInfo.SduLength, &txPduInfo.SduDataPtr))
							{
								PduLengthType availableData;
								PduR_SoAdTpCopyTxData(SoAd_Config.PduRoute[SoAdSrcPduId].SourcePduId, &txPduInfo, /* retry */NULL, &availableData );
							}else{
								// No free buffer. Inform PduR..
								PduR_SoAdTpTxConfirmation(SoAd_Config.PduRoute[SoAdSrcPduId].SourcePduId, NTFRSLT_E_NO_BUFFER);
								PduAdminList[SoAdSrcPduId].PduStatus = PDU_TP_SENDING;
								SoAd_SendIpMessage(socketNr, txPduInfo.SduDataPtr, txPduInfo.SduLength);
								PduR_SoAdTpTxConfirmation(SoAd_Config.PduRoute[SoAdSrcPduId].SourcePduId, NTFRSLT_OK);
							}
							SoAd_BufferFree(txPduInfo.SduDataPtr);
							PduAdminList[SoAdSrcPduId].PduStatus = PDU_IDLE;
					} else {
						/* Socket not ready */
						returnCode = E_NOT_OK;
					}
				} else {
					/* A PDU is already in progress */
					returnCode = E_NOT_OK;
				}
			} else {
				/* Attached length and length from PduRoutingTable does not match */
				returnCode = E_NOT_OK;
			}
			break;
#ifdef USE_DOIP
		case SOAD_AUTOSAR_CONNECTOR_DOIP:
			ASLOG(SOAD, ("!!!soad:%s begin, DoIP_HandleTpTransmit\n",__FUNCTION__));
			returnCode = DoIp_HandleTpTransmit(SoAdSrcPduId, SoAdSrcPduInfoPtr);
			break;
#endif
		default:
			// Not supported connector type
			returnCode = E_NOT_OK;
			break;
		} /* End of switch */

	} else {
		/* SocketAdminRef is NULL */
		returnCode = E_NOT_OK;
	}
#endif /* USE_PDUR */
	return returnCode;
}


/** @req SOAD194 */
void TcpIp_Shutdown(void)
{

}


/** @req SOAD143 */
void TcpIp_MainFunctionCyclic(void)
{

}

Std_ReturnType TcpIp_SetDhcpHostNameOption(uint8* HostNameOption, uint8 HostNameLen)
{
	return E_OK;
}

#ifdef USE_SD
Std_ReturnType SoAd_OpenSoCon( SoAd_SoConIdType SoConId )
{
	Std_ReturnType ercd = E_NOT_OK;
	return ercd;
}

Std_ReturnType SoAd_CloseSoCon( SoAd_SoConIdType SoConId, boolean abort )
{
	Std_ReturnType ercd = E_NOT_OK;
	return ercd;
}

Std_ReturnType SoAd_SetRemoteAddr( SoAd_SoConIdType SoConId, const TcpIp_SockAddrType* RemoteAddrPtr )
{
	Std_ReturnType ercd = E_NOT_OK;

	if (SoConId < SOAD_PDU_ROUTE_COUNT)
	{
		SocketAdminList[SoConId].RemotePort = RemoteAddrPtr->port;
		memcpy(&SocketAdminList[SoConId].RemoteIpAddress, RemoteAddrPtr->addr, sizeof(SocketAdminList[SoConId].RemoteIpAddress));
		char addr[32];
		inet_aton(addr,RemoteAddrPtr->addr);
		ercd = E_OK;
	}

	return ercd;
}

Std_ReturnType SoAd_IfTransmit( PduIdType SoAdSrcPduId, const PduInfoType* SoAdSrcPduInfoPtr )
{
	ASLOG(SOAD, ("!!!soad:SoAd_IfTransmit begin\n"));
	return SoAdIf_Transmit(SoAdSrcPduId, SoAdSrcPduInfoPtr);
}

Std_ReturnType SoAd_GetRemoteAddr( SoAd_SoConIdType SoConId, TcpIp_SockAddrType* IpAddrPtr )
{
	Std_ReturnType ercd = E_NOT_OK;

	if (SoConId < SOAD_PDU_ROUTE_COUNT)
	{
		IpAddrPtr->port = SocketAdminList[SoConId].RemotePort;
		memcpy(IpAddrPtr->addr, &SocketAdminList[SoConId].RemoteIpAddress, sizeof(SocketAdminList[SoConId].RemoteIpAddress));
		ercd = E_OK;
	}

	return ercd;
}

Std_ReturnType SoAd_GetSoConId( PduIdType TxPduId, SoAd_SoConIdType* SoConIdPtr )
{
	Std_ReturnType ercd = E_NOT_OK;
	*SoConIdPtr = TxPduId;
	return ercd;
}
Std_ReturnType SoAd_RequestIpAddrAssignment( SoAd_SoConIdType SoConId,
		TcpIp_IpAddrAssignmentType Type, const TcpIp_SockAddrType* LocalIpAddrPtr,
		uint8 Netmask, const TcpIp_SockAddrType* DefaultRouterPtr )
{
	Std_ReturnType ercd = E_NOT_OK;
	return ercd;
}

Std_ReturnType SoAd_GetLocalAddr( SoAd_SoConIdType SoConId, TcpIp_SockAddrType* LocalAddrPtr,
		uint8* NetmaskPtr, TcpIp_SockAddrType* DefaultRouterPtr )
{
	Std_ReturnType ercd = E_NOT_OK;
	uint32 localIpAddress;
	if (SoConId < SOAD_PDU_ROUTE_COUNT)
	{
		if(NULL != SocketAdminList[SoConId].SocketConnectionRef->SocketLocalIpAddress)
		{
			localIpAddress = inet_addr(SocketAdminList[SoConId].SocketConnectionRef->SocketLocalIpAddress);
		}
		else
		{
			localIpAddress = SoAd_GetLocalIp();
		}
		LocalAddrPtr->port = SocketAdminList[SoConId].SocketConnectionRef->SocketLocalPort;
		memcpy(LocalAddrPtr->addr, &localIpAddress, sizeof(localIpAddress));
		ercd = E_OK;
	}
	return ercd;
}

Std_ReturnType SoAd_EnableRouting( SoAd_RoutingGroupIdType id )
{
	Std_ReturnType ercd = E_NOT_OK;
	return ercd;
}
Std_ReturnType SoAd_EnableSpecificRouting( SoAd_RoutingGroupIdType id, SoAd_SoConIdType SoConId )
{
	Std_ReturnType ercd = E_NOT_OK;
	return ercd;
}

Std_ReturnType SoAd_DisableRouting( SoAd_RoutingGroupIdType id )
{
	Std_ReturnType ercd = E_NOT_OK;
	return ercd;
}

Std_ReturnType SoAd_DisableSpecificRouting( SoAd_RoutingGroupIdType id, SoAd_SoConIdType SoConId )
{
	Std_ReturnType ercd = E_NOT_OK;
	return ercd;
}

Std_ReturnType SoAd_Arc_GetSoConIdFromRxPdu( PduIdType RxPduId, SoAd_SoConIdType* SoConIdPtr )
{
	Std_ReturnType ercd = E_NOT_OK;
	return ercd;
}
#endif
#endif /* USE_SOAD */

