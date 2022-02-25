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
/* ============================ [ INCLUDES  ] ====================================================== */
#ifdef USE_SOAD
#include "SoAd.h"
#include "PduR.h"
#ifdef USE_SD
#include "SD.h"
#endif
/* ============================ [ MACROS    ] ====================================================== */
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
static const SoAd_SocketConnectionType SoAd_SocketConnection [SOAD_SOCKET_COUNT] =
{
	{	/* for DCM */
		.SocketId = 0,
		.SocketLocalIpAddress = "172.18.0.200",
		.SocketLocalPort = 13400,
		.SocketProtocol = SOAD_SOCKET_PROT_TCP,
		.AutosarConnectorType = SOAD_AUTOSAR_CONNECTOR_DOIP,
	},
#ifdef USE_SD
	{	/* for SD UNICAST */
		.SocketId = 1,
		.SocketLocalIpAddress = NULL,
		.SocketLocalPort = 30509,
		.SocketProtocol = SOAD_SOCKET_PROT_UDP,
		.AutosarConnectorType = SOAD_AUTOSAR_CONNECTOR_PDUR,
		.PduProvideBufferEnable = FALSE,
	},
	{	/* for SD MULTICAST */
		.SocketId = 2,
		.SocketLocalIpAddress = "224.244.224.245",
		.SocketLocalPort = 30490,
		.SocketProtocol = SOAD_SOCKET_PROT_UDP,
		.AutosarConnectorType = SOAD_AUTOSAR_CONNECTOR_PDUR,
		.PduProvideBufferEnable = FALSE,

		.SocketRemoteIpAddress = "224.244.224.245",
		.SocketRemotePort = 30490,
	},
#endif
};
static const DoIp_TargetAddressConfigType SoAd_DoIpTargetAddresses[DOIP_TARGET_COUNT]=
{
	{
		.addressValue=0xfeed,  /* this 16 bit ta */
		.txPdu=SOADTP_ID_SOAD_TX,
		.rxPdu=PDUR_ID_SOAD_RX
	},
};
static const DoIp_TesterConfigType SoAd_DoIpTesters[DOIP_TESTER_COUNT] =
{
	{
		.address = 0xbeef,
	}
};
static const DoIp_RoutingActivationConfigType SoAd_DoIpRoutingActivations[DOIP_ROUTINGACTIVATION_COUNT] =
{
	{
		.activationNumber = 0xda,
		.authenticationCallback = NULL,
		.confirmationCallback = NULL
	}
};
static const DoIp_RoutingActivationToTargetAddressMappingType SoAd_DoIpRoutingActivationToTargetAddressMap[DOIP_ROUTINGACTIVATION_TO_TARGET_RELATION_COUNT] =
{
	{
		.routingActivation = 0,
		.target = 0,
	}
};
static const SoAd_PduRouteType SoAd_PduRoute[SOAD_PDU_ROUTE_COUNT] =
{
	{	/* for DCM */
		.DestinationSocketRef = &SoAd_SocketConnection[0],
		.SourcePduId = PDUR_ID_SOAD_TX,
	},
	{
		.SourceSduLength = 16,
		.DestinationSocketRef = &SoAd_SocketConnection[1],
		.UserTxConfirmationUL = SOAD_UL_SD
	},
	{
		.SourceSduLength = 16,
		.DestinationSocketRef = &SoAd_SocketConnection[2],
		.UserTxConfirmationUL = SOAD_UL_SD
	},
};

static const SoAd_SocketRouteType SoAd_SocketRoute[SOAD_SOCKET_ROUTE_COUNT] =
{
	{
		.SourceSocketRef = &SoAd_SocketConnection[0],
		.SourceId = 0,
	},
#ifdef USE_SD
	{
		.SourceSocketRef = &SoAd_SocketConnection[1],
		.SourceId = 1,
		.DestinationPduId = SD_RX_UNICAST_PDUID,
		.DestinationSduLength = 16, /* the SOMEIP message header size is 16 */
		.UserRxIndicationUL = SOAD_UL_SD
	},
	{
		.SourceSocketRef = &SoAd_SocketConnection[2],
		.SourceId = 2,
		.DestinationPduId = SD_RX_MULTICAST_PDUID,
		.DestinationSduLength = 16, /* the SOMEIP message header size is 16 */
		.UserRxIndicationUL = SOAD_UL_SD
	},
#endif
};

const SoAd_ConfigType SoAd_Config =
{
	.SocketConnection = SoAd_SocketConnection,
	.SocketRoute = SoAd_SocketRoute,
	.DoIpTargetAddresses = SoAd_DoIpTargetAddresses,
	.DoIpTesters= SoAd_DoIpTesters,
	.DoIpRoutingActivations = SoAd_DoIpRoutingActivations,
	.DoIpRoutingActivationToTargetAddressMap = SoAd_DoIpRoutingActivationToTargetAddressMap,
	.PduRoute = SoAd_PduRoute
};
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
Std_ReturnType SoAd_DoIp_Arc_GetVin(uint8* buf, uint8 len)
{
	return E_OK;
}

Std_ReturnType SoAd_DoIp_Arc_GetEid(uint8* buf, uint8 len)
{
	return E_OK;
}

Std_ReturnType SoAd_DoIp_Arc_GetGid(uint8* buf, uint8 len)
{
	return E_OK;
}

Std_ReturnType SoAd_DoIp_Arc_GetFurtherActionRequired(uint8* buf)
{
	return E_OK;
}
#endif
