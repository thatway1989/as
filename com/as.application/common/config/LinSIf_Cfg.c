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
#include "LinSIf.h"
#include "PduR.h"
/* ============================ [ MACROS    ] ====================================================== */
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
static const LinSIf_FrameType LinSIf_Channel0Frmes[] = {
	{ LINSIF_ENHANCED, LinSIfTxPdu, 0x3D, 8, PDUR_ID2_TxLinDiag },
	{ LINSIF_ENHANCED, LinSIfRxPdu, 0x3C, 8, PDUR_ID_RxLinDiag },
};
static const LinSIf_ChannelConfigType LinSIf_ChannelConfigs[] = {
	{LinSIf_Channel0Frmes, ARRAY_SIZE(LinSIf_Channel0Frmes)},
};
const LinSIf_ConfigType LinSIf_Config = {
	LinSIf_ChannelConfigs
};
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
