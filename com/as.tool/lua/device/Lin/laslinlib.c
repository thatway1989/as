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
#include "laslinlib.h"
#include "asdebug.h"
#include <sys/queue.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <ctype.h>
/* ============================ [ MACROS    ] ====================================================== */
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
static int lasdev_open  (const char* device, const char* option, void** param);
static int lasdev_read  (void* param,char** data);
static int lasdev_write (void* param,const char* data,size_t size);
static void lasdev_close(void* param);
static int lasdev_ioctl (void* param,int type, const char* data,size_t size,char** rdata);
static void* rx_daemon(void* param);
/* ============================ [ DATAS     ] ====================================================== */
const LAS_DeviceOpsType lin_dev_ops = {
	.name = "lin/",
	.open = lasdev_open,
	.read = lasdev_read,
	.write = lasdev_write,
	.close = lasdev_close,
	.ioctl = lasdev_ioctl
};

static const Lin_DeviceOpsType* linOps [] =
{
	#ifdef USE_LUA_DEV_LIN_SOCKET
	&lin_socket_ops,
	#endif
	NULL
};
/* ============================ [ FUNCTIONS ] ====================================================== */
static const Lin_DeviceOpsType* search_ops(const char* name)
{
	const Lin_DeviceOpsType *ops,**o;
	o = linOps;
	ops = NULL;
	while(*o != NULL)
	{
		if(name == strstr(name,(*o)->name))
		{
			ops = *o;
			break;
		}
		o++;
	}

	return ops;
}

static int lasdev_open  (const char* device, const char* option, void** param)
{
	int r = 0;
	Lin_DeviceType* dev = NULL;
	const Lin_DeviceOpsType* ops = NULL;

	ops = search_ops(&device[4]);
	if(NULL == ops) {
		ASLOG(ERROR, ("LIN device(%s) is not found\n", device));
		r = -1;
	}

	if(0 == r) {
		dev = malloc(sizeof(Lin_DeviceType));
		if(NULL == dev) {
			r = -2;
		}
	}

	if(0 == r) {
		strcpy(dev->name, device);
		dev->ops = ops;
		dev->param = NULL;
		dev->size = 0;
		STAILQ_INIT(&dev->head);
		pthread_mutex_init(&dev->q_lock, NULL);
		dev->killed = FALSE;

		r = ops->open(dev, option);

		if(0 == r) {
			if(0 != pthread_create(&(dev->rx_thread),NULL,rx_daemon,(void*)dev)) {
				r = -3;
				ops->close(dev);
			}
		}

		if(0 == r) {
			*param = (void*) dev;
		} else {
			free(dev);
		}
	}
	return r;
}

static int lasdev_read  (void* param,char** data)
{
	int len = 0;
	Lin_DeviceType* dev = (Lin_DeviceType*)param;
	struct Lin_Frame_s* frame;
	(void)pthread_mutex_lock(&dev->q_lock);
	while(FALSE == STAILQ_EMPTY(&dev->head))
	{
		frame = STAILQ_FIRST(&dev->head);
		STAILQ_REMOVE_HEAD(&dev->head,entry);
		*data = (char*)frame->data;
		len = frame->size;
	}
	(void)pthread_mutex_unlock(&dev->q_lock);
	return len;
}

static int lasdev_write (void* param,const char* data,size_t size)
{
	int len = LIN_MTU;
	Lin_DeviceType* dev = (Lin_DeviceType*)param;
	Lin_FrameType frame;

	memset(&frame, 0, LIN_MTU);
	if(((uint8_t)data[0] == LIN_TYPE_HEADER) && (size == 2)) {
		frame.type = LIN_TYPE_HEADER;
		frame.pid = (uint8_t)data[1];
	} else if(((uint8_t)data[0] == LIN_TYPE_DATA) && (size > 2)) {
		frame.type = LIN_TYPE_DATA;
		frame.dlc = size - 2;
		memcpy(&frame.data, &data[1], frame.dlc);
		frame.checksum = data[size-1];
	} else if(((uint8_t)data[0] == LIN_TYPE_HEADER_AND_DATA) && (size > 3)) {
		frame.type = LIN_TYPE_HEADER_AND_DATA;
		frame.pid = (uint8_t)data[1];
		frame.dlc = size - 3;
		memcpy(&frame.data, &data[2], frame.dlc);
		frame.checksum = data[size-1];
	} else {
		ASLOG(ERROR,("Invalid data format for %s\n", dev->name));
		len = -EINVAL;
	}

	if(LIN_MTU == len) {
		len = dev->ops->write(dev, &frame);
	}

	if(LIN_MTU == len) {
		len = size;
	}

	return len;
}

static void lasdev_close(void* param)
{
	void* thread_return;
	Lin_DeviceType* dev = (Lin_DeviceType*)param;
	dev->killed = TRUE;
	pthread_join(dev->rx_thread, &thread_return);
	dev->ops->close(dev);
	free(dev);
}

static int lasdev_ioctl (void* param,int type, const char* data,size_t size,char** rdata)
{
	int r = ENOTSUP;

	return r;
}

void* rx_daemon(void* param)
{
	Lin_DeviceType* dev = (Lin_DeviceType*)param;
	Lin_FrameType frame;
	struct Lin_Frame_s* pframe;
	int r;

	while(FALSE == dev->killed) {
		r = dev->ops->read(dev, &frame);
		if(LIN_MTU == r) {
			pframe = malloc(sizeof(struct Lin_Frame_s));
			if(NULL != pframe) {
				if(frame.type == LIN_TYPE_HEADER) {
					pframe->data[0] = LIN_TYPE_HEADER;
					pframe->data[1] = frame.pid;
					pframe->size = 2;
				} else if(frame.type == LIN_TYPE_DATA) {
					pframe->data[0] = LIN_TYPE_DATA;
					memcpy(&pframe->data[1], frame.data, frame.dlc);
					pframe->data[1+frame.dlc] = frame.checksum;
					pframe->size = 2+frame.dlc;
				} else if(frame.type == LIN_TYPE_HEADER_AND_DATA) {
					pframe->data[0] = LIN_TYPE_HEADER_AND_DATA;
					pframe->data[1] = frame.pid;
					memcpy(&pframe->data[2], frame.data, frame.dlc);
					pframe->data[2+frame.dlc] = frame.checksum;
					pframe->size = 3+frame.dlc;
				} else {
					ASLOG(ERROR, ("invalid frame from %s\n", dev->name));
					continue;
				}
				(void)pthread_mutex_lock(&dev->q_lock);
				STAILQ_INSERT_TAIL(&dev->head, pframe, entry);
				(void)pthread_mutex_unlock(&dev->q_lock);
			}
		}
		usleep(100);
	}

	return NULL;
}
