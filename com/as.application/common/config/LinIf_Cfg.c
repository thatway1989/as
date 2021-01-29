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
#include "Lin.h"
#include "PduR.h"
/* ============================ [ MACROS    ] ====================================================== */
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
const LinIf_ChannelConfigType LinIfChannelCfg[LINIF_CONTROLLER_CNT] = {
		LIN_CHANNEL_0,
		&LinChannelConfig[LIN_CHANNEL_0]
};

const LinIf_FrameType LinIfFrameCfg[] = {
	{ UNCONDITIONAL, ENHANCED, LinIfTxPdu, 0x10, 8, PDUR_ID2_LIN_TX_MSG1 },
	{ UNCONDITIONAL, ENHANCED, LinIfRxPdu, 0x20, 8, PDUR_ID_LIN_RX_MSG1 },
	{ UNCONDITIONAL, ENHANCED, LinIfTxPdu, 0x3C, 8, PDUR_ID2_TxLinDiag },
	{ UNCONDITIONAL, ENHANCED, LinIfRxPdu, 0x3D, 8, PDUR_ID_RxLinDiag },
};

static const LinIf_EntryType LinIfEntryTableApp[] = {
	{ LINIF_ID_LIN_TX_MSG1, LINIF_CONVERT_MS_TO_MAIN_CYCLES(1000) },
	{ LINIF_ID_LIN_RX_MSG1, LINIF_CONVERT_MS_TO_MAIN_CYCLES(1000) },
};

static const LinIf_EntryType LinIfEntryTableDiagRequest[] = {
	{ LINIF_ID_TxLinDiag, LINIF_CONVERT_MS_TO_MAIN_CYCLES(20) },
};

static const LinIf_EntryType LinIfEntryTableDiagResponse[] = {
	{ LINIF_ID_RxLinDiag, LINIF_CONVERT_MS_TO_MAIN_CYCLES(20) },
};

const LinIf_ScheduleTableType LinIfScheduleTableCfg[] = {
	{ LinIfEntryTableApp, ARRAY_SIZE(LinIfEntryTableApp) },
	{ LinIfEntryTableDiagRequest, ARRAY_SIZE(LinIfEntryTableDiagRequest) },
	{ LinIfEntryTableDiagResponse, ARRAY_SIZE(LinIfEntryTableDiagResponse) },
};
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
