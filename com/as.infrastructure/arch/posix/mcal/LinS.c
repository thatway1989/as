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
#include "lasdevlib.h"
#include "laslinlib.h"
#include "asdebug.h"
#include "Os.h"
/* ============================ [ MACROS    ] ====================================================== */
#define AS_LOG_LINS  0
#define AS_LOG_LINSE 2

#define LIN_BIT(v, pos) (((v)>>(pos)) & 0x01)
/* ============================ [ TYPES     ] ====================================================== */
typedef struct
{
	uint8_t type;
	uint8_t pid;
	uint8_t data[9]; /* data and its checksum */
	uint8_t dlc;
} LinS_FrameType;
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
static int linChannelToFdMap[LINS_CHANNEL_NUM];
static LinS_FrameType linFrame[LINS_CHANNEL_NUM];
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
void LinS_SimulatorRunning(void)
{
	int i;
	int fd;
	uint8_t* data;
	size_t size;
	for(i=0; i<LINS_CHANNEL_NUM; i++) {
		fd = linChannelToFdMap[i];
		if(fd < 0) continue;
		size = asdev_read(fd, &data);
		if((2 == size) && (LIN_TYPE_HEADER == data[0])) {
			linFrame[i].type = LIN_TYPE_HEADER;
			linFrame[i].pid = data[1];
			LinSIf_RxIndication(i, LINS_HEADER, &data[1], size-1);
		} else if((size > 2) && (LIN_TYPE_DATA == data[0])) {
			linFrame[i].type = LIN_TYPE_DATA;
			linFrame[i].dlc = size - 2;
			memcpy(linFrame[i].data, &data[1], size - 2);
			linFrame[i].data[size - 2] = data[size-1];
			LinSIf_RxIndication(i, LINS_FULL, &linFrame[i].pid, size);
		} else if((size > 2) && (LIN_TYPE_HEADER_AND_DATA == data[0])) {
			linFrame[i].type = LIN_TYPE_HEADER_AND_DATA;
			linFrame[i].pid = data[1];
			linFrame[i].dlc = size - 3;
			memcpy(linFrame[i].data, &data[2], size - 3);
			linFrame[i].data[size - 3] = data[size-1];
			LinSIf_RxIndication(i, LINS_FULL, &data[1], size-1);
		} else {
			if(size > 0) {
				ASLOG(LINSE, ("invalid frame received\n"));
			}
		}

		if(size > 0) {
			ASLOG(LINS, ("%d RX: %c %02X @ %d\n", i, data[0], data[1], GetOsTick()));
			free(data);
		}
	}
}

void LinS_Init(void)
{
	int i, fd;
	char device[] = "lin/socket/0";

	for(i=0; i<LINS_CHANNEL_NUM; i++) {
		device[11] += i;
		fd = asdev_open(device, "rw");
		if(fd >= 0) {
			linChannelToFdMap[i] = fd;
		} else {
			linChannelToFdMap[i] = -1;
			ASLOG(LINSE, ("failed to open %s with error %d\n", device, fd));
		}
	}
}

Std_ReturnType LinS_SendFrame(NetworkHandleType netowrk, PduInfoType* pduInfo)
{
	int r=-1, fd;
	Std_ReturnType ercd = E_OK;
	uint8_t data[10];
	int len = 0;
	fd = linChannelToFdMap[netowrk];
	if(fd >= 0) {
		data[0] = LIN_TYPE_DATA;
		memcpy(&data[1], pduInfo->SduDataPtr, pduInfo->SduLength);
		data[1+pduInfo->SduLength] = pduInfo->SduDataPtr[pduInfo->SduLength];
		len = 2+pduInfo->SduLength;

		if(E_OK == ercd) {
			r = asdev_write(fd, data, len);
			if(len != r) {
				ercd = E_NOT_OK;
			}
			ASLOG(LINS, ("%d TX: %c %02X @ %d\n", netowrk, data[0], data[1], GetOsTick()));
		}
	} else {
		ercd = E_NOT_OK;
	}
	return ercd;
}
