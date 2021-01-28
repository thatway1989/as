#ifndef COM_AS_INFRASTRUCTURE_INCLUDE_BSWM_H_
#define COM_AS_INFRASTRUCTURE_INCLUDE_BSWM_H_
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
#include "Std_Types.h"
#include "Os.h"
#include "Nm.h"
#include "CanSm.h"
#include "Dcm.h"
#include "EcuM.h"
#include "LinIf.h"
#include "Nvm.h"
#ifdef USE_SD
#include "SD.h"
#endif
/* ============================ [ MACROS    ] ====================================================== */

/* ============================ [ TYPES     ] ====================================================== */
typedef uint8 BswM_ModeType;
typedef uint8 BswM_ModeGroupType;
typedef uint8 BswM_UserType;
typedef void  BswM_ConfigType;
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
void BswM_BswMModeRequest(BswM_ModeGroupType requested_modeGroup,BswM_ModeType requested_mode);
void BswM_BswMModeSwitchNotification(BswM_ModeGroupType switched_modeGroup,BswM_ModeType switched_mode);
void BswM_BswMPartitionRestarted(void);
//void BswM_CanSM_CurrentIcomConfiguration(NetworkHandleType Network,IcomConfigIdType ActiveConfiguration,IcomSwitch_ErrorType Error);
//void BswM_CanSM_CurrentState(NetworkHandleType Network,CanSM_BswMRequestedModeType CurrentState);
void BswM_ComM_CurrentMode(NetworkHandleType Network,ComM_ModeType RequestedMode);
//void BswM_ComM_CurrentPNCMode(PNCHandleType PNC,ComM_PncModeType CurrentPncMode);
void BswM_ComM_InitiateReset(void);
void BswM_Dcm_ApplicationUpdated(void);
//void BswM_Dcm_CommunicationMode_CurrentState(NetworkHandleType Network,Dcm_CommunicationModeType RequestedMode);
void BswM_Deinit(void);
void BswM_EcuM_CurrentState(EcuM_StateType CurrentState);
void BswM_EcuM_CurrentWakeup(EcuM_WakeupSourceType source,EcuM_WakeupStatusType state);
void BswM_EcuM_RequestedState(EcuM_StateType State,EcuM_RunStatusType CurrentStatus);
//void BswM_EthSM_CurrentState(NetworkHandleType Network,EthSM_NetworkModeStateType CurrentState);
//void BswM_FrSM_CurrentState(NetworkHandleType Network,FrSM_BswM_StateType CurrentState);
void BswM_GetVersionInfo(Std_VersionInfoType* VersionInfo);
void BswM_Init(const BswM_ConfigType * ConfigPtr);
void BswM_J1939DcmBroadcastStatus(uint16 NetworkMask);
void BswM_J1939Nm_StateChangeNotification(NetworkHandleType Network,uint8 Node,Nm_StateType NmState);
void BswM_LinSM_CurrentSchedule(NetworkHandleType Network,LinIf_SchHandleType CurrentSchedule);
//void BswM_LinSM_CurrentState(NetworkHandleType Network,LinSM_ModeType CurrentState);
void BswM_LinTp_RequestMode(NetworkHandleType Network,LinTp_Mode LinTpRequestedMode);
void BswM_NmIf_CarWakeUpIndication(NetworkHandleType Network);
void BswM_NvM_CurrentBlockMode(NvM_BlockIdType Block,NvM_RequestResultType CurrentBlockMode);
void BswM_NvM_CurrentJobMode(uint8 ServiceId,NvM_RequestResultType CurrentJobMode);
void BswM_RequestMode(BswM_UserType requesting_user,BswM_ModeType requested_mode);
#ifdef USE_SD
//void BswM_Sd_ClientServiceCurrentState(uint16 SdClientServiceHandleId,Sd_ClientServiceCurrentStateType CurrentClientState);
//void BswM_Sd_ConsumedEventGroupCurrentState(uint16 SdConsumedEventGroupHandleId,Sd_ConsumedEventGroupCurrentStateTypeConsumedEventGroupState);
void BswM_Sd_EventHandlerCurrentState(uint16 SdEventHandlerHandleId,Sd_EventHandlerCurrentStateType EventHandlerStatus);
#endif
Std_ReturnType BswM_TriggerSlaveRTEStop(CoreIdType CoreID);
Std_ReturnType BswM_TriggerStartUpPhase2(CoreIdType CoreID);
#endif /* COM_AS_INFRASTRUCTURE_INCLUDE_BSWM_H_ */
