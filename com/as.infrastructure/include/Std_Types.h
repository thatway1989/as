/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2015  AS <parai@foxmail.com>
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
#ifndef STD_TYPES_H
#define STD_TYPES_H
/* ============================ [ INCLUDES  ] ====================================================== */
#if defined(USE_LINUX)
#include <linux/types.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/printk.h>
#else
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#endif
#include "io.h"
#include "Compiler.h"
#ifdef USE_LIBELF
#include "elfloader.h"
#endif
/* ============================ [ MACROS    ] ====================================================== */
#ifndef USE_LIBELF
#define ELF_EXPORT(fnc)
#define ELF_EXPORT_ALIAS(fnc, name)
#endif
#if !defined(HIGH_BYTE_FIRST)
#define HIGH_BYTE_FIRST     0U
#endif

#if !defined(LOW_BYTE_FIRST)
#define LOW_BYTE_FIRST      1U
#endif

#ifndef MIN
#define MIN(_x,_y) (((_x) < (_y)) ? (_x) : (_y))
#endif
#ifndef MAX
#define MAX(_x,_y) (((_x) > (_y)) ? (_x) : (_y))
#endif
#ifndef E_OK
#define E_OK 					(Std_ReturnType)0
#endif
#ifndef E_NOT_OK
#define E_NOT_OK 				(Std_ReturnType)1
#endif

#define E_NO_DTC_AVAILABLE		(Std_ReturnType)2
#define E_SESSION_NOT_ALLOWED	(Std_ReturnType)4
#define E_PROTOCOL_NOT_ALLOWED	(Std_ReturnType)5
#define E_REQUEST_NOT_ACCEPTED	(Std_ReturnType)8
#define E_REQUEST_ENV_NOK		(Std_ReturnType)9
#ifndef E_PENDING	// for WIN32
#define E_PENDING				(Std_ReturnType)10
#endif
#define E_COMPARE_KEY_FAILED	(Std_ReturnType)11
#define E_FORCE_RCRRP			(Std_ReturnType)12

#ifndef NULL
#define NULL 0
#endif

#ifndef NULL_PTR
#define NULL_PTR (void*)0
#endif

#ifndef TRUE
#define TRUE                    (boolean)1
#endif
#ifndef FALSE
#define FALSE                   (boolean)0
#endif

#ifndef True
#define True                    (boolean)1
#endif
#ifndef False
#define False                   (boolean)0
#endif

#ifndef true
#define true                    (boolean)1
#endif
#ifndef false
#define false                   (boolean)0
#endif

#define STD_HIGH		0x01
#define STD_LOW			0x00

#define STD_ACTIVE		0x01
#define STD_IDLE		0x00

#define STD_ON			0x01
#define STD_OFF			0x00

#define SIZE_OF_ARRAY(a) 	(sizeof(a)/sizeof(a[0]))

#ifndef OS_TICKS_PER_SECOND
#define OS_TICKS_PER_SECOND 1000
#endif
#ifndef USECONDS_PER_TICK
#define USECONDS_PER_TICK (1000000/OS_TICKS_PER_SECOND)
#endif

#ifdef __LINUX__
#define MS2TICKS(m) (m)
#else
#define MS2TICKS(m) ((m+(1000/OS_TICKS_PER_SECOND -1))/(1000/OS_TICKS_PER_SECOND))
#endif

#ifndef ENOTSUP
#define ENOTSUP         129
#endif

#ifdef USE_STDRT
#include <rtthread.h>
#define malloc rt_malloc
#define strcmp rt_strcmp
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
#endif

#ifdef USE_STD_ERROR
#define EPERM 1
#define ENOENT 2
#define ENOFILE ENOENT
#define ESRCH 3
#define EINTR 4
#define EIO 5
#define ENXIO 6
#define E2BIG 7
#define ENOEXEC 8
#define EBADF 9
#define ECHILD 10
#define EAGAIN 11
#define ENOMEM 12
#define EACCES 13
#define EFAULT 14
#define EBUSY 16
#define EEXIST 17
#define EXDEV 18
#define ENODEV 19
#define ENOTDIR 20
#define EISDIR 21
#define ENFILE 23
#define EMFILE 24
#define ENOTTY 25
#define EFBIG 27
#define ENOSPC 28
#define ESPIPE 29
#define EROFS 30
#define EMLINK 31
#define EPIPE 32
#ifndef EDOM
#define EDOM 33
#endif
#define EDEADLK 36
#define ENAMETOOLONG 38
#define ENOLCK 39
#define ENOSYS 40
#define ENOTEMPTY 41

#define EINVAL 22
#ifndef ERANGE
#define ERANGE 34
#endif
#define EILSEQ 42

#define ETIMEDOUT 138
#endif
/* ============================ [ TYPES     ] ====================================================== */
#if defined(USE_LINUX)
/* Exact integral types.  */

/* Signed.  */

/* There is some amount of overlap with <sys/types.h> as known by inet code */

typedef signed char		int8_t;
typedef short int		int16_t;
typedef int			int32_t;
typedef long long int		int64_t;

/* Unsigned.  */
typedef unsigned char		uint8_t;
typedef unsigned short int	uint16_t;
typedef unsigned int		uint32_t;
typedef unsigned long long int	uint64_t;

/* Small types.  */
#endif
#if defined(USE_FAST_LEAST_TYPE_DEF)
/* Signed.  */
typedef signed char		int_least8_t;
typedef short int		int_least16_t;
typedef int			int_least32_t;
typedef long long int		int_least64_t;

/* Unsigned.  */
typedef unsigned char		uint_least8_t;
typedef unsigned short int	uint_least16_t;
typedef unsigned int		uint_least32_t;
typedef unsigned long long int	uint_least64_t;

/* Fast types.  */

/* Signed.  */
typedef signed char		int_fast8_t;
typedef long int		int_fast16_t;
typedef long int		int_fast32_t;
typedef long int		int_fast64_t;

/* Unsigned.  */
typedef unsigned char		uint_fast8_t;
typedef unsigned long int	uint_fast16_t;
typedef unsigned long int	uint_fast32_t;
typedef unsigned long int	uint_fast64_t;
#endif
typedef unsigned char		       	boolean;
typedef int8_t        				sint8;
typedef uint8_t			       		uint8;
typedef int16_t		        		sint16;
typedef uint16_t		      		uint16;
/* note for 64 bit ubuntu, long is 8 bytes */
typedef int32_t	         	    	sint32;
typedef uint32_t		       		uint32;
typedef int64_t			 			sint64;
typedef uint64_t			 		uint64;
typedef int_least8_t				sint8_least;
typedef uint_least8_t   			uint8_least;
typedef int_least16_t   			sint16_least;
typedef uint_least16_t  			uint16_least;
typedef int_least32_t   			sint32_least;
typedef uint_least32_t  			uint32_least;
typedef int_fast8_t					sint8f;
typedef uint_fast8_t				uint8f;
typedef int_fast16_t				sint16f;
typedef uint_fast16_t   			uint16f;
typedef int_fast32_t				sint32f;
typedef uint_fast32_t   			uint32f;

typedef float                       float32;
typedef double                      float64;

typedef uint8                       Std_ReturnType;

typedef uint64 u64;
typedef uint32 u32;
typedef uint16 u16;
typedef uint8  u8;

typedef sint64 s64;
typedef sint32 s32;
typedef sint16 s16;
typedef sint8  s8;

typedef unsigned int imask_t;

typedef struct {
	// TODO: not done!!
	uint16 vendorID;
	uint16 moduleID;
	uint8  instanceID;

	uint8 sw_major_version;    /**< Vendor numbers */
	uint8 sw_minor_version;    /**< Vendor numbers */
	uint8 sw_patch_version;    /**< Vendor numbers */

	uint8 ar_major_version;    /**< Autosar spec. numbers */
	uint8 ar_minor_version;    /**< Autosar spec. numbers */
	uint8 ar_patch_version;    /**< Autosar spec. numbers */
} Std_VersionInfoType;

#ifndef __use_os_trampoline__
typedef uint8 CoreIdType;
#endif
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
extern void Irq_Enable(void);
extern void Irq_Disable(void);
#define Irq_Save(imask) imask=__Irq_Save()
extern imask_t __Irq_Save(void);
extern void Irq_Restore(imask_t imask);

extern void *asmalloc( size_t xWantedSize );
extern void  asfree( void *pv );
extern void* kzmalloc(size_t size);
extern void* palloc(size_t size);
#endif /* STD_TYPES_H */
