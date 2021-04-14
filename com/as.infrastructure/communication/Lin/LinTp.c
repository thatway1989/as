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
#ifdef USE_BSWM
#include "BswM.h"
#endif
#include "asdebug.h"
/* ============================ [ MACROS    ] ====================================================== */
#define AS_LOG_LINTP 0
#define AS_LOG_LINTPE 1

/* see ISO 15765-2 2004 */
#define N_PCI_MASK  0x30
#define N_PCI_SF    0x00
#define N_PCI_FF    0x10
#define N_PCI_CF    0x20
#define N_PCI_SF_DL 0x07

#define N_PCI_SN   0x0F
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
static LinTp_StatusType ReceiveSF(LinTp_ContextType* context, uint8* Data)
{
	uint8 length;
	uint8 *pData;

	if(context->index > 0) {
		ASLOG(LINTPE, ("SF received, abort previous reception\n"));
	}

	length = Data[0]&N_PCI_SF_DL;
	pData = &(Data[1]);

	memcpy(context->PduInfo.SduDataPtr, pData, length);

	context->index = length;

	return LINTP_RX_OK;
}

static LinTp_StatusType ReceiveFF(LinTp_ContextType* context, uint8* Data)
{
	uint8 length;
	uint8 *pData;
	LinTp_StatusType status = LINTP_RX_ERROR;

	if(context->index > 0) {
		ASLOG(LINTPE, ("FF received, abort previous reception\n"));
	}

	length = ((Data[0]&0x0F) << 8) + Data[1];
	if(length <= context->PduInfo.SduLength) {
		pData = &(Data[2]);
		memcpy(context->PduInfo.SduDataPtr, pData, 5);
		context->PduInfo.SduLength = length;
		context->index = 5;
		context->SN = 1;
		status = LINTP_RX_BUSY;
	} else {
		ASLOG(LINTPE, ("Buffer not enough expected %d, real %d\n", length, context->PduInfo.SduLength));
	}

	return status;
}

static LinTp_StatusType ReceiveCF(LinTp_ContextType* context, uint8* Data)
{
	uint8 length;
	uint8 *pData;
	LinTp_StatusType status = LINTP_RX_ERROR;

	if(context->index == 0) {
		ASLOG(LINTPE, ("CF received without FF\n"));
	}

	if(context->SN == (Data[0]&N_PCI_SN)) {
		context->SN ++ ;
		if(context->SN > 15) { context->SN = 0; }
		length = context->PduInfo.SduLength - context->index;
		if( length > 6 ) { length = 6; }
		if(length > 0) {
			pData = &(Data[1]);
			memcpy(&context->PduInfo.SduDataPtr[context->index], pData, length);
			context->index += length;
			if(context->index >= context->PduInfo.SduLength) {
				status = LINTP_RX_OK;
			} else {
				status = LINTP_RX_BUSY;
			}
		} else {
			ASLOG(LINTPE, ("too much CF received\n"));
		}
	}

	return status;
}
/* ============================ [ FUNCTIONS ] ====================================================== */
void LinTp_Init(const LinTp_ConfigType* ConfigPtr)
{
	(void*) ConfigPtr;
	asAssert(LinTp_Config.txPduNum == LinTp_Config.rxPduNum);
}

Std_ReturnType LinTp_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
	Std_ReturnType ercd = E_OK;
	NetworkHandleType network;
	LinTp_ContextType* context;

	if(TxPduId < LinTp_Config.txPduNum) {
		context = &LinTp_Config.context[TxPduId];
		network = LinTp_Config.txPduConfigs[TxPduId].network;
		#ifdef USE_BSWM
		BswM_LinTp_RequestMode(network, LINTP_DIAG_REQUEST);
		#else
		ercd = LinIf_ScheduleRequest(network, LinTp_Config.txPduConfigs[TxPduId].schTbl);
		#endif
		context->PduInfo = *PduInfoPtr;
		context->index = 0;
		context->SN = 1;
		context->timer = 0;
		ASLOG(LINTP, ("LinTp %d request transmit %d bytes\n", TxPduId, PduInfoPtr->SduLength));
	} else {
		ercd = E_NOT_OK;
	}

	return ercd;
}

Std_ReturnType LinTp_TriggerTransmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
	Std_ReturnType ercd = E_NOT_OK;
	PduLengthType doSize, pos=0;
	LinTp_ContextType* context;
	uint8_t* data;

	if(TxPduId < LinTp_Config.txPduNum) {
		data = PduInfoPtr->SduDataPtr;
		context = &LinTp_Config.context[TxPduId];
		if(context->index < context->PduInfo.SduLength) {
			doSize = context->PduInfo.SduLength - context->index;
			data[pos++] = LinTp_Config.txPduConfigs[TxPduId].NA;
			if(0 == context->index) {
				if(doSize <= 6) {
					data[pos++] = N_PCI_SF|doSize;
				} else {
					data[pos++] = N_PCI_FF|((doSize>>8)&0xF);
					data[pos++] = doSize&0xFF;
					doSize = 5;
				}
			} else {
				if(doSize > 6) {
					doSize = 6;
				}
				data[pos++] = N_PCI_CF|context->SN;
				context->SN ++;
				if(context->SN > 15) {
					context->SN = 0;
				}
			}
			memcpy(&data[pos], context->PduInfo.SduDataPtr+context->index, doSize);
			pos += doSize;
			while(pos < 8) {
				data[pos++] = 0x55;
			}
			context->index += doSize;
			ASLOG(LINTP, ("LinTp %d provide %d bytes, left %d bytes\n", TxPduId, doSize, context->PduInfo.SduLength - context->index));
			ercd = E_OK;
		}
	}
	return ercd;
}

PduLengthType LinTp_TransmitLeft(PduIdType TxPduId)
{
	PduLengthType left = 0;
	LinTp_ContextType* context;

	if(TxPduId < LinTp_Config.txPduNum) {
		context = &LinTp_Config.context[TxPduId];
		left = context->PduInfo.SduLength - context->index;
	}

	return left;
}

Std_ReturnType LinTp_StartReception(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
	Std_ReturnType ercd = E_OK;
	NetworkHandleType network;
	LinTp_ContextType* context;

	if(RxPduId < LinTp_Config.rxPduNum) {
		context = &LinTp_Config.context[RxPduId];
		network = LinTp_Config.rxPduConfigs[RxPduId].network;
		#ifdef USE_BSWM
		BswM_LinTp_RequestMode(network, LINTP_DIAG_RESPONSE);
		#else
		ercd = LinIf_ScheduleRequest(network, LinTp_Config.rxPduConfigs[RxPduId].schTbl);
		#endif
		context->PduInfo = *PduInfoPtr;
		context->index = 0;
		context->SN = 1;
		context->timer = LinTp_Config.rxPduConfigs[RxPduId].timeout;
		ASLOG(LINTP, ("LinTp %d start reception\n", RxPduId));
	} else {
		ercd = E_NOT_OK;
	}

	return ercd;
}

LinTp_StatusType LinTp_RxIndication(PduIdType RxPduId, PduInfoType *PduInfo)
{
	LinTp_StatusType status = LINTP_RX_ERROR;
	LinTp_ContextType* context;

	if(RxPduId < LinTp_Config.rxPduNum) {
		context = &LinTp_Config.context[RxPduId];
		if(PduInfo->SduDataPtr[0] != LinTp_Config.rxPduConfigs[RxPduId].NA) {
			ASLOG(LINTP, ("ignore as NA R %X != E %X\n", PduInfo->SduDataPtr[0], LinTp_Config.rxPduConfigs[RxPduId].NA));
		} else {
			switch(PduInfo->SduDataPtr[1]&N_PCI_MASK) {
				case N_PCI_SF:
					status = ReceiveSF(context, &PduInfo->SduDataPtr[1]);
					break;
				case N_PCI_FF:
					status = ReceiveFF(context, &PduInfo->SduDataPtr[1]);
					break;
				case N_PCI_CF:
					status = ReceiveCF(context, &PduInfo->SduDataPtr[1]);
					break;
				default:
					ASLOG(LINTP, ("invalid frame\n"));
					break;
			}
		}

		if(LINTP_RX_OK == status) {
			context->timer = 0;
			PduInfo->SduDataPtr = context->PduInfo.SduDataPtr;
			PduInfo->SduLength = context->index;
		} else if(LINTP_RX_BUSY == status) {
			context->timer = LinTp_Config.rxPduConfigs[RxPduId].timeout;
		} else {
			context->timer = 0;
		}
	}

	return status;
}

void LinTp_MainFunction(void) {

	int i;
	LinTp_ContextType* context;
	for(i=0; i<LinTp_Config.rxPduNum; i++) {
		context = &LinTp_Config.context[i];
		if(context->timer > 0) {
			context->timer --;
			if (context->timer == 0) {
				#ifdef USE_LINTPGW
				LinTpGw_RxIndication(i, E_NOT_OK);
				#else
				#error TODO
				#endif
			}
		}
	}
}

void LinTp_Shutdown(void)
{

}

