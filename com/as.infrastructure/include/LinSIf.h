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
#ifndef _LINSIF_H_
#define _LINSIF_H_
/* ============================ [ INCLUDES  ] ====================================================== */
#include "ComStack_Types.h"
/* ============================ [ MACROS    ] ====================================================== */
/* ============================ [ TYPES     ] ====================================================== */
typedef enum {
	LINS_HEADER,
	LINS_FULL,
} LinS_IndicationType;

typedef enum {
	LINSIF_STANDARD,
	LINSIF_ENHANCED
} LinSIf_ChecksumType;

typedef enum {
	LinSIfTxPdu,
	LinSIfRxPdu
} LinSIf_PduDirectionType;

typedef struct {
	LinSIf_ChecksumType     ChecksumType;
	LinSIf_PduDirectionType PduDirection;
	uint8 Pid;
	uint8 Length;
	PduIdType TargetPduId;
} LinSIf_FrameType;

typedef struct {
	const LinSIf_FrameType* frames;
	uint16 numFrames;
} LinSIf_ChannelConfigType;

typedef struct {
	const LinSIf_ChannelConfigType* channelConfigs;
} LinSIf_ConfigType;

#include "LinSIf_Cfg.h"
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
void LinSIf_Init(const LinSIf_ConfigType* config);
void LinSIf_RxIndication(NetworkHandleType network, LinS_IndicationType itype, uint8_t* data, PduLengthType length);
#endif /* _LINSIF_H_ */
