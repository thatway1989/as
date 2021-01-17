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
#ifndef LINIF_TYPES_H_
#define LINIF_TYPES_H_
/* ============================ [ INCLUDES  ] ====================================================== */
#include "Std_Types.h"
#include "ComStack_Types.h"
/* ============================ [ MACROS    ] ====================================================== */
/* ============================ [ TYPES     ] ====================================================== */

typedef uint8 LinIf_SchHandleType;

typedef enum {
	UNCONDITIONAL
} LinIf_FrameTypeType;

typedef enum {
	STANDARD,
	ENHANCED
} LinIfChecksumType;

typedef enum {
	LinIfTxPdu,
	LinIfRxPdu
} LinIf_PduDirectionType;

typedef struct {
	LinIf_FrameTypeType LinIfFrameType;
	LinIfChecksumType LinIfChecksumType;
	LinIf_PduDirectionType LinIfPduDirection;
	uint8 LinIfPid;
	uint8 LinIfLength;
	PduIdType LinIfTxTargetPduId;
} LinIf_FrameType;

typedef struct {
	uint32 LinIfFrameRef;
	uint16 LinIfDelay;
} LinIf_EntryType;

typedef struct {
	const LinIf_EntryType* LinIfEntry;
	uint16 LinIfNofEntries;
} LinIf_ScheduleTableType;

typedef struct {
	uint8 LinIfChannelId;
	const Lin_ChannelConfigType* LinIfChannelRef;
} LinIf_ChannelConfigType;
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */

#endif




