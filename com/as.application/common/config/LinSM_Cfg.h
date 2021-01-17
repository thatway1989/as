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
#ifndef _LINSM_CFG_H_
#define _LINSM_CFG_H_
/* ============================ [ INCLUDES  ] ====================================================== */
#include "SchM_cfg.h"

/* ============================ [ MACROS    ] ====================================================== */
#define LINSM_CONVERT_MS_TO_MAIN_CYCLES(x)     MIAN_FUNCTION_MS2TICKS(LINSM,x)
#define LINSM_WAKEUP_TIMEOUT LINSM_CONVERT_MS_TO_MAIN_CYCLES(1000)
#define LINSM_SCHEDULE_REQUEST_TIMEOUT LINSM_CONVERT_MS_TO_MAIN_CYCLES(10000)
#define LINSM_GOTO_SLEEP_TIMEOUT LINSM_CONVERT_MS_TO_MAIN_CYCLES(5000)
/* ============================ [ TYPES     ] ====================================================== */
typedef struct {
	uint32 LinSMRxPduGroupRef;
	uint32 LinSMTxPduGroupRef;
} LinSM_ScheduleConfigType;
typedef struct {
	LinSM_ScheduleConfigType *LinSMSchedule;
} LinSM_ChannelConfigType;
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
extern const LinSM_ChannelConfigType LinSMChannelType[];
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
#endif /* _LINSM_CFG_H_ */
