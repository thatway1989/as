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
#include "LinSTp.h"
#include "Dcm.h"
#include "asdebug.h"
/* ============================ [ MACROS    ] ====================================================== */
#define AS_LOG_LINSTP  0
#define AS_LOG_LINSTPE 2

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
static void ReceiveSF(PduIdType Instance, uint8* Data) {
	LinSTp_ContextType* context;
	PduInfoType* PduInfo = NULL;
	const LinSTp_ChannelConfigType* config;
	uint8 length;
	uint8 *pData;
	BufReq_ReturnType ret;

	context = &LinSTp_Config.contexts[Instance];
	config = &LinSTp_Config.channelConfigs[Instance];

	ASLOG(LINSTP, ("[%d]ReceiveSF\n",  Instance));
	if (context->state != LINSTP_IDLE) {
		ASLOG(LINSTPE, ("[%d]Received SF when in state %d.\n", Instance, context->state));
		Dcm_RxIndication(config->dcmRxId, NTFRSLT_E_NOT_OK);
		context->state = LINSTP_IDLE;
	}

	length = Data[0]&N_PCI_SF_DL;
	pData = &(Data[1]);
	ret = Dcm_ProvideRxBuffer(config->dcmRxId, length, &PduInfo);
	if( (BUFREQ_OK == ret) &&
		(NULL != PduInfo) &&
		(NULL != PduInfo->SduDataPtr) &&
		(length <= PduInfo->SduLength)) {
		memcpy(PduInfo->SduDataPtr, pData,length);
		context->state = LINSTP_RX_PROVIDED_TO_DCM;
		context->timer = 0;
		Dcm_RxIndication(config->dcmRxId, NTFRSLT_OK);
	} else {
		ASLOG(LINSTPE, ("[%d]SF provide RX buffer failed\n"));
	}
}

static void ReceiveFF(PduIdType Instance, uint8* Data) {
	PduLengthType length;
	uint8 *pData;
	BufReq_ReturnType ret;
	LinSTp_ContextType* context;
	PduInfoType* PduInfo = NULL;
	const LinSTp_ChannelConfigType* config;

	context = &LinSTp_Config.contexts[Instance];
	config = &LinSTp_Config.channelConfigs[Instance];

	ASLOG(LINSTP, ("[%d]ReceiveFF\n", Instance));
	if(context->state != LINSTP_IDLE) {
		ASLOG(LINSTPE, ("[%d]Received FF when in state %d.\n", Instance, context->state));
		Dcm_RxIndication(config->dcmRxId, NTFRSLT_E_NOT_OK);
		context->state = LINSTP_IDLE;
	}

	length = ((Data[0]&0x0F) << 8) + Data[1];
	pData = &(Data[2]);
	ret = Dcm_ProvideRxBuffer(config->dcmRxId, length, &PduInfo);
	if( (BUFREQ_OK == ret) &&
		(NULL != PduInfo) &&
		(NULL != PduInfo->SduDataPtr) &&
		(length <= PduInfo->SduLength)) {
		context->PduInfo.SduLength = length;
		context->PduInfo.SduDataPtr = PduInfo->SduDataPtr;
		memcpy(PduInfo->SduDataPtr, pData, 5);
		context->index = 5;
		context->SN = 1;
		context->state = LINSTP_RX_BUSY;
		context->timer =config->timeout;
	} else {
		ASLOG(LINSTPE, ("[%d]Received FF provide RX buffer failed\n", Instance));
	}
}

static void ReceiveCF(PduIdType Instance, uint8* Data) {
	PduLengthType length;
	uint8 *pData;
	BufReq_ReturnType ret;
	LinSTp_ContextType* context;
	PduInfoType* PduInfo = NULL;
	const LinSTp_ChannelConfigType* config;

	context = &LinSTp_Config.contexts[Instance];
	config = &LinSTp_Config.channelConfigs[Instance];

	ASLOG(LINSTP, ("[%d]ReceiveCF\n", Instance));
	if(context->state != LINSTP_RX_BUSY) {
		ASLOG(LINSTPE, ("[%d]Received CF when in state %d.\n", Instance, context->state));
	} else {
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
					context->state = LINSTP_RX_PROVIDED_TO_DCM;
					Dcm_RxIndication(config->dcmRxId, NTFRSLT_OK);
					context->timer = 0;
				} else {
					context->timer =config->timeout;
				}
			} else {
				ASLOG(LINSTPE, ("too much CF received\n"));
				context->state = LINSTP_IDLE;
				Dcm_RxIndication(config->dcmRxId, NTFRSLT_E_NOT_OK);
				context->timer = 0;
			}
		} else {
			context->state = LINSTP_IDLE;
			Dcm_RxIndication(config->dcmRxId, NTFRSLT_E_WRONG_SN);
			ASLOG(LINSTPE, ("[%d]Sequence Number Wrong, Abort Current Receiving.\n", Instance));
			context->timer = 0;
		}
	}
}
/* ============================ [ FUNCTIONS ] ====================================================== */
void LinSTp_RxIndication(PduIdType Instance, const PduInfoType *PduInfo) {
	if(Instance < LinSTp_Config.channelNums) {
		if(PduInfo->SduDataPtr[0] != LinSTp_Config.channelConfigs[Instance].NA) {
			ASLOG(LINSTP, ("%d: not for me\n", Instance));
		} else {
			switch(PduInfo->SduDataPtr[1]&N_PCI_MASK) {
			case N_PCI_SF:
				ReceiveSF(Instance, &PduInfo->SduDataPtr[1]);
				break;
			case N_PCI_FF:
				ReceiveFF(Instance, &PduInfo->SduDataPtr[1]);
				break;
			case N_PCI_CF:
				ReceiveCF(Instance, &PduInfo->SduDataPtr[1]);
				break;
			default:
				ASLOG(LINSTP, ("invalid frame\n"));
				break;
		}
		}
	} else {
		ASLOG(LINSTPE, ("invalid instance %d\n", Instance));
	}
}

Std_ReturnType LinSTp_Transmit(PduIdType Instance, const PduInfoType *PduInfo)
{
	Std_ReturnType ercd = E_OK;
	BufReq_ReturnType ret;
	PduInfoType* PduInfo2 = NULL;
	LinSTp_ContextType* context;
	const LinSTp_ChannelConfigType* config;

	if(Instance < LinSTp_Config.channelNums) {
		context = &LinSTp_Config.contexts[Instance];
		config = &LinSTp_Config.channelConfigs[Instance];
		if((LINSTP_RX_PROVIDED_TO_DCM == context->state) || (LINSTP_IDLE == context->state)) {
			ret = Dcm_ProvideTxBuffer(config->dcmTxId, &PduInfo2, PduInfo->SduLength);
			if( (BUFREQ_OK == ret) &&
				(NULL != PduInfo2) &&
				(NULL != PduInfo2->SduDataPtr) &&
				(PduInfo->SduLength <= PduInfo2->SduLength)) {
				context->PduInfo.SduLength = PduInfo->SduLength;
				context->PduInfo.SduDataPtr = PduInfo2->SduDataPtr;
				context->index = 0;
				context->state = LINSTP_TX_BUSY;
				context->timer =config->timeout;
			} else {
				ASLOG(LINSTPE, ("[%d] provide TX buffer failed\n",Instance));
			}
		}
	} else {
		ercd = E_NOT_OK;
	}

	return ercd;
}

Std_ReturnType LinSTp_TriggerTransmit(PduIdType Instance, PduInfoType *PduInfoPtr)
{
	Std_ReturnType ercd = E_NOT_OK;
	LinSTp_ContextType* context;
	const LinSTp_ChannelConfigType* config;
	PduLengthType doSize, pos=0;
	uint8_t* data;

	if(Instance < LinSTp_Config.channelNums) {
		context = &LinSTp_Config.contexts[Instance];
		config = &LinSTp_Config.channelConfigs[Instance];
		if(LINSTP_TX_BUSY == context->state) {
			data = PduInfoPtr->SduDataPtr;
			if(context->index < context->PduInfo.SduLength) {
				doSize = context->PduInfo.SduLength - context->index;
				data[pos++] = config->NA;
				if(0 == context->index) {
					if(doSize <= 6) {
						data[pos++] = N_PCI_SF|doSize;
					} else {
						doSize = 5;
						data[pos++] = N_PCI_FF|((doSize>>8)&0xF);
						data[pos++] = doSize&0xFF;
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
				if(context->index >= context->PduInfo.SduLength) {
					Dcm_TxConfirmation(config->dcmTxId, NTFRSLT_OK);
					context->timer = 0;
					context->state = LINSTP_IDLE;
				} else {
					context->timer =config->timeout;
				}
				ercd = E_OK;
			}
		}
	}

	return ercd;
}


void LinSTp_MainFunction(void) {
	int i;
	LinSTp_ContextType* context;
	const LinSTp_ChannelConfigType* config;

	for(i=0; i<LinSTp_Config.channelNums; i++) {
		context = &LinSTp_Config.contexts[i];
		config = &LinSTp_Config.channelConfigs[i];
		if(context->timer > 0) {
			context->timer --;
			if (context->timer == 0) {
				switch(context->state) {
					case LINSTP_RX_BUSY:
						Dcm_RxIndication(config->dcmRxId, NTFRSLT_E_NOT_OK);
						break;
					case LINSTP_TX_BUSY:
						Dcm_TxConfirmation(config->dcmTxId, NTFRSLT_E_NOT_OK);
						break;
					default:
						break;
				}
				ASLOG(LINSTPE, ("%d timeout in state %d\n", i, context->state));
				context->state = LINSTP_IDLE;
			}
		}
	}
}
