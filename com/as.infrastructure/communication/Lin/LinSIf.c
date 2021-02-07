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
#include "LinSIf.h"
#include "PduR_LinIf.h"
#include "Os.h"
#include "asdebug.h"
/* ============================ [ MACROS    ] ====================================================== */
#define AS_LOG_LINSIF  0
#define AS_LOG_LINSIFE 2

#define LIN_BIT(v, pos) (((v)>>(pos)) & 0x01)
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
extern void LinS_Init(void);
extern Std_ReturnType LinS_SendFrame(NetworkHandleType netowrk, PduInfoType* pduInfo);
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
static const LinSIf_FrameType* getFrameConfig(NetworkHandleType network, uint8_t pid) {
	int i;
	const LinSIf_FrameType* frame = NULL;

	//TODO: check PID
	pid = pid&0x3F;
	for(i=0; i<LinSIf_Config.channelConfigs[network].numFrames; i++) {
		if(pid == LinSIf_Config.channelConfigs[network].frames[i].Pid) {
			frame = &LinSIf_Config.channelConfigs[network].frames[i];
		}
	}

	return frame;
}
uint8_t getPPid(uint8_t Pid)
{
	uint8_t pid = Pid & 0x3F;
	uint8_t p0 = LIN_BIT(pid, 0) ^ LIN_BIT(pid, 1) ^ LIN_BIT(pid, 2) ^ LIN_BIT(pid, 4);
	uint8_t p1 = ~(LIN_BIT(pid, 1) ^ LIN_BIT(pid, 3) ^ LIN_BIT(pid, 4) ^ LIN_BIT(pid, 5));
	pid = pid | (p0<<6) | (p1<<7);

	return pid;
}

static uint8_t calcChecksum(const LinSIf_FrameType* frame, uint8_t* data, PduLengthType length)
{
	int i;
	uint8_t checksum = 0;
	uint8_t pid;

	if(LINSIF_ENHANCED == frame->ChecksumType) {
		pid = getPPid(frame->Pid);
		checksum += pid;
	}

	for(i=0; i<length; i++) {
		checksum += data[i];
	}

	return ~checksum;
}

static void handleHeader(NetworkHandleType network, uint8_t pid) {
	int i;
	const LinSIf_FrameType* frame;
	Std_ReturnType ercd;
	uint8_t data[9]; /* data and checksum */
	PduInfoType pduInfo;

	ASLOG(LINSIF, ("%d RX: Pid=%02X\n", (int)network,(uint32)pid));
	frame = getFrameConfig(network, pid);
	if(NULL != frame) {
		if(frame->PduDirection == LinSIfTxPdu) {
			pduInfo.SduDataPtr = data;
			pduInfo.SduLength = frame->Length;
			ercd = PduR_LinIfTriggerTransmit(frame->TargetPduId, &pduInfo);
			if(E_OK == ercd) {
				data[frame->Length] = calcChecksum(frame, data, frame->Length);
				ercd = LinS_SendFrame(network, &pduInfo);
				if(E_OK != ercd) {
					ASLOG(LINSIFE, ("failed to send frame\n"));
				}
			} else {
				ASLOG(LINSIFE, ("failed to get PDU data\n"));
			}
		}
	} else {
		ASLOG(LINSIF, ("ignored\n"));
	}
}

static void handleFull(NetworkHandleType network, uint8_t pid, uint8_t* data, PduLengthType length, uint8_t checksum) {
	int i;
	const LinSIf_FrameType* frame;
	Std_ReturnType ercd;
	PduInfoType pduInfo;
	uint8_t cksum;
	ASLOG(LINSIF, ("%d RX: Pid=%02X, DLC=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X] checksum=%02X @%u\n",
				(int)network,(uint32)pid,(int)length,
				(uint32)data[0],(uint32)data[1],(uint32)data[2],(uint32)data[3],
				(uint32)data[4],(uint32)data[5],(uint32)data[6],(uint32)data[7],
				(uint32)checksum, GetOsTick()));
	frame = getFrameConfig(network, pid);
	if(NULL != frame) {
		if(frame->PduDirection == LinSIfRxPdu) {
			cksum = calcChecksum(frame, data, length);
			if(cksum == checksum) {
				pduInfo.SduDataPtr = data;
				pduInfo.SduLength = length;
				PduR_LinIfRxIndication(frame->TargetPduId,&pduInfo);
			} else {
				ASLOG(LINSIFE, ("checksum %02X is not correct\n", cksum));
			}
		}
	} else {
		ASLOG(LINSIF, ("ignored\n"));
	}
}
/* ============================ [ FUNCTIONS ] ====================================================== */
void LinSIf_Init(const LinSIf_ConfigType* config)
{
	(void*) config;
	LinS_Init();
}

void LinSIf_RxIndication(NetworkHandleType network, LinS_IndicationType itype, uint8_t* data, PduLengthType length)
{
	asAssert(network < LINSIF_CHANNEL_NUM);
	asAssert(data);
	if(LINS_HEADER == itype) {
		asAssert(length == 1);
		handleHeader(network, data[0]);
	} else if(LINS_FULL== itype) {
		asAssert(length > 2);
		handleFull(network, data[0], &data[1], length-2, data[length-1]);
	} else {
		asAssert(0);
	}

}
