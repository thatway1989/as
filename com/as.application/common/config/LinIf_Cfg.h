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
#ifndef _LINIF_CFG_H_
#define _LINIF_CFG_H_
/* ============================ [ INCLUDES  ] ====================================================== */
#include "Std_Types.h"
#include "Lin_Cfg.h"
#include "LinIf_Types.h"
#include "SchM_cfg.h"
/* ============================ [ MACROS    ] ====================================================== */
#define LINIF_CONVERT_MS_TO_MAIN_CYCLES(x)     MIAN_FUNCTION_MS2TICKS(LINIF,x)

#define LINIF_CONTROLLER_CNT 1

#define LINIF_FRAME_REF_10 0
#define LINIF_FRAME_REF_20 1
#define LINIF_FRAME_NUM 2

#define LINIF_SCHEDULE_TABLE_1 0
#define LINIF_SCHEDULE_TABLE_NUM 1
/* ============================ [ TYPES     ] ====================================================== */
/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
extern const LinIf_ChannelConfigType LinIfChannelCfg[LINIF_CONTROLLER_CNT];
extern const LinIf_FrameType LinIfFrameCfg[LINIF_FRAME_NUM];
extern const LinIf_ScheduleTableType LinIfScheduleTableCfg[LINIF_SCHEDULE_TABLE_NUM];
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
#endif /* _LINIF_CFG_H_ */
