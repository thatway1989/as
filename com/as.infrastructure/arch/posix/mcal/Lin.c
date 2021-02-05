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
#include "Lin.h"
#include "lasdevlib.h"
#include "laslinlib.h"
#include "asdebug.h"
#include "Os.h"
/* ============================ [ MACROS    ] ====================================================== */
#define AS_LOG_LIN  0
#define AS_LOG_LINE 2

#define LIN_BIT(v, pos) (((v)>>(pos)) & 0x01)
/* ============================ [ TYPES     ] ====================================================== */
typedef enum {
	LIN_STATE_IDLE,
	LIN_STATE_ONLINE,
	LIN_STATE_ONLY_HEADER_TRANSMITTING,
	LIN_STATE_HEADER_TRANSMITTING,
	LIN_STATE_RESPONSE_TRANSMITTING,
	LIN_STATE_FULL_TRANSMITTING,
	LIN_STATE_WAITING_RESPONSE,
	LIN_STATE_ONLY_HEADER_TRANSMITTED,
	LIN_STATE_RESPONSE_TRANSMITTED,
	LIN_STATE_FULL_TRANSMITTED,
	LIN_STATE_RESPONSE_RECEIVED,
} Lin_StateType;
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
static int linChannelToFdMap[LIN_CHANNEL_NUM];
static Lin_StateType linState[LIN_CHANNEL_NUM];
static Lin_FrameType linFrame[LIN_CHANNEL_NUM];
/* ============================ [ LOCALS    ] ====================================================== */
static uint8_t Lin_GetPid(Lin_PduType* PduInfoPtr)
{
	uint8_t pid = PduInfoPtr->Pid & 0x3F;
	uint8_t p0 = LIN_BIT(pid, 0) ^ LIN_BIT(pid, 1) ^ LIN_BIT(pid, 2) ^ LIN_BIT(pid, 4);
	uint8_t p1 = ~(LIN_BIT(pid, 1) ^ LIN_BIT(pid, 3) ^ LIN_BIT(pid, 4) ^ LIN_BIT(pid, 5));
	pid = pid | (p0<<6) | (p1<<7);

	return pid;
}

static void Lin_GetData(Lin_PduType* PduInfoPtr, uint8_t* data, uint8_t* p_checksum)
{
	int i;
	uint8_t checksum = 0;
	uint8_t pid;

	if(LIN_ENHANCED_CS == PduInfoPtr->Cs) {
		pid = Lin_GetPid(PduInfoPtr);
		checksum += pid;
	}

	for(i=0; i<PduInfoPtr->DI; i++) {
		data[i] = PduInfoPtr->SduPtr[i];
		checksum += PduInfoPtr->SduPtr[i];
	}

	*p_checksum = ~checksum;
}
/* ============================ [ FUNCTIONS ] ====================================================== */
void Lin_SimulatorRunning(void)
{
	int i;
	int fd;
	uint8_t* data;
	size_t size;
	for(i=0; i<LIN_CHANNEL_NUM; i++) {
		fd = linChannelToFdMap[i];
		if(fd < 0) continue;
		size = asdev_read(fd, &data);
		if((2 == size) && (LIN_TYPE_HEADER == data[0])) {
			linFrame[i].type = LIN_TYPE_HEADER;
			linFrame[i].pid = data[1];
			// TODO: check PID parity
		} else if((size > 2) && (LIN_TYPE_DATA == data[0])) {
			linFrame[i].type = LIN_TYPE_DATA;
			linFrame[i].dlc = size - 2;
			memcpy(linFrame[i].data, &data[1], size - 2);
			linFrame[i].checksum = data[size-1];
			// TODO: check checksum
			if(linState[i] == LIN_STATE_ONLY_HEADER_TRANSMITTING) {
				/* slave to slave response */
				linState[i] = LIN_STATE_ONLY_HEADER_TRANSMITTED;
			} else if((linState[i] == LIN_STATE_WAITING_RESPONSE) ||
					  (linState[i] == LIN_STATE_HEADER_TRANSMITTING)) {
				linState[i] = LIN_STATE_RESPONSE_RECEIVED;
			} else {
				ASLOG(LINE, ("frame received in state %d\n", linState[i]));
			}
		} else {
			if(size > 0) {
				ASLOG(LINE, ("invalid frame received\n"));
			}
		}

		if(size > 0) {
			ASLOG(LIN, ("%d RX: %c %02X @ %d\n", i, data[0], data[1], GetOsTick()));
			free(data);
		}
		switch(linState[i]) {
			case LIN_STATE_ONLY_HEADER_TRANSMITTING:
				linState[i] = LIN_STATE_ONLY_HEADER_TRANSMITTED;
				break;
			case LIN_STATE_HEADER_TRANSMITTING:
				linState[i] = LIN_STATE_WAITING_RESPONSE;
				break;
			case LIN_STATE_RESPONSE_TRANSMITTING:
				linState[i] = LIN_STATE_RESPONSE_TRANSMITTED;
				break;
			case LIN_STATE_FULL_TRANSMITTING:
				linState[i] = LIN_STATE_FULL_TRANSMITTED;
				break;
			default:
				break;
		}
	}
}

void Lin_Init(const Lin_ConfigType* Config)
{
	int i;
	(void) Config;

	for(i=0; i<LIN_CHANNEL_NUM; i++) {
		linChannelToFdMap[i] = -1;
		linState[i] = LIN_STATE_IDLE;
	}
}

void Lin_InitChannel(  uint8 Channel, const Lin_ChannelConfigType* Config )
{
	char device[] = "lin/socket/0";
	(void)Config;

	int fd;
	if(Channel < LIN_CHANNEL_NUM) {
		fd = linChannelToFdMap[Channel];
		if(fd < 0) {
			device[11] += Channel;
			fd = asdev_open(device, "rw");
			if(fd >= 0) {
				linChannelToFdMap[Channel] = fd;
				linState[Channel] = LIN_STATE_ONLINE;
			} else {
				ASLOG(LINE, ("failed to open %s with error %d\n", device, fd));
			}
		} else {
			ASLOG(LINE, ("device %s already opened\n", device));
		}
	} else {
		ASLOG(LINE, ("invalid channel %d\n", Channel));
	}
}

void Lin_DeInitChannel( uint8 Channel )
{
	int fd;
	if(Channel < LIN_CHANNEL_NUM) {
		fd = linChannelToFdMap[Channel];
		if(fd >= 0) {
			asdev_close(fd);
			linChannelToFdMap[Channel] = -1;
		} else {
			ASLOG(LINE, ("channel %d not opened\n", Channel));
		}
	} else {
		ASLOG(LINE, ("invalid channel %d\n", Channel));
	}
}

void Lin_DeInit()
{
	int i;

	for(i=0; i<LIN_CHANNEL_NUM; i++) {
		if(linChannelToFdMap[i] >= 0) {
			asdev_close(linChannelToFdMap[i]);
		}
		linChannelToFdMap[i] = -1;
	}
}

void Lin_WakeupValidation( void )
{

}

Std_ReturnType Lin_SendFrame( uint8 Channel,  Lin_PduType* PduInfoPtr )
{
	int r=-1, fd;
	Std_ReturnType ercd = E_OK;
	uint8_t data[12];
	int len = 0;
	Lin_StateType state;
	fd = linChannelToFdMap[Channel];
	if(fd >= 0) {
		if(LIN_MASTER_RESPONSE == PduInfoPtr->Drc) {
			data[0] = LIN_TYPE_HEADER_AND_DATA;
			data[1] = Lin_GetPid(PduInfoPtr);
			Lin_GetData(PduInfoPtr, &data[2], &data[2+PduInfoPtr->DI]);
			len = 3+PduInfoPtr->DI;
			state = LIN_STATE_FULL_TRANSMITTING;
		} else if((LIN_SLAVE_RESPONSE == PduInfoPtr->Drc) ||
				  (LIN_SLAVE_TO_SLAVE ==  PduInfoPtr->Drc)) {
			data[0] = LIN_TYPE_HEADER;
			data[1] = Lin_GetPid(PduInfoPtr);
			len = 2;
			if(LIN_SLAVE_RESPONSE == PduInfoPtr->Drc) {
				state = LIN_STATE_HEADER_TRANSMITTING;
			} else {
				state = LIN_STATE_ONLY_HEADER_TRANSMITTING;
			}
		} else if(LIN_RESPONSE_DATA == PduInfoPtr->Drc) {
			data[0] = LIN_TYPE_DATA;
			Lin_GetData(PduInfoPtr, &data[1], &data[1+PduInfoPtr->DI]);
			len = 2+PduInfoPtr->DI;
			state = LIN_STATE_RESPONSE_TRANSMITTING;
		} else {
			ercd = E_NOT_OK;
		}

		if(E_OK == ercd) {
			ASLOG(LIN, ("%d TX: %c %02X @ %d\n", Channel, data[0], data[1], GetOsTick()));
			r = asdev_write(fd, data, len);
			if(len != r) {
				ercd = E_NOT_OK;
			} else {
				linState[Channel] = state;
			}
		}
	} else {
		ercd = E_NOT_OK;
	}
	return ercd;
}

Std_ReturnType Lin_GoToSleep( uint8 Channel )
{
	Std_ReturnType ercd = E_OK;
	return ercd;
}

Std_ReturnType Lin_GoToSleepInternal( uint8 Channel )
{
	Std_ReturnType ercd = E_OK;
	return ercd;
}

Std_ReturnType Lin_WakeUp( uint8 Channel )
{
	Std_ReturnType ercd = E_OK;
	return ercd;
}

Lin_StatusType Lin_GetStatus( uint8 Channel, uint8** Lin_SduPtr )
{
	Lin_StatusType status = E_OK;
	*Lin_SduPtr = NULL;
	switch(linState[Channel]) {
		case LIN_STATE_ONLINE:
			status = LIN_CH_OPERATIONAL;
			break;
		case LIN_STATE_HEADER_TRANSMITTING:
		case LIN_STATE_WAITING_RESPONSE:
			status = LIN_RX_NO_RESPONSE;
			break;
		case LIN_STATE_ONLY_HEADER_TRANSMITTING:
		case LIN_STATE_RESPONSE_TRANSMITTING:
		case LIN_STATE_FULL_TRANSMITTING:
			status = LIN_TX_BUSY;
			break;
		case LIN_STATE_ONLY_HEADER_TRANSMITTED:
		case LIN_STATE_RESPONSE_TRANSMITTED:
		case LIN_STATE_FULL_TRANSMITTED:
			status = LIN_TX_OK;
			break;
		case LIN_STATE_RESPONSE_RECEIVED:
			*Lin_SduPtr = linFrame[Channel].data;
			status = LIN_RX_OK;
			break;
	}
	return status;
}

