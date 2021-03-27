/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2018  AS <parai@foxmail.com>
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
#ifndef _SD_CFG_H_
#define _SD_CFG_H_
/* ============================ [ INCLUDES  ] ====================================================== */
#include "Std_Types.h"
#include "TcpIp.h"
#include "SD_Types.h"
/* ============================ [ MACROS    ] ====================================================== */
#define SD_NUMBER_OF_INSTANCES 1
#define SD_VERSION_INFO_API STD_ON
#define SD_DEV_ERROR_DETECT STD_ON
#define SD_MAIN_FUNCTION_CYCLE_TIME_MS 10

#define SchM_Enter_SD_EA_0() Irq_Disable()
#define SchM_Exit_SD_EA_0() Irq_Enable()

#define SD_RX_UNICAST_PDUID   0
#define SD_RX_MULTICAST_PDUID 1

#define SD_CLIENT_SERVICE_SAMPLE1 0
#define SD_CLIENT_SERVICE_SAMPLE2 1

#define SD_SERVER_SERVICE_SAMPLE1 0

#define SD_SERVER_SERVICE_EVENT_SAMPLE1 0
/* ============================ [ TYPES     ] ====================================================== */

/* ============================ [ DECLARES  ] ====================================================== */
/* ============================ [ DATAS     ] ====================================================== */
extern const Sd_ConfigType Sd_Config;
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
/** @req SWS_BswM_00204 */
void BswM_Sd_ClientServiceCurrentState(uint16 SdClientServiceHandleId,
                                       Sd_ClientServiceCurrentStateType CurrentClientState);

/** @req SWS_BswM_00207 */
void BswM_Sd_ConsumedEventGroupCurrentState(uint16 SdConsumedEventGroupHandleId,
                                            Sd_ConsumedEventGroupCurrentStateType ConsumedEventGroupState);

/** @req SWS_BswM_00210 */
void BswM_Sd_EventHandlerCurrentState(uint16 SdEventHandlerHandleId,
                                      Sd_EventHandlerCurrentStateType EventHandlerStatus);
#endif /* _SD_CFG_H_ */
