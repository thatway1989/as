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
#include "asdebug.h"
/* ============================ [ MACROS    ] ====================================================== */
#define AS_LOG_LINE 2
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
static int linChannelToFdMap[LIN_CHANNEL_NUM];
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
void Lin_SimulatorRunning(void)
{

}

void Lin_Init(const Lin_ConfigType* Config)
{
	int i;
	(void) Config;

	for(i=0; i<LIN_CHANNEL_NUM; i++) {
		linChannelToFdMap[i] = -1;
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



Std_ReturnType Lin_SendHeader(  uint8 Channel,  Lin_PduType* PduInfoPtr )
{
	Std_ReturnType ercd = E_OK;
	return ercd;
}

Std_ReturnType Lin_SendResponse(  uint8 Channel,   Lin_PduType* PduInfoPtr )
{
	Std_ReturnType ercd = E_OK;
	return ercd;
}

Std_ReturnType Lin_GoToSleep(  uint8 Channel )
{
	Std_ReturnType ercd = E_OK;
	return ercd;
}

Std_ReturnType Lin_GoToSleepInternal(  uint8 Channel )
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
	Std_ReturnType ercd = E_OK;
	return ercd;
}

