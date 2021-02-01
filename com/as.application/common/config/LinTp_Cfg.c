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
#include "LinIf.h"
#include "CanTp.h"
/* ============================ [ MACROS    ] ====================================================== */
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */

#ifdef USE_LINTPGW
static uint8_t LinTpGw_rxBuffer[512];
static const PduInfoType LinTpGw_PduInfo =
{
	(uint8*)LinTpGw_rxBuffer, sizeof(LinTpGw_rxBuffer)
};

static const LinTpGw_InstanceConfigType LinTpGw_InstanceConfig[] = {
	{
		&LinTpGw_PduInfo,
		CANTP_ID_TxCan2LinDiag,
		LINIF_SCH_TABLE_APPLICATIVE
	}
};

static LinTpGw_RuntimeType LinTpGw_Runtime[] = {
	{ 0, 0, LINTPGW_BUFFER_IDLE }
};

const LinTpGw_ConfigType LinTpGw_Config = {
	LinTpGw_InstanceConfig,
	LinTpGw_Runtime,
	ARRAY_SIZE(LinTpGw_InstanceConfig)
};
#endif
static const LinTp_TxPduConfigType LinTp_TxPduConfigs[] = {
	{
		0,
#ifndef USE_BSWM
		LINIF_SCH_TABLE_DIAG_REQUEST,
#endif
		0x11,
	}
};
#ifdef __WINDOWS__
/* my windows LIN Bus simulator real time ability is too bad */
#define LINTP_TIMIEOUT_MS 1000
#else
#define LINTP_TIMIEOUT_MS 100
#endif
static const LinTp_RxPduConfigType LinTp_RxPduConfigs[] = {
	{
		0,
#ifndef USE_BSWM
		LINIF_SCH_TABLE_DIAG_RESPONSE,
#endif
		0x11,
		LINIF_CONVERT_MS_TO_MAIN_CYCLES(LINTP_TIMIEOUT_MS),
	}
};

static LinTp_ContextType LinTp_Context[] = {
	{{NULL, 0}, 0, 0, 0},
};

const LinTp_ConfigType LinTp_Config = {
	LinTp_TxPduConfigs,
	ARRAY_SIZE(LinTp_TxPduConfigs),
	LinTp_RxPduConfigs,
	ARRAY_SIZE(LinTp_RxPduConfigs),
	LinTp_Context
};
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
