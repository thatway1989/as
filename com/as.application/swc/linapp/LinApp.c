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
#include "asdebug.h"
#include "LinIf.h"
#ifdef USE_LINSM
#include "LinSM.h"
#include "LinSM_Cbk.h"
#endif
#include "Rte_LinApp.h"
/* ============================ [ MACROS    ] ====================================================== */
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
void LinApp_Init(void)
{
#ifdef USE_LINSM
	LinSM_RequestComMode(0, COMM_FULL_COMMUNICATION);
	LinSM_WakeUp_Confirmation(0, TRUE);
#endif
	LinIf_ScheduleRequest(0, 0);
}

void LinApp_Exit(void)
{

}

void LinApp_Run(void)
{

}
