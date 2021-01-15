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

		r = ops->open(dev, option);

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

	return len;
}

static int lasdev_write (void* param,const char* data,size_t size)
{
	int len;
	Lin_DeviceType* dev = (Lin_DeviceType*)param;

	len = dev->ops->write(dev, data, size);

	return len;
}

static void lasdev_close(void* param)
{
	Lin_DeviceType* dev = (Lin_DeviceType*)param;
	dev->ops->close(dev);
	free(dev);
}

static int lasdev_ioctl (void* param,int type, const char* data,size_t size,char** rdata)
{
	int r = ENOTSUP;

	return r;
}

