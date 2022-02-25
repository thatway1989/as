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
/* ============================ [ INCLUDES  ] ====================================================== */
#include "EcuM.h"
#include "asdebug.h"
#ifdef USE_STDRT
#include "rthw.h"
#include "rtthread.h"
#ifdef RT_USING_LWIP
#include <lwip/sys.h>
#include <netif/ethernetif.h>
#endif
#ifdef RT_USING_DFS
/* dfs filesystem:ELM filesystem init */
#include <dfs_elm.h>
#if defined(RT_USING_LWEXT4)
#include <dfs_ext.h>
#endif
/* dfs Filesystem APIs */
#include <dfs_fs.h>
#endif
#ifdef RT_USING_PTHREADS
#include <pthread.h>
#endif
#endif
/* ============================ [ MACROS	] ====================================================== */

/* ============================ [ TYPES	 ] ====================================================== */

/* ============================ [ DECLARES  ] ====================================================== */
#ifdef USE_STDRT
extern void rt_hw_timer_init(void);
extern void rt_hw_console_init(void);
extern void finsh_system_init(void);
extern void finsh_set_device(const char* name);
extern void tap_netif_hw_init(void);
extern void lwip_system_init(void);
extern void netbios_init(void);
extern void rt_hw_asblk_init_all(void);
extern void ftpd_start();
extern rt_err_t rt_thread_sleep(rt_tick_t tick);
extern void finsh_set_echo(rt_uint32_t echo);
extern int rt_system_dlmodule_init(void);
#endif
/* ============================ [ DATAS	 ] ====================================================== */
#ifdef USE_STDRT
#ifdef RT_USING_HEAP
static rt_uint8_t ucHeap[RT_HEAP_SIZE];
#endif
#endif
/* ============================ [ LOCALS	] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
int main(int argc,char* argv[])
{
	ASLOG(ON, ("\n!!!---------main function begin----------\n"));
	ASENVINIT(argc,argv);
	EcuM_Init();
	while(1);
	return 0;
}
#ifdef USE_STDRT
void rt_init_thread(void* parameter)
{
	rt_kprintf("rt_init_thread is running!\n");
#ifdef RT_USING_PTHREADS
	pthread_system_init();
#endif

#ifdef RT_USING_DFS
	rt_hw_asblk_init_all();

	/* initialize the device file system */
	dfs_init();

#ifdef RT_USING_DFS_ELMFAT
	/* initialize the elm chan FatFS file system*/
	elm_init();
#endif
#if defined(RT_USING_LWEXT4)
	dfs_ext_init();
#endif
#ifdef RT_USING_MODULE
	rt_system_dlmodule_init();
#endif

#ifdef USE_FATFS
	if (dfs_mount("asblk0", "/", "elm", 0, 0) == 0)
	{
		rt_kprintf("File System FATFS initialized!\n");
	}
	else
	{
		rt_kprintf("File System FATFS initialzation failed!\n");
	}
#endif
#if defined(USE_LWEXT4)
#ifdef USE_FATFS
	mkdir("/ext", -1);
	if (dfs_mount("asblk1", "/ext", "ext", 0, 0) == 0)
#else
	if (dfs_mount("asblk1", "/", "ext", 0, 0) == 0)
#endif
	{
		rt_kprintf("File System EXTFS initialized!\n");
	}
	else
	{
		rt_kprintf("File System EXTFS initialzation failed!\n");
	}
#endif
#endif  /* RT_USING_DFS */

	EcuM_Init();

#ifdef USE_ARCH_X86
	rt_thread_sleep(100);
	finsh_set_echo(1);
#endif
}
void __weak rt_hw_board_init(void)
{

}
void rt_application_init(void)
{
	rt_thread_t tid;

#ifdef RT_USING_HEAP
	/* init memory system */
	rt_system_heap_init((void *)ucHeap, (void *)&ucHeap[RT_HEAP_SIZE]);
#endif

	/* init the console */
	rt_hw_console_init();
	rt_console_set_device("console");

#ifdef RT_USING_FINSH
	/* init finsh */
	finsh_system_init();
#ifndef RT_USING_POSIX
	finsh_set_device("console");
#endif
#endif

	tid = rt_thread_create("rtinit",
						  (void(*)(void*))rt_init_thread, RT_NULL,
						   2048, 0, 20);

	if (tid != RT_NULL)
		rt_thread_startup(tid);

	rt_hw_timer_init();
}
void rtthread_startup(void)
{
	rt_hw_interrupt_disable();

	/* board level initalization
	 * NOTE: please initialize heap inside board initialization.
	 */
	rt_hw_board_init();

	/* show RT-Thread version */
	rt_show_version();

	/* timer system initialization */
	rt_system_timer_init();

	/* scheduler system initialization */
	rt_system_scheduler_init();

#ifdef RT_USING_SIGNALS
	/* signal system initialization */
	rt_system_signal_init();
#endif

	/* create init_thread */
	rt_application_init();

	/* timer thread initialization */
	rt_system_timer_thread_init();

	/* idle thread initialization */
	rt_thread_idle_init();

	/* start scheduler */
	rt_system_scheduler_start();

}
#ifdef USE_LWIP
void LwIP_Init(void)
{
	ASLOG(ON, ("!!!main.c lwip init begin\n"));
	/* initialize lwip stack */
	/* register ethernetif device */
	tap_netif_hw_init();

	/* register ethernetif device */
	eth_system_device_init();
	/* initialize lwip system */
	lwip_system_init();

	netbios_init();
	rt_kprintf("TCP/IP initialized!\n");
	ASLOG(ON, ("!!!main.c lwip init end\n"));
}
#endif
#endif

#ifdef __WINDOWS__
#ifndef MINGW_HAS_SECURE_API
#include <time.h>
int gmtime_s(struct tm* now,time_t * t)
{
	struct tm* lt = localtime(t);
	memcpy(now,lt,sizeof(struct tm));

	return 0;
}
#endif
#endif
#ifdef __TERMUX__
int _isnan()
{
	return 0;
}
void _finite()
{
}
#endif
