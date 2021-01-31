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
#define AS_LOG_LINTPGW 0
#define AS_LOG_LINTPGWE 1

#define LINTPGW_LOG_ERROR() ASLOG(LINTPGWE, ("%s(%d) error in state %d\n", __func__, Instance, LINTPGW_RTE.PduState))

#define LINTPGW_INSTANCE_NUM LinTpGw_Config.instanceNum
#define LINTPGW_RTE (LinTpGw_Config.rte[Instance])
#define LINTPGW_SDU_SIZE (LinTpGw_Config.instanceConfig[Instance].PduInfo->SduLength)
#define LINTPGW_SDU (LinTpGw_Config.instanceConfig[Instance].PduInfo)
#define LINTPGW_SCHTBL (LinTpGw_Config.instanceConfig[Instance].schTbl)

/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
BufReq_ReturnType LinTpGw_ProvideRxBuffer(PduIdType Instance, PduLengthType tpSduLength, PduInfoType **pduInfoPtr)
{ /* call from CanTp to receive the UDS message which should be forwarded to LinTp */
	BufReq_ReturnType ret = BUFREQ_OK;

	asAssert(Instance < LINTPGW_INSTANCE_NUM);

	if(LINTPGW_BUFFER_IDLE != LINTPGW_RTE.PduState)
	{
		ret = BUFREQ_E_BUSY;
	}
	else if(NULL == pduInfoPtr)
	{
		ret = BUFREQ_E_NOT_OK;
	}
	else if((tpSduLength > LINTPGW_SDU_SIZE) || (0 == tpSduLength))
	{
		ret = BUFREQ_E_OVFL;
	}
	else
	{
		*pduInfoPtr = (PduInfoType *)LINTPGW_SDU;
		LINTPGW_RTE.PduLength = tpSduLength;
		LINTPGW_RTE.PduState = LINTPGW_BUFFER_PROVIDED_TO_CANTP_RX;
	}

	return ret;
}
BufReq_ReturnType LinTpGw_ProvideTxBuffer(PduIdType Instance, PduInfoType **pduInfoPtr, PduLengthType length)
{
	BufReq_ReturnType ret = BUFREQ_OK;

	asAssert(Instance < LINTPGW_INSTANCE_NUM);

	if(LINTPGW_BUFFER_LINTP_FULL != LINTPGW_RTE.PduState)
	{
		ret = BUFREQ_E_BUSY;
	}
	else if(NULL == pduInfoPtr)
	{
		ret = BUFREQ_E_NOT_OK;
	}
	else if(length > LINTPGW_SDU_SIZE)
	{
		ret = BUFREQ_E_OVFL;
	}
	else
	{
		*pduInfoPtr = (PduInfoType *)LINTPGW_SDU;
		LINTPGW_RTE.PduState = LINTPGW_BUFFER_PROVIDED_TO_CANTP_TX;
	}
	return ret;
}

void LinTpGw_RxIndication(PduIdType Instance, NotifResultType result)
{
	asAssert(Instance < LINTPGW_INSTANCE_NUM);
	PduInfoType pduInfo;
	if(result == NTFRSLT_OK)
	{
		asAssert(LINTPGW_RTE.PduState == LINTPGW_BUFFER_PROVIDED_TO_CANTP_RX);
		LINTPGW_RTE.PduState = LINTPGW_BUFFER_CANTP_FULL;
		LINTPGW_RTE.index = 0;
		pduInfo.SduDataPtr = LINTPGW_SDU->SduDataPtr;
		pduInfo.SduLength = LINTPGW_RTE.PduLength;
		LinTp_Transmit(Instance, &pduInfo);
	}
	else
	{
		LINTPGW_LOG_ERROR();
		if(LINTPGW_RTE.PduState == LINTPGW_BUFFER_PROVIDED_TO_LINTP_RX) {
			LinIf_ScheduleRequest(Instance, LINTPGW_SCHTBL);
		}
		LINTPGW_RTE.PduState = LINTPGW_BUFFER_IDLE;
	}
}

void LinTpGw_LinTpRxIndication(PduIdType Instance, const PduInfoType *PduInfo)
{
	if(LINTPGW_RTE.PduState == LINTPGW_BUFFER_PROVIDED_TO_LINTP_RX) {
		LinTp_RxIndication(Instance, PduInfo);
	} else {
		LINTPGW_LOG_ERROR();
	}
}

void LinTpGw_TxConfirmation(PduIdType Instance, NotifResultType result)
{
	asAssert(Instance < LINTPGW_INSTANCE_NUM);

	if(result == NTFRSLT_OK) {
		if(LINTPGW_BUFFER_CANTP_FULL == LINTPGW_RTE.PduState) {
			if(0 == LinTp_TransmitLeft(Instance)) {
				LINTPGW_RTE.PduState = LINTPGW_BUFFER_PROVIDED_TO_LINTP_RX;
				LinTp_StartReception(Instance, LINTPGW_SDU);
			}
		} else {
			LINTPGW_LOG_ERROR();
		}
	} else {
		LINTPGW_LOG_ERROR();
		LinIf_ScheduleRequest(Instance, LINTPGW_SCHTBL);
	}
}

Std_ReturnType LinTpGw_TriggerTransmit(PduIdType Instance, PduInfoType *PduInfoPtr)
{
	Std_ReturnType ret = E_NOT_OK;
	asAssert(Instance < LINTPGW_INSTANCE_NUM);

	if(LINTPGW_BUFFER_CANTP_FULL == LINTPGW_RTE.PduState) {
		ret = LinTp_TriggerTransmit(Instance, PduInfoPtr);
	} else {
		LINTPGW_LOG_ERROR();
	}

	return ret;
}
