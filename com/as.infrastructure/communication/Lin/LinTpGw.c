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
#include "LinIf.h"
#include "asdebug.h"
/* ============================ [ MACROS    ] ====================================================== */
#define AS_LOG_LINTPGW 1

#define LINTPGW_INSTANCE_NUM LinTpGw_Config.instanceNum
#define LINTPGW_RTE (LinTpGw_Config.rte[Instance])
#define LINTPGW_RXSDU_SIZE (LinTpGw_Config.instanceConfig[Instance].rxPduInfo->SduLength)
#define LINTPGW_RXSDU (LinTpGw_Config.instanceConfig[Instance].rxPduInfo)
#define LINTPGW_TXSDU_SIZE (LinTpGw_Config.instanceConfig[Instance].txPduInfo->SduLength)
#define LINTPGW_TXSDU (LinTpGw_Config.instanceConfig[Instance].txPduInfo)
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
BufReq_ReturnType LinTpGw_ProvideRxBuffer(PduIdType Instance, PduLengthType tpSduLength, PduInfoType **pduInfoPtr)
{
	BufReq_ReturnType ret = BUFREQ_OK;

	asAssert(Instance < LINTPGW_INSTANCE_NUM);

	if(LINTPGW_BUFFER_IDLE != LINTPGW_RTE.rxPduState)
	{
		ret = BUFREQ_E_BUSY;
	}
	else if(NULL == pduInfoPtr)
	{
		ret = BUFREQ_E_NOT_OK;
	}
	else if((tpSduLength > LINTPGW_RXSDU_SIZE) || (0 == tpSduLength))
	{
		ret = BUFREQ_E_OVFL;
	}
	else
	{
		*pduInfoPtr = (PduInfoType *)LINTPGW_RXSDU;
		LINTPGW_RTE.rxPduLength = tpSduLength;
		LINTPGW_RTE.rxPduState = LINTPGW_BUFFER_PROVIDED;
	}

	return ret;
}
BufReq_ReturnType LinTpGw_ProvideTxBuffer(PduIdType Instance, PduInfoType **pduInfoPtr, PduLengthType length)
{
	BufReq_ReturnType ret = BUFREQ_OK;

	asAssert(Instance < LINTPGW_INSTANCE_NUM);

	if(LINTPGW_BUFFER_FULL != LINTPGW_RTE.txPduState)
	{
		ret = BUFREQ_E_BUSY;
	}
	else if(NULL == pduInfoPtr)
	{
		ret = BUFREQ_E_NOT_OK;
	}
	else if(length > LINTPGW_TXSDU_SIZE)
	{
		ret = BUFREQ_E_OVFL;
	}
	else
	{
		*pduInfoPtr = (PduInfoType *)LINTPGW_TXSDU;
		LINTPGW_RTE.txPduState = LINTPGW_BUFFER_PROVIDED;
	}
	return ret;
}

void LinTpGw_RxIndication(PduIdType Instance, NotifResultType result)
{
	asAssert(Instance < LINTPGW_INSTANCE_NUM);
	PduInfoType pduInfo;
	if(result == NTFRSLT_OK)
	{
		LINTPGW_RTE.rxPduState = LINTPGW_BUFFER_FULL;
		LINTPGW_RTE.index = 0;
		pduInfo.SduDataPtr = LINTPGW_RXSDU->SduDataPtr;
		pduInfo.SduLength = LINTPGW_RTE.rxPduLength;
		LinTp_Transmit(Instance, &pduInfo);
	}
	else
	{
		LINTPGW_RTE.rxPduState = LINTPGW_BUFFER_IDLE;
	}
}

void LinTpGw_TxConfirmation(PduIdType Instance, NotifResultType result)
{
	asAssert(Instance < LINTPGW_INSTANCE_NUM);

	ASLOG(LINTPGW, ("TxConfirmation(%d,%d)\n", Instance, result));
	if(result == NTFRSLT_OK)
	{
		if(0 == LinTp_TransmitLeft(Instance)) {
			LinIf_ScheduleRequest(Instance, 0);
		}
	}
	else
	{

	}

	LINTPGW_RTE.txPduState = LINTPGW_BUFFER_IDLE;
}

Std_ReturnType LinTpGw_TriggerTransmit(PduIdType Instance, PduInfoType *PduInfoPtr)
{
	Std_ReturnType ret = E_NOT_OK;
	asAssert(Instance < LINTPGW_INSTANCE_NUM);

	if(LINTPGW_BUFFER_FULL == LINTPGW_RTE.rxPduState) {
		ret = LinTp_TriggerTransmit(Instance, PduInfoPtr);
	}

	return ret;
}
