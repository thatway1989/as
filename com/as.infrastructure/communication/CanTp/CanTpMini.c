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

/* based on https://github.com/parai/OpenOSEK/blob/master/OpenCom/src/CanTp.c */
/* ============================ [ INCLUDES  ] ====================================================== */
#include "CanTp.h" /** @req CANTP219 */
#include "CanTp_Cbk.h" /** @req CANTP233 */
#include "CanIf.h"
#include "Dcm.h"
#include "Dcm_Cbk.h"
#ifdef USE_CAN
#include "Can.h"
#endif
#include "Os.h"
#include "asdebug.h"
/* ============================ [ MACROS    ] ====================================================== */
#define AS_LOG_CANTP     0
#define AS_LOG_CANTPE    1

#ifndef CANTP_INSTANCE_NUM
#define CANTP_INSTANCE_NUM 1
#endif

#ifndef CANIF_CANTP_TXPDUID_
#define CANIF_CANTP_TXPDUID_ CANIF_ID_TxDiagP2P
#endif

#if defined(USE_ANYOS) && defined(__AS_BOOTLOADER__)
#ifndef CANTP_MAIN_FUNCTION_PERIOD
#define CANTP_MAIN_FUNCTION_PERIOD 1
#endif
#endif

#ifndef CANTP_MAIN_FUNCTION_PERIOD
#define CANTP_MAIN_FUNCTION_PERIOD 10
#endif
#define msToCanTpTick(__ms) (((__ms)+CANTP_MAIN_FUNCTION_PERIOD-1)/CANTP_MAIN_FUNCTION_PERIOD)

#ifndef CAN_LL_DL
#define CAN_LL_DL 8
#endif

/* Default Configuration */
#define N_As_     1
#define N_Bs_     200
#define N_Cs_     200
#define N_Ar_     200
#define N_Br_     200
#define N_Cr_     200
#define N_STmin_  1
#define N_BS_     8

#define CANTP_LL_DL_ CAN_LL_DL

#define CANIF_CANTP_TXPDUID CANTP_RTE.parameter[0]
#define N_Bs                CANTP_RTE.parameter[1]
#define N_Cr                CANTP_RTE.parameter[2]
#define N_STmin             CANTP_RTE.parameter[3]
#define N_BS                CANTP_RTE.parameter[4]
#if defined(__HIWARE__)
#define CANTP_LL_DL CAN_LL_DL
#else
#define CANTP_LL_DL         CANTP_RTE.parameter[5]
#endif
#define CANTP_PADDING_VALUE 0x55

#define CANTP_INSTANCE_DEFAULT_PARAMETER	\
	{ CANIF_CANTP_TXPDUID_, N_Bs_, N_Cr_, N_STmin_, N_BS_, CANTP_LL_DL_ }

/* see ISO 15765-2 2004 */
#define N_PCI_MASK  0x30
#define N_PCI_SF    0x00
#define N_PCI_FF    0x10
#define N_PCI_CF    0x20
#define N_PCI_FC    0x30
#define N_PCI_SF_DL 0x07
/* Flow Control Status Mask */
#define N_PCI_FS    0x0F
/* Flow Control Status */
#define N_PCI_CTS   0x00
#define N_PCI_WT    0x01
#define N_PCI_OVFLW 0x02

#define N_PCI_SN   0x0F

#define N_SF_MAX_LENGTH   ((CANTP_LL_DL>8)?(CANTP_LL_DL-2):7)
#define N_FF_MAX_LENGTH   ((CANTP_LL_DL>8)?(CANTP_LL_DL-6):6)

#define CANTP_RTE canTpRTE[Instance]

#define tpSetAlarm(timeMs)						\
do{												\
	CANTP_RTE.timer = msToCanTpTick(timeMs)+1;	\
}while(0)

#define tpSignalAlarm()					\
do{										\
	if(CANTP_RTE.timer > 1)				\
	{									\
		CANTP_RTE.timer --;				\
	}									\
}while(0)
#define tpCancelAlarm() { CANTP_RTE.timer = 0;}
#define tpIsAlarmTimeout() ( 1u == CANTP_RTE.timer )
#define tpIsAlarmStarted() ( 0u != CANTP_RTE.timer )

/* ============================ [ TYPES     ] ====================================================== */
enum
{
	CANTP_IDLE = 0,
	/* ========== Receive ========== */
	CANTP_WAIT_CF,
	CANTP_WAIT_FC,
	/* ========== BUSY ============= */
	CANTP_BUSY,          /* To say the rx buffer of handle is in used by UDS,locked */
	/* ========== Send ============= */
	CANTP_START_TO_SEND,
	CANTP_SEND_CF,
	CANTP_SEND_FC
};

typedef struct
{
	PduInfoType* pdu;
	TimerType timer;
	const uint16* parameter;
	PduLengthType SduLength;
	PduLengthType SduIndex;
	uint8 BS;				/* Block Size */
	uint8 SN;				/* Sequence Number */
	uint8 STmin;
	uint8 state;
} CanTp_RuntimeType; /* RTE */
/* ============================ [ DECLARES  ] ====================================================== */
static void SendFC(PduIdType Instance);
/* ============================ [ DATAS     ] ====================================================== */
static CanTp_RuntimeType canTpRTE[CANTP_INSTANCE_NUM];

static const uint16 canTpInstanceDefaultParameter[] = CANTP_INSTANCE_DEFAULT_PARAMETER;
/* ============================ [ staticS    ] ====================================================== */
static void ReceiveSF(uint8 Instance, uint8* Data)
{
	uint8 length;
	uint8 *pData;
	BufReq_ReturnType ret;
	ASLOG(CANTP, ("[%d]ReceiveSF\n",  Instance));
	if(CANTP_RTE.state != CANTP_IDLE)
	{
		ASLOG(CANTPE, ("[%d]Received SF when in state %d.\n",  Instance, CANTP_RTE.state));
	}
	else
	{
		length = Data[0]&N_PCI_SF_DL;
		if(0 == length)
		{	/* okay, not classic CANTP */
			length = Data[1];
			pData = &(Data[2]);
		}
		else
		{
			pData = &(Data[1]);
		}
		ret = Dcm_ProvideRxBuffer(Instance, length, &(CANTP_RTE.pdu));
		if( (BUFREQ_OK == ret) &&
			(NULL != CANTP_RTE.pdu) &&
			(NULL != CANTP_RTE.pdu->SduDataPtr) &&
			(length <= CANTP_RTE.pdu->SduLength)) {
			memcpy(CANTP_RTE.pdu->SduDataPtr,pData,length);
			CANTP_RTE.state = CANTP_BUSY;
			Dcm_RxIndication(Instance, NTFRSLT_OK);
		}
		else
		{
			ASLOG(CANTPE, ("[%d]Received SF provide RX buffer failed %d (%p/%p,%d).\n", Instance,
					ret, CANTP_RTE.pdu, CANTP_RTE.pdu->SduDataPtr, CANTP_RTE.pdu->SduLength));
		}
	}
}

static void ReceiveFF(PduIdType Instance, uint8* Data)
{
	PduLengthType length;
	uint8 *pData;
	BufReq_ReturnType ret;

	ASLOG(CANTP, ("[%d]ReceiveFF\n", Instance));
	if(CANTP_RTE.state != CANTP_IDLE)
	{
		ASLOG(CANTPE, ("[%d]Received FF when in state %d.\n", Instance, CANTP_RTE.state));
	}
	else
	{
		length = ((Data[0]&0x0F) << 8) + Data[1];
		if(0 == length)
		{	/* okay, not classic CANTP */
			length = ((uint32)Data[2]<<24) + ((uint32)Data[3]<<16) + ((uint32)Data[4]<<8) + ((uint32)Data[5]);
			pData = &(Data[6]);
		}
		else
		{
			pData = &(Data[2]);
		}
		ret = Dcm_ProvideRxBuffer(Instance, length, &(CANTP_RTE.pdu));
		if( (BUFREQ_OK == ret) &&
			(NULL != CANTP_RTE.pdu) &&
			(NULL != CANTP_RTE.pdu->SduDataPtr) &&
			(length <= CANTP_RTE.pdu->SduLength)) {
			CANTP_RTE.SduLength = length;
			CANTP_RTE.SduIndex = N_FF_MAX_LENGTH;
			CANTP_RTE.SN = 1;
			memcpy(CANTP_RTE.pdu->SduDataPtr,pData,N_FF_MAX_LENGTH);
			SendFC(Instance);
		}
		else
		{
			ASLOG(CANTPE, ("[%d]Received FF provide RX buffer failed %d (%p/%p,%d).\n", Instance,
					ret, CANTP_RTE.pdu, CANTP_RTE.pdu->SduDataPtr, CANTP_RTE.pdu->SduLength));
		}
	}
}

static void ReceiveCF(PduIdType Instance, uint8* Data)
{
	PduLengthType doSz;
	ASLOG(CANTP, ("[%d]ReceiveCF %d/%d [%02X%02X%02X%02X%02X%02X%02X%02X]\n", Instance,
			CANTP_RTE.SduIndex , CANTP_RTE.SduLength,
			Data[0], Data[1], Data[2], Data[3], Data[4], Data[5], Data[6], Data[7]));

	if(CANTP_RTE.state != CANTP_WAIT_CF)
	{
		ASLOG(CANTPE, ("[%d]Received CF when in state %d.\n", Instance, CANTP_RTE.state));
	}
	else
	{
		if(CANTP_RTE.SN == (Data[0]&N_PCI_SN))
		{
			CANTP_RTE.SN ++ ;
			if(CANTP_RTE.SN > 15) { CANTP_RTE.SN = 0; }
			doSz = CANTP_RTE.SduLength - CANTP_RTE.SduIndex;
			if( doSz > (CANTP_LL_DL-1) ) { doSz = CANTP_LL_DL-1; }
			memcpy(CANTP_RTE.pdu->SduDataPtr + CANTP_RTE.SduIndex, &(Data[1]), doSz);
			CANTP_RTE.SduIndex += doSz;
			if(CANTP_RTE.SduIndex >= CANTP_RTE.SduLength)
			{
				CANTP_RTE.state = CANTP_BUSY;
				tpCancelAlarm();
				Dcm_RxIndication(Instance, NTFRSLT_OK);
			}
			else
			{
				CANTP_RTE.BS --;
				if(0 == CANTP_RTE.BS)
				{
					SendFC(Instance);
				}

				tpSetAlarm(N_Cr);
			}
		}
		else
		{
			CANTP_RTE.state = CANTP_IDLE;
			Dcm_RxIndication(Instance, NTFRSLT_E_NOT_OK);
			ASLOG(CANTPE, ("[%d]Sequence Number Wrong, Abort Current Receiving.\n", Instance));
		}
	}
}

static void ReceiveFC(PduIdType Instance, uint8* Data)
{
	ASLOG(CANTP, ("[%d]ReceiveFC\n",  Instance));
	if(CANTP_RTE.state != CANTP_WAIT_FC)
	{
		ASLOG(CANTPE, ("[%d]Receive FC when in state %d.\n",  Instance, CANTP_RTE.state));
	}
	else
	{
		if((Data[0]&N_PCI_FS) == N_PCI_CTS)
		{
			CANTP_RTE.state = CANTP_SEND_CF;
			if(0u != Data[1])
			{
				CANTP_RTE.BS = Data[1] + 1;
			}
			else
			{
				CANTP_RTE.BS = 0; /* Send all the left without FC */
			}
			CANTP_RTE.STmin = Data[2];
			tpSetAlarm(CANTP_RTE.STmin);
		}
	}
}

static void SendFC(PduIdType Instance)
{
	Std_ReturnType ercd;
	PduInfoType pdu;
	uint8 data[CANTP_LL_DL];

	ASLOG(CANTP, ("[%d]SendFC\n", Instance));
	memset(data, CANTP_PADDING_VALUE, CANTP_LL_DL);

	data[0] = N_PCI_FC|N_PCI_CTS;
	data[1] = N_BS;
	data[2] = N_STmin;
	pdu.SduDataPtr = data;
	pdu.SduLength = CANTP_LL_DL;

	ercd = CanIf_Transmit(CANIF_CANTP_TXPDUID,&pdu);
	if(E_OK == ercd)
	{
		CANTP_RTE.state = CANTP_WAIT_CF;
		tpSetAlarm(N_Cr);
		CANTP_RTE.BS = N_BS;
	}
	else
	{
		/* FAILED, REDO SendFC */
		CANTP_RTE.state = CANTP_SEND_FC;
	}
}

static void SendCF(PduIdType Instance)
{
	boolean doSend = FALSE;
	Std_ReturnType ercd;
	PduInfoType pdu;
	PduLengthType doSz;
	uint8 data[CANTP_LL_DL];

	ASLOG(CANTP, ("[%d]SendCF\n",  Instance));
	memset(data, CANTP_PADDING_VALUE, CANTP_LL_DL);

	if(tpIsAlarmStarted())
	{
		tpSignalAlarm();
		if(tpIsAlarmTimeout())
		{
			doSend = TRUE;
		}
	}
	else
	{
		asAssert(0);
	}

	if(doSend)
	{
		doSz = CANTP_RTE.SduLength - CANTP_RTE.SduIndex;
		if(doSz > (CANTP_LL_DL-1)) { doSz = CANTP_LL_DL-1; }
		data[0] = N_PCI_CF|CANTP_RTE.SN;
		memcpy(&data[1], CANTP_RTE.pdu->SduDataPtr+CANTP_RTE.SduIndex, doSz);
		pdu.SduDataPtr = data;
		pdu.SduLength = CANTP_LL_DL;
		ercd = CanIf_Transmit(CANIF_CANTP_TXPDUID,&pdu);
		if(E_OK == ercd)
		{
			CANTP_RTE.SduIndex += doSz;
			CANTP_RTE.SN ++;
			if(CANTP_RTE.SN > 15) { CANTP_RTE.SN = 0; }
			if(CANTP_RTE.SduIndex >= CANTP_RTE.SduLength)
			{
				CANTP_RTE.state = CANTP_IDLE;
				Dcm_TxConfirmation(Instance, NTFRSLT_OK);
			}
			else if(CANTP_RTE.BS > 1)
			{
				CANTP_RTE.BS --;
				if(1u ==CANTP_RTE.BS)
				{
					CANTP_RTE.state = CANTP_WAIT_FC;
					tpSetAlarm(N_Bs);
				}
				else
				{
					tpSetAlarm(CANTP_RTE.STmin);
				}
			}
			else /* BS = 0 */
			{
				tpSetAlarm(CANTP_RTE.STmin);
			}

		}
		else
		{
			/* FAILED, REDO as no state change */
		}
	}
}
static void SendSF(PduIdType Instance)
{
	Std_ReturnType ercd;
	PduInfoType pdu;
	PduLengthType llLen;
	uint8 data[CANTP_LL_DL];
	uint8* pData;

	ASLOG(CANTP, ("[%d]SendSF\n",  Instance));
	memset(data, CANTP_PADDING_VALUE, CANTP_LL_DL);

	if(CANTP_RTE.SduLength <= 7) {
		llLen = 8;
	} else {
		llLen = CANTP_LL_DL;
	}

	if(llLen > 8)
	{
		data[0] = N_PCI_SF;
		data[1] = CANTP_RTE.SduLength;
		pData = &data[2];
	}
	else
	{
		data[0] = N_PCI_SF|CANTP_RTE.SduLength;
		pData = &data[1];
	}
	memcpy(pData, CANTP_RTE.pdu->SduDataPtr, CANTP_RTE.SduLength);
	pdu.SduDataPtr = data;
	pdu.SduLength  = llLen;

	ercd = CanIf_Transmit(CANIF_CANTP_TXPDUID,&pdu);
	if(E_OK == ercd)
	{
		CANTP_RTE.state = CANTP_IDLE;
		Dcm_TxConfirmation(Instance, NTFRSLT_OK);
	}
	else
	{
		/* FAILED, REDO as no state change */
	}
}

static void SendFF(PduIdType Instance)
{
	Std_ReturnType ercd;
	PduInfoType pdu;
	uint8 data[CANTP_LL_DL];
	uint8* pData;
	uint8 i;

	ASLOG(CANTP, ("[%d]SendFF\n",  Instance));

	if(CANTP_LL_DL > 8)
	{
		data[0] = N_PCI_FF;
		data[1] = 0;
		data[2] = (CANTP_RTE.SduLength>>24)&0xFF;
		data[3] = (CANTP_RTE.SduLength>>16)&0xFF;
		data[4] = (CANTP_RTE.SduLength>>8 )&0xFF;
		data[5] = (CANTP_RTE.SduLength>>0 )&0xFF;
		pData = &data[6];
	}
	else
	{
		data[0] = N_PCI_FF|( (CANTP_RTE.SduLength>>8)&0x0F );
		data[1] = CANTP_RTE.SduLength&0xFF;
		pData = &data[2];
	}
	memcpy(pData, CANTP_RTE.pdu->SduDataPtr, N_FF_MAX_LENGTH);
	pdu.SduDataPtr = data;
	pdu.SduLength = CANTP_LL_DL;

	ercd = CanIf_Transmit(CANIF_CANTP_TXPDUID,&pdu);
	if(E_OK == ercd)
	{
		CANTP_RTE.SduIndex = N_FF_MAX_LENGTH;
		CANTP_RTE.SN = 1;
		CANTP_RTE.state = CANTP_WAIT_FC;
		tpSetAlarm(N_Bs);
	}
	else
	{
		/* FAILED, REDO as no state change */
	}
}

static void StartTransmit(PduIdType Instance)
{
	if(CANTP_RTE.SduLength <= N_SF_MAX_LENGTH)
	{
		SendSF(Instance);
	}
	else
	{
		SendFF(Instance);
	}
}

static void HandleWaiting(PduIdType Instance)
{
	if(tpIsAlarmStarted())
	{
		tpSignalAlarm();
		if(tpIsAlarmTimeout())
		{
			ASLOG(CANTPE, ("[%d]Timeout in the state %d.\n",  Instance, CANTP_RTE.state));
			switch(CANTP_RTE.state)
			{
				case CANTP_WAIT_CF:
					Dcm_RxIndication(Instance, NTFRSLT_E_NOT_OK);
					break;
				case CANTP_WAIT_FC:
					Dcm_TxConfirmation(Instance, NTFRSLT_E_NOT_OK);
					break;
			}

			CANTP_RTE.state = CANTP_IDLE;
		}
	}
	else
	{
		asAssert(0);
	}
}
/* ============================ [ FUNCTIONS ] ====================================================== */
void CanTp_Init(void)
{
	PduIdType Instance;
	for(Instance=0; Instance<CANTP_INSTANCE_NUM; Instance++)
	{
		memset(&CANTP_RTE, 0, sizeof(CANTP_RTE));
		CANTP_RTE.parameter = canTpInstanceDefaultParameter;
	}
}

void CanTp_SetParameter(PduIdType Instance, const uint16* parameter)
{
	asAssert(Instance < CANTP_INSTANCE_NUM);
	CANTP_RTE.parameter = parameter;
}

Std_ReturnType CanTp_Transmit( PduIdType CanTpTxSduId, const PduInfoType * pdu )
{
	Std_ReturnType ercd = E_OK;
	BufReq_ReturnType ret;
	/* NOTE: RX IDs FIRST and THEN TX IDs */
	PduIdType Instance = CanTpTxSduId - CANTP_INSTANCE_NUM;
	asAssert(Instance < CANTP_INSTANCE_NUM);
	ASLOG(CANTP, ("[%d]CanTp_Transmit\n",Instance));
	if((CANTP_BUSY == CANTP_RTE.state) || (CANTP_IDLE == CANTP_RTE.state))
	{
		ret = Dcm_ProvideTxBuffer(Instance, &(CANTP_RTE.pdu), pdu->SduLength);
		if( (BUFREQ_OK == ret) &&
			(NULL != CANTP_RTE.pdu) &&
			(NULL != CANTP_RTE.pdu->SduDataPtr) &&
			(pdu->SduLength <= CANTP_RTE.pdu->SduLength)) {
			CANTP_RTE.SduLength = pdu->SduLength;
			CANTP_RTE.SduIndex = 0;
			CANTP_RTE.state = CANTP_START_TO_SEND;
			StartTransmit(Instance);
		}
		else
		{
			ASLOG(CANTPE, ("[%d]CanTp_Transmit provide TX buffer failed %d(%p/%p,%d).\n",Instance,
					ret, CANTP_RTE.pdu, CANTP_RTE.pdu->SduDataPtr, CANTP_RTE.pdu->SduLength));
		}
	}
	else
	{
		ASLOG(CANTPE, ("[%d]CANTP is not busy in receiving, current state is %d\n", Instance, CANTP_RTE.state));
		ercd = E_NOT_OK;
	}
	return ercd;
}

void CanTp_MainFunction(void)
{
	PduIdType Instance;
	for(Instance=0; Instance<CANTP_INSTANCE_NUM; Instance++)
	{
		switch(CANTP_RTE.state)
		{
			case CANTP_WAIT_CF:
			case CANTP_WAIT_FC:
				HandleWaiting(Instance);
				break;
			case CANTP_START_TO_SEND:
				StartTransmit(Instance);
				break;
			case CANTP_SEND_CF:
				SendCF(Instance);
				break;
			case CANTP_SEND_FC:
				SendFC(Instance);
				break;
			default:
				break;
		}
	}
}

void CanTp_RxIndication( PduIdType Instance, const PduInfoType *pdu )
{
	asAssert(Instance < CANTP_INSTANCE_NUM);
	if(N_PCI_SF == (pdu->SduDataPtr[0]&N_PCI_MASK))
	{
		ReceiveSF(Instance, pdu->SduDataPtr);
	}
	else if(N_PCI_FF == (pdu->SduDataPtr[0]&N_PCI_MASK))
	{
		ReceiveFF(Instance, pdu->SduDataPtr);
	}
	else if(N_PCI_CF == (pdu->SduDataPtr[0]&N_PCI_MASK))
	{
		ReceiveCF(Instance, pdu->SduDataPtr);
	}
	else if(N_PCI_FC == (pdu->SduDataPtr[0]&N_PCI_MASK))
	{
		ReceiveFC(Instance, pdu->SduDataPtr);
	}
	else
	{
		ASLOG(CANTPE, ("[%d]Invalid PCI", Instance));
	}
}

void CanTp_TxConfirmation( PduIdType CanTpTxSduId )
{
	PduIdType Instance = CanTpTxSduId - CANTP_INSTANCE_NUM;
	asAssert(Instance < CANTP_INSTANCE_NUM);
}
