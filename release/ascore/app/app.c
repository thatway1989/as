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
#include "Os.h"
#ifdef USE_SG
#include "Sg.h"
#endif
#ifdef USE_LCD
#include "Lcd.h"
#endif
#ifdef USE_XCP
#include "Xcp.h"
#endif
#ifdef USE_FTP
#include "ftpd.h"
#endif
#if( defined(USE_PTHREAD) || defined(RT_USING_PTHREADS) || defined(__LINUX__) || defined(__WINDOWS__))
#include "pthread.h"
#include <unistd.h>
#endif
#ifdef USE_TRACE
#include "trace.h"
#endif

#ifdef USE_SD
#include "SD.h"
#endif
// #define AS_PERF_ENABLED
#include "asdebug.h"
/* ============================ [ MACROS    ] ====================================================== */
#define AS_LOG_OS 0
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
#ifdef USE_CPLUSPLUS
extern void cppapp_start(void);
#endif
/* ============================ [ DATAS     ] ====================================================== */
#ifdef USE_XCP
#if defined(__LINUX__) || defined(__WINDOWS__)
uint8_t xcpSimMTAMemory[1024];
#else
uint8_t xcpSimMTAMemory[32];
#endif
#endif

//#define PROTOTHREAD_TEST
#if defined(USE_PROTOTHREAD) && (defined(__LINUX__) || defined(__WINDOWS__))
#if defined(PROTOTHREAD_TEST)
extern struct process protoDemoProductProc;
extern struct process protoDemoConsumerProc;
#endif
#if defined(USE_UIP)
extern struct process protoUIPMain;
#endif
struct process * const autostart_processes[] = {
#if defined(PROTOTHREAD_TEST)
	&protoDemoProductProc,
	&protoDemoConsumerProc,
#endif
#if defined(USE_UIP)
	&protoUIPMain,
#endif
	NULL
};
#endif
/* ============================ [ LOCALS    ] ====================================================== */

/* ============================ [ FUNCTIONS ] ====================================================== */
void StartupHook(void)
{
	uint32 endian = 0xdeadbeef;
	printf(" start application BUILD @ %s %s\n",__DATE__,__TIME__);

	if(0xde == (*(uint8_t*)&endian))
	{
		printf(" cpu is big endian\n");
	}
	else
	{
		printf(" cpu is little endian\n");
	}

#ifdef USE_LCD
	Lcd_Init();
#endif
#ifdef USE_SG
	Sg_Init();
#endif

#ifdef USE_LVGL
    /*Initialize LittlevGL*/
	extern void lv_init(void);
	extern void lv_hw_dsp_init(void);
	extern void lv_hw_mouse_init(void);
    lv_init();
    lv_hw_dsp_init();
    lv_hw_mouse_init();
#if USE_LV_BENCHMARK
    extern void benchmark_create(void);
    benchmark_create();
#endif
#if USE_LV_DEMO
    extern void demo_create(void);
    demo_create();
#endif
#if USE_LV_SYSMON
    extern void sysmon_create(void);
    sysmon_create();
#endif
#if USE_LV_TERMINAL
    extern void terminal_create(void);
    terminal_create();
#endif
#if USE_LV_TPCAL
    extern void tpcal_create(void);
    tpcal_create();
#endif
#endif

#ifdef USE_XCP
	printf(" XCP MTA memory address %p\n",xcpSimMTAMemory);
	for(int i=0;i<sizeof(xcpSimMTAMemory);i++)
	{
		xcpSimMTAMemory[i] = i;
	}
#endif

#ifdef USE_TRACE
	Trace_Init();
#endif
}

void SchM_StartupHook(void)
{
	OsSetRelAlarm(AlarmApp, MS2TICKS(10), MS2TICKS(5));
}

#ifdef USE_LVGL
#if( (OS_PTHREAD_NUM > 0) || defined(RT_USING_PTHREADS) || defined(__LINUX__) || defined(__WINDOWS__))
extern void lv_task_handler(void);
void* lv_task_thread(void* args)
{
	while(TRUE)
	{
		lv_task_handler();
#if defined(RT_USING_PTHREADS)
		rt_thread_delay(1);
#else
		usleep(1000);
#endif
	}
	return NULL;
}
#endif
#endif

void __weak TaskAppHook(void) { }
TASK(TaskApp)
{
	OS_TASK_BEGIN();
	ASLOG(OFF,("TaskApp is running\n"));

	TaskAppHook();
#ifdef USE_SG
	ASPERF_MEASURE_START();
	Sg_ManagerTask();
	ASPERF_MEASURE_STOP("Sg_ManagerTask");
#endif
#ifdef USE_LVGL
#if( (OS_PTHREAD_NUM > 0) || defined(RT_USING_PTHREADS) || defined(__LINUX__) || defined(__WINDOWS__))
{
	static int flag = 0;
	static pthread_t lvThread;
	if(0 == flag)
	{
		(void)pthread_create(&lvThread, NULL, lv_task_thread, NULL);
		flag = 1;
	}
}

	extern void lv_tick_inc(uint32_t inc);
	lv_tick_inc(10);
#endif
#endif

#ifndef USE_STDRT
#ifdef USE_LWIP
	OsActivateTask(TaskLwip);
#endif
#endif

#ifdef USE_FTP
	{
		static int flag = 0;
		if(0 == flag)
		{
			if(0u == ftpd_init())
			{
				flag = 1;
			}
			else
			{
				/* try init ftpd next time */
			}
		}
	}
#endif

#ifdef USE_CPLUSPLUS
{
	static int flag = 0;
	if(0 == flag) {
		cppapp_start();
		flag = 1;
	}
}
#endif

#ifdef USE_SD
{
	static int flag = 0;
	if(0 == flag) {
		Sd_LocalIpAddrAssignmentChg(SOADIF_ID_SD_MULTICAST_TX,TCPIP_IPADDR_STATE_ASSIGNED);
		Sd_ClientServiceSetState(0, SD_CLIENT_SERVICE_REQUESTED);
		Sd_ServerServiceSetState(0, SD_SERVER_SERVICE_AVAILABLE);
		flag = 1;
	}
}
#endif
	OsTerminateTask(TaskApp);

	OS_TASK_END();
}

ALARM(AlarmApp)
{
	OsActivateTask(TaskApp);
}

#if defined(USE_PROTOTHREAD) && defined(PROTOTHREAD_TEST)
PROCESS(protoDemoProductProc,"protoDemoProductProc");
PROTO_AUTOSTART_PROCESS_EXPORT(protoDemoProductProc);
/* each bit means a product */
uint32_t buffer = 0;
#define NUM_ITEMS 10000

static struct pt_sem mutex, full, empty;

PROCESS_THREAD(protoDemoProductProc, ev, data)
{
	static int produced;
	static struct etimer et;
	process_poll(&protoDemoProductProc);
	PROCESS_BEGIN();
	PROCESS_SEM_INIT(&mutex, 1);
	PROCESS_SEM_INIT(&full, 0);
	PROCESS_SEM_INIT(&empty, 32);
	etimer_set(&et,OS_TICKS_PER_SECOND);
	PROCESS_YIELD();
	for(produced = 0; produced < NUM_ITEMS; ++produced) {

		PROCESS_WAIT_UNTIL(ev == PROCESS_EVENT_TIMER);
		PROCESS_SEM_WAIT(&empty);

		PROCESS_SEM_WAIT(&mutex);
		asAssert(0 == (buffer&(1<<(produced%32))));
		buffer |= 1<<(produced%32);
		printf("-->%d : %d\n", produced, produced%32);
		PROCESS_SEM_SIGNAL(&mutex);

		PROCESS_SEM_SIGNAL(&full);
	}
	PROCESS_END();
}

PROCESS(protoDemoConsumerProc,"protoDemoConsumerProc");
PROTO_AUTOSTART_PROCESS_EXPORT(protoDemoConsumerProc);

PROCESS_THREAD(protoDemoConsumerProc, ev, data)
{
	static int consumed;
	process_poll(&protoDemoConsumerProc);
	PROCESS_BEGIN();
	PROCESS_YIELD();
	for(consumed = 0; consumed < NUM_ITEMS; ++consumed) {

		PROCESS_SEM_WAIT(&full);

		PROCESS_SEM_WAIT(&mutex);
		asAssert(buffer&(1<<(consumed%32)));
		buffer &= ~(1<<(consumed%32));
		printf("<--%d : %d\n", consumed, consumed%32);
		PROCESS_SEM_SIGNAL(&mutex);

		PROCESS_SEM_SIGNAL(&empty);
	}
	PROCESS_END();
}
#endif

void ErrorHook(StatusType ercd)
{

	switch(ercd)
	{
		case E_OS_ACCESS:
			ASLOG(OS,("ercd = %d E_OS_ACCESS!\r\n",ercd));
			break;
		case E_OS_CALLEVEL:
			ASLOG(OS,("ercd = %d E_OS_CALLEVEL!\r\n",ercd));
			break;
		case E_OS_ID:
			ASLOG(OS,("ercd = %d E_OS_ID!\r\n",ercd));
			break;
		case E_OS_LIMIT:
			ASLOG(OS,("ercd = %d E_OS_LIMIT!\r\n",ercd));
			break;
		case E_OS_NOFUNC:
			ASLOG(OS,("ercd = %d E_OS_NOFUNC!\r\n",ercd));
			break;
		case E_OS_RESOURCE:
			ASLOG(OS,("ercd = %d E_OS_RESOURCE!\r\n",ercd));
			break;
		case E_OS_STATE:
			ASLOG(OS,("ercd = %d E_OS_STATE!\r\n",ercd));
			break;
		case E_OS_VALUE	:
			ASLOG(OS,("ercd = %d E_OS_VALUE!\r\n",ercd));
			break;
		default:
			ASLOG(OS,("ercd = %d unknown error!\r\n",ercd));
			break;
	}

	if(E_OS_CALLEVEL == ercd)
	{
		ShutdownOS(E_OS_CALLEVEL);
	}
	else
	{
		/* recover-able error */
	}
}

void PreTaskHook(void)
{
#ifdef USE_TRACE
	TaskType TaskID;
	GetTaskID(&TaskID);
	Trace_PerfStart(TaskID);
#endif
}
void PostTaskHook(void)
{
#ifdef USE_TRACE
	TaskType TaskID;
	GetTaskID(&TaskID);
	Trace_PerfStop(TaskID);
#endif
}

void ShutdownHook(StatusType ercd)
{
	printf("ShutdownHook(%X)\n",ercd);

	asAssert(0);
}

#if defined(__TOPPERS_ATK2_SC4__)
ProtectionReturnType ProtectionHook(StatusType FatalError)
{
	printf("ProtectionHook(%X)\n",FatalError);

	return PRO_SHUTDOWN;
}
#endif

void __error__(char *pcFilename, unsigned long ulLine)
{
	printf(" error @ %d of %s\n",(int)ulLine,pcFilename);
}

#if 0
#ifndef USE_STDRT
void abort(void) { printf("%s\n",__func__); asAssert(0); while(1); }
#endif
void _sbrk(void) { printf("%s\n",__func__); asAssert(0); }
void _write(void) { printf("%s\n",__func__); asAssert(0); }
void _close(void) { printf("%s\n",__func__); asAssert(0); }
void _fstat(void) { printf("%s\n",__func__); asAssert(0); }
void _isatty(void) { printf("%s\n",__func__); asAssert(0); }
void _lseek(void) { printf("%s\n",__func__); asAssert(0); }
void _read(void) { printf("%s\n",__func__); asAssert(0); }
#endif
