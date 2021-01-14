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
#include "lasdevlib.h"
#include <sys/queue.h>
#include <pthread.h>
#include <unistd.h>
#include "asdebug.h"
/* ============================ [ MACROS    ] ====================================================== */
#define AS_LOG_LAS_DEV 0
/* ============================ [ TYPES     ] ====================================================== */
struct LAS_Dev_s {
	char name[LAS_DEVICE_NAME_SIZE];
	int fd;
	void* param; /* should be filled by device control module while open */
	const LAS_DeviceOpsType* ops;
	uint32_t                size2;
	STAILQ_ENTRY(LAS_Dev_s) entry;
};

struct LAS_DevList_s {
	boolean initialized;
	pthread_mutex_t q_lock;
	STAILQ_HEAD(,LAS_Dev_s) head;
};
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
static const LAS_DeviceOpsType* devOps [] =
{
#ifdef USE_LUA_DEV_LIN
	&lin_dev_ops,
#endif
#ifdef USE_LUA_DEV_RS232
	&rs232_dev_ops,
#endif
#ifdef USE_AWS
	&websock_dev_ops,
#endif
	NULL
};
static int _fd = 0; /* file identifier start from 0 */
static struct LAS_DevList_s devListH =
{
	.initialized = FALSE,
	.q_lock=PTHREAD_MUTEX_INITIALIZER
};
/* ============================ [ LOCALS    ] ====================================================== */
static struct LAS_Dev_s* getDev(const char* name)
{
	struct LAS_Dev_s *handle,*h;
	handle = NULL;

	(void)pthread_mutex_lock(&devListH.q_lock);
	STAILQ_FOREACH(h,&devListH.head,entry)
	{
		if(0u == strcmp(h->name,name))
		{
			handle = h;
			break;
		}
	}
	(void)pthread_mutex_unlock(&devListH.q_lock);

	return handle;
}

static struct LAS_Dev_s* getDev2(int fd)
{
	struct LAS_Dev_s *handle,*h;
	handle = NULL;

	(void)pthread_mutex_lock(&devListH.q_lock);
	STAILQ_FOREACH(h,&devListH.head,entry)
	{
		if(fd == h->fd)
		{
			handle = h;
			break;
		}
	}
	(void)pthread_mutex_unlock(&devListH.q_lock);

	return handle;
}
static const LAS_DeviceOpsType* search_ops(const char* name)
{
	const LAS_DeviceOpsType *ops,**o;
	o = devOps;
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
static void freeDev(struct LAS_DevList_s*h)
{
	struct LAS_Dev_s* d;

	pthread_mutex_lock(&h->q_lock);
	while(FALSE == STAILQ_EMPTY(&h->head))
	{
		d = STAILQ_FIRST(&h->head);
		STAILQ_REMOVE_HEAD(&h->head,entry);
		d->ops->close(d->param);
		free(d);
	}
	pthread_mutex_unlock(&h->q_lock);
}

/* ============================ [ FUNCTIONS ] ====================================================== */
void luai_asdevlib_open(void)
{
	if(devListH.initialized)
	{
		freeDev(&devListH);
	}
	devListH.initialized = TRUE;
	STAILQ_INIT(&devListH.head);

}

void luai_asdevlib_close(void)
{
	if(devListH.initialized)
	{
		freeDev(&devListH);
	}

	devListH.initialized = FALSE;
}
#ifndef __AS_PY_DEV__
int luai_as_open  (lua_State *L)
{
	int n = lua_gettop(L);  /* number of arguments */

	if(n == 2)
	{
		const char* device_name;
		const char* option;
		const LAS_DeviceOpsType* ops;
		size_t ls;
		struct LAS_Dev_s* d;

		device_name = lua_tolstring(L, 1, &ls);
		if((0 == ls) || (ls > LAS_DEVICE_NAME_SIZE))
		{
			 return luaL_error(L,"incorrect argument device name to function '%s'",__func__);
		}

		option = lua_tolstring(L, 2, &ls);
		if(option == NULL)
		{
			 return luaL_error(L,"incorrect argument device option to function '%s'",__func__);
		}

		d = getDev(device_name);
		if(NULL != d)
		{
			return luaL_error(L,"LAS device(%s) is already opened '%s'",device_name,__func__);
		}
		else
		{
			ops = search_ops(device_name);
			if(NULL != ops)
			{
				int rv;
				d = malloc(sizeof(struct LAS_Dev_s));
				strcpy(d->name,device_name);

				rv = ops->open(device_name,option,&d->param);

				if(rv >= 0)
				{
					d->fd = _fd++;
					d->ops = ops;
					pthread_mutex_lock(&devListH.q_lock);
					STAILQ_INSERT_TAIL(&devListH.head,d,entry);
					pthread_mutex_unlock(&devListH.q_lock);
					lua_pushinteger(L, d->fd);        /* result OK */
				}
				else
				{
					free(d);
					return luaL_error(L, "%s device <%s> failed!",__func__,device_name);
				}
			}
			else
			{
				return luaL_error(L, "%s device <%s> is not known by lua!",__func__,device_name);
			}
		}

		return 1;
	}
	else
	{
		return luaL_error(L, "%s (\"device name\",\"device option\") API should has 2 arguments",__func__);
	}
}
int luai_as_read  (lua_State *L)
{
	int n = lua_gettop(L);  /* number of arguments */
	if(1==n)
	{
		int fd;
		int is_num;
		struct LAS_Dev_s* d;

		fd = lua_tounsignedx(L, 1,&is_num);
		if(!is_num)
		{
			 return luaL_error(L,"incorrect argument fd to function '%s'",__func__);
		}

		d = getDev2(fd);
		if(NULL == d)
		{
			 return luaL_error(L,"fd(%d) is not existed '%s'",fd,__func__);
		}
		if(d->ops->read != NULL)
		{
			char* data = NULL;;
			int len = d->ops->read(d->param,&data);

			lua_pushinteger(L,len);

			if(len > 0)
			{
				lua_pushlstring(L,data,len);
			}
			else
			{
				lua_pushnil(L);
			}

			return 2;
		}
		else
		{
			return luaL_error(L, "%s for %s is not supported",__func__,d->name);
		}
	}
	else
	{
		return luaL_error(L, "%s (fd) API should has 1 arguments",__func__);
	}
}

int luai_as_write  (lua_State *L)
{
	int n = lua_gettop(L);  /* number of arguments */
	if(3 == n)
	{
		int fd;
		int is_num;
		size_t ls,len;
		struct LAS_Dev_s* d;
		char* data;

		fd = lua_tounsignedx(L, 1, &is_num);
		if(!is_num)
		{
			 return luaL_error(L,"incorrect argument fd to function '%s'",__func__);
		}

		data = (char*)lua_tolstring(L, 2, &ls);
		if(0 == ls)
		{
			int i = 0;

			ls = luaL_len ( L , 2 ) ;
			data = malloc(ls);

			lua_pushvalue(L, 2);
			lua_pushnil(L);
			while (lua_next(L, -2))
			{
				lua_pushvalue(L, -2);
				data[i] = (char)lua_tounsignedx(L, -2,&is_num);
				ASLOG(LAS_DEV, ("write: data[%d] = %d\n", i, data[i]));
				if(!is_num)
				{
					return luaL_error(L,"invalid data[%d] to function '%s'",i,__func__);
				}
				else
				{
					i ++;
				}

				lua_pop(L, 2);
			}
			lua_pop(L, 1);
		}
		else
		{
			ASLOG(LAS_DEV, ("write string is '%s'\n", data));
		}

	    len = lua_tounsignedx(L, 3, &is_num);
		if(!is_num)
		{
			 return luaL_error(L,"incorrect argument size to function '%s'",__func__);
		}

		if(len > ls)
		{
			len = ls;
		}

		d = getDev2(fd);
		if(NULL == d)
		{
			 return luaL_error(L,"fd(%d) is not existed '%s'",fd,__func__);
		}

		if(d->ops->write != NULL)
		{
			int rv = d->ops->write(d->param,data,len);

			if(rv < 0)
			{
				return luaL_error(L, "%s write on device %s failed(%d)\n",__func__,d->name,rv);
			}
			else
			{
				lua_pushinteger(L, rv);
				return 1;
			}
		}
		else
		{
			return luaL_error(L, "%s for %s is not supported",__func__,d->name);
		}
	}
	else
	{
		return luaL_error(L, "%s (fd,data,size) API should has 3 arguments",__func__);
	}
}

int luai_as_ioctl  (lua_State *L)
{
	int n = lua_gettop(L);  /* number of arguments */
	if(4==n)
	{
		int fd,type;
		int is_num;
		size_t ls,len;
		struct LAS_Dev_s* d;
		char* data;

		fd = lua_tounsignedx(L, 1, &is_num);
		if(!is_num)
		{
			 return luaL_error(L,"incorrect argument fd to function '%s'",__func__);
		}

		type = lua_tounsignedx(L, 2, &is_num);
		if(!is_num)
		{
			 return luaL_error(L,"incorrect argument type to function '%s'",__func__);
		}

		data = (char*)lua_tolstring(L, 3, &ls);
		if(0 == ls)
		{
			int i = 0;

			ls = luaL_len ( L , 3 ) ;
			data = malloc(ls);

			lua_pushvalue(L, 3);
			lua_pushnil(L);
			while (lua_next(L, -2))
			{
				lua_pushvalue(L, -2);
				data[i] = (char)lua_tounsignedx(L, -2,&is_num);
				ASLOG(LAS_DEV, ("ioctl %d: data[%d] = %d\n", type, i, data[i]));
				if(!is_num)
				{
					return luaL_error(L,"invalid data[%d] to function '%s'",i,__func__);
				}
				else
				{
					i ++;
				}

				lua_pop(L, 2);
			}
			lua_pop(L, 1);
		}
		else
		{
			ASLOG(LAS_DEV, ("ioctl string is '%s'\n", data));
		}

	    len = lua_tounsignedx(L, 4, &is_num);
		if(!is_num)
		{
			 return luaL_error(L,"incorrect argument size to function '%s'",__func__);
		}

		if(len > ls)
		{
			len = ls;
		}

		d = getDev2(fd);
		if(NULL == d)
		{
			 return luaL_error(L,"fd(%d) is not existed '%s'",fd,__func__);
		}

		if(d->ops->ioctl != NULL)
		{
			char* rdata;
			int rv = d->ops->ioctl(d->param,type,data,len,&rdata);
			if(rv < 0)
			{
				return luaL_error(L, "%s ioctl on device %s failed(%d)\n",__func__,d->name,rv);
			}
			else
			{
				lua_pushinteger(L, rv);
				if(rv > 0)
				{
					lua_pushlstring(L,rdata,rv);
					return 2;
				}
				else
				{
					return 1;
				}
			}
		}
		else
		{
			return luaL_error(L, "%s for %s is not supported",__func__,d->name);
		}
		return 1;
	}
	else
	{
		return luaL_error(L, "%s (fd) API should has 4 arguments",__func__);
	}
}

int luai_as_close  (lua_State *L)
{
	int n = lua_gettop(L);  /* number of arguments */
	if(1==n)
	{
		int fd;
		int is_num;
		struct LAS_Dev_s* d;

		fd = lua_tounsignedx(L, 1, &is_num);
		if(!is_num)
		{
			 return luaL_error(L,"incorrect argument fd to function '%s'",__func__);
		}
		d = getDev2(fd);
		if(NULL == d)
		{
			 return luaL_error(L,"fd(%d) is not existed '%s'",fd,__func__);
		}

		if(d->ops->close != NULL)
		{
			d->ops->close(d->param);
		}
		else
		{
			return luaL_error(L, "%s for %s is not supported",__func__,d->name);
		}
		return 0;
	}
	else
	{
		return luaL_error(L, "%s (fd) API should has 1 arguments",__func__);
	}
}
#else /* __AS_PY_DEV__ */
int asdev_open(const char* device_name, const char* option)
{
	const LAS_DeviceOpsType* ops;
	struct LAS_Dev_s* d;
	int rv;
	d = getDev(device_name);
	if(NULL != d)
	{
		printf("LAS device(%s) is already opened '%s'\n",device_name,__func__);
		rv = -__LINE__; asAssert(0);
	}
	else
	{
		ops = search_ops(device_name);
		if(NULL != ops)
		{
			d = malloc(sizeof(struct LAS_Dev_s));
			strcpy(d->name,device_name);

			rv = ops->open(device_name,option,&d->param);

			if(rv >= 0)
			{
				d->fd = _fd++;
				d->ops = ops;
				pthread_mutex_lock(&devListH.q_lock);
				STAILQ_INSERT_TAIL(&devListH.head,d,entry);
				pthread_mutex_unlock(&devListH.q_lock);
                rv = d->fd;
			}
			else
			{
				free(d);
				printf("%s device <%s> failed!\n",__func__,device_name);
                rv = -__LINE__; asAssert(0);
			}
		}
		else
		{
			printf("%s device <%s> is not known by lua!\n",__func__,device_name);
			rv = -__LINE__; asAssert(0);
				
		}
	}

	return rv;
}

int asdev_write(int fd, unsigned char* data, unsigned long len)
{
	int rv;
	struct LAS_Dev_s* d;
	d = getDev2(fd);
	if(NULL == d)
	{
		printf("fd(%d) is not existed '%s'\n",fd,__func__);
		rv = -__LINE__; asAssert(0);
	}
	else if(d->ops->write != NULL)
	{
		rv = d->ops->write(d->param,(const char*)data,len);

		if(rv < 0)
		{
			printf("%s write on device %s failed(%d)\n",__func__,d->name,rv);
		}
	}
	else
	{
		printf("%s for %s is not supported\n",__func__,d->name);
		rv = -__LINE__; asAssert(0);
	}
	return rv;
}

int asdev_read(int fd, unsigned char** data)
{
	int rv;
	struct LAS_Dev_s* d;

	d = getDev2(fd);
	if(NULL == d)
	{
		printf("fd(%d) is not existed '%s'\n",fd,__func__);
		rv = -__LINE__; asAssert(0);
	}
	else if(d->ops->read != NULL)
	{
		rv = d->ops->read(d->param,(char **)data);

		if(rv > 0)
		{

		}
		else
		{
			*data=NULL;
		}

	}
	else
	{
		printf("%s for %s is not supported\n",__func__,d->name);
		rv = -__LINE__; asAssert(0);
	}
	return rv;
}

int asdev_ioctl(int fd, int type, unsigned char* data,unsigned long len, unsigned char** rdata)
{
	struct LAS_Dev_s* d;
	int rv;
	d = getDev2(fd);
	if(NULL == d)
	{
		printf("fd(%d) is not existed '%s'\n",fd,__func__);
		rv = -__LINE__; asAssert(0);
	}
	else if(d->ops->ioctl != NULL)
	{
		rv = d->ops->ioctl(d->param,type,(char*)data,len,(char**)rdata);
		if(rv < 0)
		{
			printf("%s ioctl on device %s failed(%d)\n",__func__,d->name,rv);
			rv = -__LINE__; asAssert(0);
		}
	}
	else
	{
		printf("%s for %s is not supported\n",__func__,d->name);
		rv = -__LINE__; asAssert(0);
	}
	return rv;
}
int asdev_close(int fd)
{
	struct LAS_Dev_s* d;
	int rv;
	d = getDev2(fd);
	if(NULL == d)
	{
		printf("fd(%d) is not existed '%s'\n",fd,__func__);
		rv = -__LINE__; asAssert(0);
	}
	else if(d->ops->close != NULL)
	{
		d->ops->close(d->param);
		rv = 1;
	}
	else
	{
		printf("%s for %s is not supported\n",__func__,d->name);
		rv = -__LINE__; asAssert(0);
	}
	return rv;	
}
#endif /* __AS_PY_DEV__ */
