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
/* ============================ [ MACROS    ] ====================================================== */
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */

#ifdef USE_LINTPGW
static uint8_t rxBuffer[512];
static uint8_t txBuffer[128];
static const PduInfoType rxPduInfo =
{
	(uint8*)rxBuffer, sizeof(rxBuffer)
};

static const PduInfoType txPduInfo =
{
	(uint8*)txBuffer, sizeof(txBuffer)
};

static const LinTpGw_InstanceConfigType LinTpGw_InstanceConfig[] = {
	{
		&rxPduInfo,
		&txPduInfo
	}
};

static LinTpGw_RuntimeType LinTpGw_Runtime[] = {
	{
		LINTPGW_BUFFER_IDLE,
		LINTPGW_BUFFER_IDLE
	}
};

const LinTpGw_ConfigType LinTpGw_Config = {
	LinTpGw_InstanceConfig,
	LinTpGw_Runtime,
	ARRAY_SIZE(LinTpGw_InstanceConfig)
};
#endif
static LinTp_ContextType LinTp_TxContext;
static const LinTp_TxPduConfigType LinTp_TxPduConfigs[] = {
	{
		0,
		&LinTp_TxContext,
#ifndef USE_BSWM
		LINIF_SCH_TABLE_DIAG_REQUEST
#endif
	}
};
static LinTp_ContextType LinTp_TxContext;
static const LinTp_RxPduConfigType LinTp_RxPduConfigs[] = {
	{
		0,
		&LinTp_TxContext,
#ifndef USE_BSWM
		LINIF_SCH_TABLE_DIAG_RESPONSE
#endif
	}
};

const LinTp_ConfigType LinTp_Config = {
	LinTp_TxPduConfigs,
	ARRAY_SIZE(LinTp_TxPduConfigs),
	LinTp_RxPduConfigs,
	ARRAY_SIZE(LinTp_RxPduConfigs),
};
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
