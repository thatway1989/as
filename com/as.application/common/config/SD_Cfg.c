/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2018  AS <parai@foxmail.com>
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
#include "SD.h"
#include "SD_Internal.h"
/* ============================ [ MACROS    ] ====================================================== */
#define DEFAULT_TTL 0xFFFFFF /* until next reboot */
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
static const Sd_ClientTimerType Sd_ClientTimer[1] =
{
	{
		.InitialFindDelayMax_ms = 100,
		.InitialFindDelayMin_ms = 10,
		.InitialFindRepetitionBaseDelay_ms = 200,
		.InitialFindRepetitionsMax = 3,
		.RequestResponseMaxDelay_ms = 1500,
		.RequestResponseMinDelay_ms = 0,
		.TTL = DEFAULT_TTL
	},
};
static const Sd_ServerTimerType  Sd_ServerTimer[1] =
{
	{
		.InitialOfferDelayMax_ms = 100,
		.InitialOfferDelayMin_ms = 10,
		.InitialOfferRepetitionBaseDelay_ms = 200,
		.InitialOfferRepetitionsMax = 3,
		.OfferCyclicDelay_ms = 2000,
		.RequestResponseMaxDelay_ms = 1500,
		.RequestResponseMinDelay_ms = 0,
		.TTL = DEFAULT_TTL
	},
};
static Sd_DynConsumedEventGroupType Sd_DynConsumedEventGroup0[1];
static Sd_DynConsumedEventGroupType Sd_DynConsumedEventGroup1[1];

static const Sd_ConsumedEventGroupType Sd_ConsumedEventGroup0[1] =
{
	{
		.AutoRequire = TRUE,
		.HandleId = 0,
		.Id = 0x4465,
		.MulticastActivationRef = 0,
		.MulticastGroupRef = NULL,
		.NoOfMulticastGroups = 0,
		.TimerRef = &Sd_ClientTimer[0],
		.UdpActivationRef = 0,
		.CapabilityRecord = NULL,
		.SdNoOfCapabiltyRecord = 0
	},
};

static const uint16 Sd_UdpSocketConnectionGroupSocketConnectionIds[] =
{
	1, SOADIF_ID_SD_UNICAST_RX,
};

static const Sd_ClientServiceType Sd_ClientServiceCfg[] =
{
	{
		.AutoRequire = TRUE,
		.HandleId = SD_CLIENT_SERVICE_SAMPLE1,
		.Id = 0x1234,
		.InstanceId = 0x5678,
		.MajorVersion = 0,
		.MinorVersion = 0,
		.TcpSocketConnectionGroupId = SOCKET_CONNECTION_GROUP_NOT_SET,
		.TcpSocketConnectionGroupSocketConnectionIdsPtr = NULL,
		.TimerRef = Sd_ClientTimer,
		.UdpSocketConnectionGroupId = SOCKET_CONNECTION_GROUP_NOT_SET,
		.UdpSocketConnectionGroupSocketConnectionIdsPtr = Sd_UdpSocketConnectionGroupSocketConnectionIds,
		.CapabilityRecord = NULL,
		.SdNoOfCapabiltyRecord = 0,
		.ConsumedEventGroup = Sd_ConsumedEventGroup0,
		.NoOfConsumedEventGroups = ARRAY_SIZE(Sd_DynConsumedEventGroup0),
		.ConsumedMethods.ClientServiceActivationRef = 0x5678
	},
	{
		.AutoRequire = TRUE,
		.HandleId = SD_CLIENT_SERVICE_SAMPLE2,
		.Id = 0x1235,
		.InstanceId = 0x5678,
		.MajorVersion = 0,
		.MinorVersion = 0,
		.TcpSocketConnectionGroupId = SOCKET_CONNECTION_GROUP_NOT_SET,
		.TcpSocketConnectionGroupSocketConnectionIdsPtr = NULL,
		.TimerRef = Sd_ClientTimer,
		.UdpSocketConnectionGroupId = SOCKET_CONNECTION_GROUP_NOT_SET,
		.UdpSocketConnectionGroupSocketConnectionIdsPtr = Sd_UdpSocketConnectionGroupSocketConnectionIds,
		.CapabilityRecord = NULL,
		.SdNoOfCapabiltyRecord = 0,
		.ConsumedEventGroup = NULL,
		.NoOfConsumedEventGroups = 0,
		.ConsumedMethods.ClientServiceActivationRef = 0x5678
	}
};

static const Sd_ServerServiceType Sd_ServerServiceCfg[] =
{
	{
		.AutoAvailable = TRUE,
		.HandleId = SD_SERVER_SERVICE_SAMPLE1,
		.Id = 0x1236,
		.InstanceId = 0x5678,
		.MajorVersion = 1,
		.MinorVersion = 0,
		.TcpSocketConnectionGroupId = SOCKET_CONNECTION_GROUP_NOT_SET,
		.TcpSocketConnectionGroupSocketConnectionIdsPtr = NULL,
		.TimerRef = Sd_ServerTimer,
		.UdpSocketConnectionGroupId = SOCKET_CONNECTION_GROUP_NOT_SET,
		.UdpSocketConnectionGroupSocketConnectionIdsPtr = Sd_UdpSocketConnectionGroupSocketConnectionIds,
		.CapabilityRecord = NULL,
		.SdNoOfCapabiltyRecord = 0,
		.EventHandler = NULL,
		.NoOfEventHandlers = 0,
		.ProvidedMethods.ServerServiceActivationRef = 0xDB
	},
};

static Sd_DynClientServiceType Sd_DynClientService[] =
{
	{
		.ClientServiceCfg = &Sd_ClientServiceCfg[0],
		.ConsumedEventGroups = Sd_DynConsumedEventGroup0,
	},
	{
		.ClientServiceCfg = &Sd_ClientServiceCfg[1],
		.ConsumedEventGroups = Sd_DynConsumedEventGroup1,
	}
};

static Sd_DynServerServiceType Sd_DynServerService[] =
{
	{
		.ServerServiceCfg = &Sd_ServerServiceCfg[0]
	}
};

static const Sd_InstanceType Sd_InstanceCfg =
{
	.HostName = "as",
	.SdNoOfClientServices = ARRAY_SIZE(Sd_ClientServiceCfg),
	.SdClientService = Sd_ClientServiceCfg,
	.SdClientTimer = Sd_ClientTimer,
#if defined(USE_DEM)
	.DemEventParameterRefs = NULL,
#endif
	.MulticastRxPduId = SD_RX_MULTICAST_PDUID,
	.MulticastRxPduSoConRef = SOCKET_CONNECTION_GROUP_NOT_SET,
	.TxPduId = SOADIF_ID_SD_MULTICAST_TX,
	.UnicastRxPduId = SD_RX_UNICAST_PDUID,
	.UnicastRxPduSoConRef = SOCKET_CONNECTION_GROUP_NOT_SET,
	.SdNoOfServerServices = ARRAY_SIZE(Sd_ServerServiceCfg),
	.SdServerService = Sd_ServerServiceCfg,
	.SdServerTimer = Sd_ServerTimer
};

static Sd_DynInstanceType Sd_DynInstance = {
	.InstanceCfg = &Sd_InstanceCfg,
	.SdClientService = Sd_DynClientService,
	.SdServerService = Sd_DynServerService,
	.TxSoCon = SOADIF_ID_SD_MULTICAST_TX,
	.MulticastRxSoCon = SOADIF_ID_SD_MULTICAST_RX,
	.UnicastRxSoCon = SOADIF_ID_SD_UNICAST_RX,
};

const Sd_ConfigType Sd_Config = {
	.Instance = &Sd_InstanceCfg
};

const Sd_DynConfigType Sd_DynConfig =
{
	.Instance = &Sd_DynInstance
};
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
void BswM_Sd_ClientServiceCurrentState(uint16 SdClientServiceHandleId,
			Sd_ClientServiceCurrentStateType CurrentClientState)
{

}

void BswM_Sd_ConsumedEventGroupCurrentState(uint16 SdConsumedEventGroupHandleId,
			Sd_ConsumedEventGroupCurrentStateType ConsumedEventGroupState)
{

}

void BswM_Sd_EventHandlerCurrentState(uint16 SdEventHandlerHandleId,
			Sd_EventHandlerCurrentStateType EventHandlerStatus)
{

}

