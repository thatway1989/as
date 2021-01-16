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
#ifndef _LIN_LASLINLIB_H_
#define _LIN_LASLINLIB_H_
/* ============================ [ INCLUDES  ] ====================================================== */
#include "Std_Types.h"
#include <sys/queue.h>
#include "lasdevlib.h"
#include <pthread.h>
/* ============================ [ MACROS    ] ====================================================== */
#define LIN_TYPE_INVALID          ((uint8_t)'I')
#define LIN_TYPE_BREAK            ((uint8_t)'B')
#define LIN_TYPE_SYNC             ((uint8_t)'S')
#define LIN_TYPE_HEADER           ((uint8_t)'H')
#define LIN_TYPE_DATA             ((uint8_t)'D')
#define LIN_TYPE_HEADER_AND_DATA  ((uint8_t)'F')

#define LIN_MTU sizeof(Lin_FrameType)
/* ============================ [ TYPES     ] ====================================================== */
typedef struct
{
	uint8_t type;
	uint8_t pid;
	uint8_t dlc;
	uint8_t data[8];
	uint8_t checksum;
} Lin_FrameType;

struct Lin_Frame_s {
	uint8_t data[11];
	int size;
	STAILQ_ENTRY(Lin_Frame_s) entry;
};

typedef struct Lin_DeviceOps_s Lin_DeviceOpsType;

typedef struct Lin_Device_s {
	char name[LAS_DEVICE_NAME_SIZE];
	const Lin_DeviceOpsType* ops;
	pthread_t rx_thread;
	pthread_mutex_t q_lock;
	int killed;
	void* param;
	STAILQ_HEAD(,Lin_Frame_s) head;
	uint32_t                size;
	STAILQ_ENTRY(Lin_Device_s) entry;
} Lin_DeviceType;

typedef int (*lin_device_open_t)(Lin_DeviceType* dev, const char* option);
typedef int (*lin_device_write_t)(Lin_DeviceType* dev, Lin_FrameType* frame);
typedef int (*lin_device_read_t)(Lin_DeviceType* dev, Lin_FrameType* frame);
typedef void (*lin_device_close_t)(Lin_DeviceType* dev);

struct Lin_DeviceOps_s
{
	char name[LAS_DEVICE_NAME_SIZE];
	lin_device_open_t open;
	lin_device_close_t close;
	lin_device_write_t write;
	lin_device_read_t read;
};
/* ============================ [ DECLARES  ] ====================================================== */
#ifdef USE_LUA_DEV_LIN_SOCKET
extern const Lin_DeviceOpsType lin_socket_ops;
#endif
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
#endif /* _LIN_LASLINLIB_H_ */
