#ifndef _LASDEVLIB_H_
#define _LASDEVLIB_H_
/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2017  AS <parai@foxmail.com>
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
#include "Std_Types.h"
#ifndef __AS_PY_DEV__
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#endif
/* ============================ [ MACROS    ] ====================================================== */
#define LAS_DEVICE_NAME_SIZE 256
/* ============================ [ TYPES     ] ====================================================== */
/* device: RS232 - COM1 COM2 .... */
typedef int (*lasdev_open_t)  (const char* device, const char* option, void** param);
typedef int (*lasdev_read_t)  (void* param,char** pdata);
typedef int (*lasdev_write_t) (void* param,const char* data,size_t size);
typedef int (*lasdev_ioctl_t) (void* param,int type, const char* data,size_t size,char** rdata);
typedef void (*lasdev_close_t) (void* param);

typedef struct
{
	char name[LAS_DEVICE_NAME_SIZE];
	lasdev_open_t open;
	lasdev_read_t read;
	lasdev_write_t write;
	lasdev_ioctl_t ioctl;
	lasdev_close_t close;
}LAS_DeviceOpsType;

/* ============================ [ DECLARES  ] ====================================================== */
#ifdef USE_LUA_DEV_LIN
extern const LAS_DeviceOpsType lin_dev_ops;
#endif
#ifdef USE_LUA_DEV_RS232
extern const LAS_DeviceOpsType rs232_dev_ops;
#endif
#ifdef USE_AWS
extern const LAS_DeviceOpsType websock_dev_ops;
#endif
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
void luai_asdevlib_open(void);
void luai_asdevlib_close(void);
#ifdef __AS_PY_DEV__
int asdev_open(const char* device, const char* option);
int asdev_write(int fd, unsigned char* data, unsigned long len);
int asdev_read(int fd, unsigned char** data);
int asdev_ioctl(int fd, int type, unsigned char* data,unsigned long len, unsigned char** rdata);
int asdev_close(int fd);
#else
int luai_as_open  (lua_State *L);
int luai_as_read  (lua_State *L);
int luai_as_write  (lua_State *L);
int luai_as_ioctl  (lua_State *L);
int luai_as_close  (lua_State *L);
#endif
#endif /* _LASDEVLIB_H_ */
