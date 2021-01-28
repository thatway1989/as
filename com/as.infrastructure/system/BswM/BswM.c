/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2016  AS <parai@foxmail.com>
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
#include "BswM.h"
#ifdef USE_LINTP
#include "LinIf.h"
#ifdef USE_LINSM
#include "LinSM.h"
#endif
#endif
/* ============================ [ MACROS    ] ====================================================== */

/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
#ifdef USE_LINTP
void BswM_LinTp_RequestMode(NetworkHandleType Network, LinTp_Mode LinTpRequestedMode)
{
#ifdef USE_LINSM
	switch(LinTpRequestedMode) {
		case LINTP_APPLICATIVE_SCHEDULE:
			LinSM_ScheduleRequest(Network, LINIF_SCH_TABLE_APPLICATIVE);
			break;
		case LINTP_DIAG_REQUEST:
			LinSM_ScheduleRequest(Network, LINIF_SCH_TABLE_DIAG_REQUEST);
			break;
		case LINTP_DIAG_RESPONSE:
			LinSM_ScheduleRequest(Network, LINIF_SCH_TABLE_DIAG_RESPONSE);
			break;
	}
#else
	switch(LinTpRequestedMode) {
		case LINTP_APPLICATIVE_SCHEDULE:
			LinIf_ScheduleRequest(Network, LINIF_SCH_TABLE_APPLICATIVE);
			break;
		case LINTP_DIAG_REQUEST:
			LinIf_ScheduleRequest(Network, LINIF_SCH_TABLE_DIAG_REQUEST);
			break;
		case LINTP_DIAG_RESPONSE:
			LinIf_ScheduleRequest(Network, LINIF_SCH_TABLE_DIAG_RESPONSE);
			break;
	}
#endif
}
#endif
