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
#ifndef _LINTP_H_
#define _LINTP_H_
/* ============================ [ INCLUDES  ] ====================================================== */
#include "ComStack_Types.h"
#include "LinTp_Cfg.h"
/* ============================ [ MACROS    ] ====================================================== */
/* ============================ [ TYPES     ] ====================================================== */
typedef struct {
	const PduInfoType* rxPduInfo;
	const PduInfoType* txPduInfo;
} LinTpGw_InstanceConfigType;

typedef enum {
	LINTPGW_BUFFER_IDLE = 0,
	LINTPGW_BUFFER_PROVIDED,
	LINTPGW_BUFFER_FULL,
} LinTpGw_BufferStateType;

typedef struct
{
	uint8 rxPduState;
	uint8 txPduState;
	uint32_t rxPduLength;
	uint32_t txPduLength;
	uint32_t index;
} LinTpGw_RuntimeType; /* RTE */

typedef struct {
	const LinTpGw_InstanceConfigType* instanceConfig;
	LinTpGw_RuntimeType* rte;
	uint32_t instanceNum;
} LinTpGw_ConfigType;

typedef enum {
	LINTP_APPLICATIVE_SCHEDULE,
	LINTP_DIAG_REQUEST,
	LINTP_DIAG_RESPONSE
} LinTp_Mode;

typedef struct {
	PduInfoType PduInfo;
	PduLengthType index;
} LinTp_ContextType;

typedef struct {
	NetworkHandleType network;
	LinTp_ContextType* context;
	#ifndef USE_BSWM
	LinIf_SchHandleType schTbl;
	#endif
} LinTp_TxPduConfigType;

typedef struct {
	NetworkHandleType network;
	LinTp_ContextType* context;
	#ifndef USE_BSWM
	LinIf_SchHandleType schTbl;
	#endif
} LinTp_RxPduConfigType;

typedef struct {
	const LinTp_TxPduConfigType* txPduConfigs;
	uint32_t txPduNum;
	const LinTp_RxPduConfigType* rxPduConfigs;
	uint32_t rxPduNum;
} LinTp_ConfigType;
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
extern const LinTpGw_ConfigType LinTpGw_Config;
extern const LinTp_ConfigType LinTp_Config;
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
/* Gateway to route UDS request from CANTP master to LINTP slave */
BufReq_ReturnType LinTpGw_ProvideRxBuffer(PduIdType Instance, PduLengthType tpSduLength, PduInfoType **pduInfoPtr);
BufReq_ReturnType LinTpGw_ProvideTxBuffer(PduIdType Instance, PduInfoType **pduInfoPtr, PduLengthType length);
void LinTpGw_RxIndication(PduIdType Instance, NotifResultType result);
void LinTpGw_TxConfirmation(PduIdType Instance, NotifResultType result);
Std_ReturnType LinTpGw_TriggerTransmit(PduIdType Instance, PduInfoType *PduInfoPtr);

void LinTp_Init(const LinTp_ConfigType* ConfigPtr);
Std_ReturnType LinTp_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);
Std_ReturnType LinTp_TriggerTransmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);
PduLengthType LinTp_TransmitLeft(PduIdType TxPduId);
#endif /* _LINTP_H_ */
