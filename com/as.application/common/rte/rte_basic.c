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
#include "Std_Types.h"
#include "Os.h"
/* ============================ [ MACROS    ] ====================================================== */

/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
#ifdef USE_RTE_SWC_TELLTALE
extern void Swc_TelltaleManager(void);
extern void Telltale_run(void);
#endif

#ifdef USE_RTE_SWC_GAUGE
extern void Gauge_Run(void);
#endif

#ifdef USE_RTE_SWC_LINAPP
extern void LinApp_Init(void);
#endif
/* ============================ [ DATAS     ] ====================================================== */
#ifdef USE_RTE_SWC_TELLTALE
TimerType timerTelltaleRun = 0u;
#endif
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
TASK(Rte_Runnable)
{	/* period is 10ms */
#ifdef USE_RTE_SWC_TELLTALE
	timerTelltaleRun ++;
	if(timerTelltaleRun >= 2)
	{
		Telltale_run();
		timerTelltaleRun = 0;
	}
	Swc_TelltaleManager();
#endif
#ifdef USE_RTE_SWC_GAUGE
	Gauge_Run();
#endif
	OsTerminateTask(Rte_Runnable);
}
ALARM(Alarm_Rte_Runnable)
{
	OsActivateTask(Rte_Runnable);
}

KSM(RteIdle,Init)
{
	KGS(RteIdle,Start);
}
KSM(RteIdle,Start)
{
	#ifdef USE_RTE_SWC_LINAPP
	LinApp_Init();
	#endif
	OsSetRelAlarm(Alarm_Rte_Runnable, 1, MS2TICKS(10));
	KGS(RteIdle,Running);
}
KSM(RteIdle,Stop)
{

}
KSM(RteIdle,Running)
{
}
