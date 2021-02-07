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
#include "LinSTp.h"
#include "Dcm.h"
/* ============================ [ MACROS    ] ====================================================== */
#ifdef __WINDOWS__
/* my windows LIN Bus simulator real time ability is too bad */
#define LINSTP_TIMIEOUT_MS 1000
#else
#define LINSTP_TIMIEOUT_MS 100
#endif
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
static const LinSTp_ChannelConfigType LinSTp_ChannelConfigs[1] = {
	{DCM_ID_RxLinDiagTp, DCM_ID_TxLinDiagTp, LINSTP_CONVERT_MS_TO_MAIN_CYCLES(LINSTP_TIMIEOUT_MS), 0x11}
};

static LinSTp_ContextType LinSTp_Contexts[] ={
	{{NULL, 0}, 0, 0, 0, 0},
};
const LinSTp_ConfigType LinSTp_Config = {
	LinSTp_ChannelConfigs,
	ARRAY_SIZE(LinSTp_ChannelConfigs),
	LinSTp_Contexts,
};
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
