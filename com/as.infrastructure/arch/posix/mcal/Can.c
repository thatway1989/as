/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2015  AS <parai@foxmail.com>
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
#include "Os.h"
#include "Can.h"
#include "Mcu.h"
#include "CanIf_Cbk.h"
#if defined(USE_DET)
#include "Det.h"
#endif
#if defined(USE_DEM)
#include "Dem.h"
#endif
#include <pthread.h>
#include <sys/queue.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "asdebug.h"
#ifdef __AS_CAN_BUS__
#include "lascanlib.h"
#else
#include "RPmsg.h"
#endif
/* ============================ [ MACROS    ] ====================================================== */
#define USE_CAN_STATISTICS      STD_ON
#define AS_LOG_CAN 0

#ifndef CAN_DEV_NAME
#define CAN_DEV_NAME "socket"
#endif

/* CONFIGURATION NOTES
 * ------------------------------------------------------------------
 * - CanHandleType must be CAN_ARC_HANDLE_TYPE_BASIC
 *   i.e. CanHandleType=CAN_ARC_HANDLE_TYPE_FULL NOT supported
 *   i.e CanIdValue is NOT supported
 * - All CanXXXProcessing must be CAN_ARC_PROCESS_TYPE_INTERRUPT
 *   ie CAN_ARC_PROCESS_TYPE_POLLED not supported
 * - HOH's for Tx are global and Rx are for each controller
 */

/* IMPLEMENTATION NOTES
 * -----------------------------------------------
 * - A HOH us unique for a controller( not a config-set )
 * - Hrh's are numbered for each controller from 0
 * - Only one transmit mailbox is used because otherwise
 *   we cannot use tx_confirmation since there is no way to know
 *   which mailbox caused the tx interrupt. TP will need this feature.
 * - Sleep,wakeup not fully implemented since other modules lack functionality
 */

/* ABBREVATIONS
 *  -----------------------------------------------
 * - Can Hardware unit - One or multiple Can controllers of the same type.
 * - Hrh - HOH with receive definitions
 * - Hth - HOH with transmit definitions
 *
 */


#define GET_CONTROLLER_CONFIG(_controller)	\
        					&Can_Global.config->CanConfigSet->CanController[(_controller)]

#define GET_CALLBACKS() \
							(Can_Global.config->CanConfigSet->CanCallbacks)

#define GET_PRIVATE_DATA(_controller) \
									&CanUnit[_controller]

#define GET_CONTROLLER_CNT() (CAN_CONTROLLER_CNT)


#if ( CAN_DEV_ERROR_DETECT == STD_ON )
#define VALIDATE(_exp,_api,_err ) \
        if( !(_exp) ) { \
          Det_ReportError(MODULE_ID_CAN,0,_api,_err); \
          return CAN_NOT_OK; \
        }

#define VALIDATE_NO_RV(_exp,_api,_err ) \
        if( !(_exp) ) { \
          Det_ReportError(MODULE_ID_CAN,0,_api,_err); \
          return; \
        }

#define DET_REPORTERROR(_x,_y,_z,_q) Det_ReportError(_x, _y, _z, _q)
#else
#define VALIDATE(_exp,_api,_err )
#define VALIDATE_NO_RV(_exp,_api,_err )
#define DET_REPORTERROR(_x,_y,_z,_q)
#endif

#if defined(USE_DEM)
#define VALIDATE_DEM_NO_RV(_exp,_err ) \
        if( !(_exp) ) { \
          Dem_ReportErrorStatus(_err, DEM_EVENT_STATUS_FAILED); \
          return; \
        }
#else
#define VALIDATE_DEM_NO_RV(_exp,_err )
#endif

#define CAN_EMPTY_MESSAGE_BOX 0xFFFF
/* ============================ [ TYPES     ] ====================================================== */

typedef enum
{
  CAN_UNINIT = 0,
  CAN_READY
} Can_DriverStateType;

// Mapping between HRH and Controller//HOH
typedef struct Can_Arc_ObjectHOHMapStruct
{
  CanControllerIdType CanControllerRef;    // Reference to controller
  const Can_HardwareObjectType* CanHOHRef;       // Reference to HOH.
} Can_Arc_ObjectHOHMapType;

/* Type for holding global information used by the driver */
typedef struct {
  Can_DriverStateType initRun;

  // Our config
  const Can_ConfigType *config;

  // One bit for each channel that is configured.
  // Used to determine if validity of a channel
  // 1 - configured
  // 0 - NOT configured
  uint32  configured;
  // Maps the a channel id to a configured channel id
  uint8   channelMap[CAN_CONTROLLER_CNT];

  // This is a map that maps the HTH:s with the controller and Hoh. It is built
  // during Can_Init and is used to make things faster during a transmit.
  Can_Arc_ObjectHOHMapType CanHTHMap[NUM_OF_HTHS];
} Can_GlobalType;

#ifndef __AS_CAN_BUS__
struct Can_RPmsgPud_s {
	Can_RPmsgPduType msg;
	STAILQ_ENTRY(Can_RPmsgPud_s) pduEntry;
};

struct Can_RPmsgPduQueue_s {
	pthread_mutex_t w_lock;
	STAILQ_HEAD(,Can_RPmsgPud_s) pduHead;
};
#endif
/* Type for holding information about each controller */
typedef struct {
  CanIf_ControllerModeType state;
  uint8		lock_cnt;

  // Statistics
#if (USE_CAN_STATISTICS == STD_ON)
    Can_Arc_StatisticsType stats;
#endif

  // Data stored for Txconfirmation callbacks to CanIf
  PduIdType swPduHandle; //
#ifndef __AS_CAN_BUS__
  struct Can_RPmsgPduQueue_s rQ;
#endif
} Can_UnitType;

/* ============================ [ DATAS     ] ====================================================== */
Can_UnitType CanUnit[CAN_CONTROLLER_CNT] =
{
  {
    .state = CANIF_CS_UNINIT,
	#ifndef __AS_CAN_BUS__
	.rQ.w_lock = PTHREAD_MUTEX_INITIALIZER
	#endif
  },
  {
    .state = CANIF_CS_UNINIT,
	#ifndef __AS_CAN_BUS__
	.rQ.w_lock = PTHREAD_MUTEX_INITIALIZER
	#endif
  },
  {
    .state = CANIF_CS_UNINIT,
	#ifndef __AS_CAN_BUS__
	.rQ.w_lock = PTHREAD_MUTEX_INITIALIZER
	#endif
  },
  {
    .state = CANIF_CS_UNINIT,
	#ifndef __AS_CAN_BUS__
	.rQ.w_lock = PTHREAD_MUTEX_INITIALIZER
	#endif
  },
  {
    .state = CANIF_CS_UNINIT,
	#ifndef __AS_CAN_BUS__
	.rQ.w_lock = PTHREAD_MUTEX_INITIALIZER
	#endif
  },
};
// Global config
Can_GlobalType Can_Global =
{
  .initRun = CAN_UNINIT,
};
/* ============================ [ DECLARES  ] ====================================================== */
#ifdef __AS_CAN_BUS__
extern int can_open(unsigned long busid,const char* device_name,unsigned long port, unsigned long baudrate);
extern int can_write(unsigned long busid,unsigned long canid,unsigned long dlc,unsigned char* data);
extern int can_read(unsigned long busid,unsigned long canid,unsigned long* p_canid,unsigned long *dlc,unsigned char* data);
extern void luai_canlib_open(void);
extern void luai_canlib_close(void);
#endif

extern void Can_SimulatorRunning(void);
/* ============================ [ LOCALS    ] ====================================================== */
/* ============================ [ FUNCTIONS ] ====================================================== */
/**
 * Function that finds the Hoh( HardwareObjectHandle ) from a Hth
 * A HTH may connect to one or several HOH's. Just find the first one.
 *
 * @param hth The transmit handle
 * @returns Ptr to the Hoh
 */
static const Can_HardwareObjectType * Can_FindHoh( Can_Arc_HTHType hth , uint32* controller)
{
  const Can_HardwareObjectType *hohObj;
  const Can_Arc_ObjectHOHMapType *map;

  map = &Can_Global.CanHTHMap[hth];

  // Verify that this is the correct map
  if (map->CanHOHRef->CanObjectId != hth)
  {
    DET_REPORTERROR(MODULE_ID_CAN, 0, 0x6, CAN_E_PARAM_HANDLE);
  }
  hohObj = map->CanHOHRef;

  // Verify that this is the correct Hoh type
  if ( hohObj->CanObjectType == CAN_OBJECT_TYPE_TRANSMIT)
  {
    *controller = map->CanControllerRef;
    return hohObj;
  }

  DET_REPORTERROR(MODULE_ID_CAN, 0, 0x6, CAN_E_PARAM_HANDLE);

  return NULL;
}

// This initiates ALL can controllers
void Can_Init( const Can_ConfigType *config ) {
  Can_UnitType *canUnit;
  const Can_ControllerConfigType *canHwConfig;
  uint8 ctlrId;

  VALIDATE_NO_RV( (Can_Global.initRun == CAN_UNINIT), 0x0, CAN_E_TRANSITION );
  VALIDATE_NO_RV( (config != NULL ), 0x0, CAN_E_PARAM_POINTER );

  // Save config
  Can_Global.config = config;
  Can_Global.initRun = CAN_READY;

	#ifdef __AS_CAN_BUS__
	luai_canlib_open();
	#endif

  for (int configId=0; configId < CAN_CTRL_CONFIG_CNT; configId++) {
    canHwConfig = GET_CONTROLLER_CONFIG(configId);
    ctlrId = canHwConfig->CanControllerId;

    // Assign the configuration channel used later..
    Can_Global.channelMap[canHwConfig->CanControllerId] = configId;
    Can_Global.configured |= (1<<ctlrId);

    canUnit = GET_PRIVATE_DATA(ctlrId);
    canUnit->state = CANIF_CS_STOPPED;

    canUnit->lock_cnt = 0;
    canUnit->swPduHandle = CAN_EMPTY_MESSAGE_BOX;	/* 0xFFFF marked as Empty and invalid */
	#ifndef __AS_CAN_BUS__
    (void)pthread_mutex_lock(&canUnit->rQ.w_lock);
	STAILQ_INIT(&canUnit->rQ.pduHead);
	(void)pthread_mutex_unlock(&canUnit->rQ.w_lock);
	#else
	if(FALSE == can_open(configId,CAN_DEV_NAME,ctlrId,canHwConfig->CanControllerBaudRate*1000))
	{
		ASLOG(STDOUT, ("for device <socket> please start vcan for simulation:\n\t"
				"sudo modprobe vcan\n\t"
				"sudo ip link add dev can%d type vcan\n\t"
				"sudo ip link set up can%d\n\t"
				"sudo ip link set can%d mtu 72\n", ctlrId, ctlrId, ctlrId));
		ASLOG(STDOUT, ("for device <socketwin> please start the windows socket can bus driver build/nt/socket_can_driver.exe %d\n", ctlrId));
		asAssert(0);
	}
	#endif
    // Clear stats
#if (USE_CAN_STATISTICS == STD_ON)
    memset(&canUnit->stats, 0, sizeof(Can_Arc_StatisticsType));
#endif


    Can_InitController(ctlrId, canHwConfig);

    // Loop through all Hoh:s and map them into the HTHMap
    const Can_HardwareObjectType* hoh;
    hoh = canHwConfig->Can_Arc_Hoh;
    hoh--;
    do
    {
      hoh++;

      if (hoh->CanObjectType == CAN_OBJECT_TYPE_TRANSMIT)
      {
        Can_Global.CanHTHMap[hoh->CanObjectId].CanControllerRef = canHwConfig->CanControllerId;
        Can_Global.CanHTHMap[hoh->CanObjectId].CanHOHRef = hoh;
      }
    } while (!hoh->Can_Arc_EOL);
  }
  return;
}

// Unitialize the module
void Can_DeInit()
{
  Can_UnitType *canUnit;
  const Can_ControllerConfigType *canHwConfig;
  uint32 ctlrId;

  for (int configId=0; configId < CAN_CTRL_CONFIG_CNT; configId++) {
    canHwConfig = GET_CONTROLLER_CONFIG(configId);
    ctlrId = canHwConfig->CanControllerId;

    canUnit = GET_PRIVATE_DATA(ctlrId);
    canUnit->state = CANIF_CS_UNINIT;

    Can_DisableControllerInterrupts(ctlrId);

    canUnit->lock_cnt = 0;

    // Clear stats
#if (USE_CAN_STATISTICS == STD_ON)
    memset(&canUnit->stats, 0, sizeof(Can_Arc_StatisticsType));
#endif
  }

	#ifdef __AS_CAN_BUS__
	luai_canlib_close();
	#endif

  Can_Global.config = NULL;
  Can_Global.initRun = CAN_UNINIT;

  return;
}

void Can_InitController( uint8 controller, const Can_ControllerConfigType *config)
{
  Can_UnitType *canUnit;
  uint8 cId = controller;
  const Can_ControllerConfigType *canHwConfig;
  const Can_HardwareObjectType *hohObj;

  VALIDATE_NO_RV( (Can_Global.initRun == CAN_READY), 0x2, CAN_E_UNINIT );
  VALIDATE_NO_RV( (config != NULL ), 0x2,CAN_E_PARAM_POINTER);
  VALIDATE_NO_RV( (controller < GET_CONTROLLER_CNT()), 0x2, CAN_E_PARAM_CONTROLLER );

  canUnit = GET_PRIVATE_DATA(controller);

  VALIDATE_NO_RV( (canUnit->state==CANIF_CS_STOPPED), 0x2, CAN_E_TRANSITION );

  canHwConfig = GET_CONTROLLER_CONFIG(Can_Global.channelMap[cId]);

  // Start this baby up
  // set CAN enable bit, deactivate listen-only mode,
  // use Bus Clock as clock source and select loop back mode on/off
  // acceptance filters
   hohObj = canHwConfig->Can_Arc_Hoh;
   --hohObj;
   do {
     ++hohObj;
     if (hohObj->CanObjectType == CAN_OBJECT_TYPE_RECEIVE)
     {

     }
   }while( !hohObj->Can_Arc_EOL );

  canUnit->state = CANIF_CS_STOPPED;
  Can_EnableControllerInterrupts(cId);

  return;
}


Can_ReturnType Can_SetControllerMode( uint8 controller, Can_StateTransitionType transition ) {

  Can_ReturnType rv = CAN_OK;
  VALIDATE( (controller < GET_CONTROLLER_CNT()), 0x3, CAN_E_PARAM_CONTROLLER );

  Can_UnitType *canUnit = GET_PRIVATE_DATA(controller);

  VALIDATE( (canUnit->state!=CANIF_CS_UNINIT), 0x3, CAN_E_UNINIT );
  switch(transition )
  {
	  case CAN_T_START:
		canUnit->state = CANIF_CS_STARTED;
		ASLOG(STDOUT, ("can set on-line!\n"));
		if (canUnit->lock_cnt == 0){   // REQ CAN196
		  Can_EnableControllerInterrupts(controller);
		}
		break;
	  case CAN_T_WAKEUP:
		VALIDATE(canUnit->state == CANIF_CS_SLEEP, 0x3, CAN_E_TRANSITION);
		canUnit->state = CANIF_CS_STOPPED;
		break;
	  case CAN_T_SLEEP:  //CAN258, CAN290
		// Should be reported to DEM but DET is the next best
		VALIDATE(canUnit->state == CANIF_CS_STOPPED, 0x3, CAN_E_TRANSITION);
		canUnit->state = CANIF_CS_SLEEP;
		break;
	  case CAN_T_STOP:
		// Stop
		canUnit->state = CANIF_CS_STOPPED;
		break;
	  default:
		// Should be reported to DEM but DET is the next best
		VALIDATE(canUnit->state == CANIF_CS_STOPPED, 0x3, CAN_E_TRANSITION);
		break;
  }
  return rv;
}

void Can_DisableControllerInterrupts( uint8 controller )
{
  Can_UnitType *canUnit;

  VALIDATE_NO_RV( (controller < GET_CONTROLLER_CNT()), 0x4, CAN_E_PARAM_CONTROLLER );

  canUnit = GET_PRIVATE_DATA(controller);

  VALIDATE_NO_RV( (canUnit->state!=CANIF_CS_UNINIT), 0x4, CAN_E_UNINIT );

  if(canUnit->lock_cnt > 0 )
  {
    // Interrupts already disabled
    canUnit->lock_cnt++;
    return;
  }
  canUnit->lock_cnt++;

 }

void Can_EnableControllerInterrupts( uint8 controller ) {
  Can_UnitType *canUnit;
  VALIDATE_NO_RV( (controller < GET_CONTROLLER_CNT()), 0x5, CAN_E_PARAM_CONTROLLER );

  canUnit = GET_PRIVATE_DATA(controller);

  VALIDATE_NO_RV( (canUnit->state!=CANIF_CS_UNINIT), 0x5, CAN_E_UNINIT );

  if( canUnit->lock_cnt > 1 )
  {
    // IRQ should still be disabled so just decrement counter
    canUnit->lock_cnt--;
    return;
  } else if (canUnit->lock_cnt == 1)
  {
    canUnit->lock_cnt = 0;
  }

  return;
}

Can_ReturnType Can_Write( Can_Arc_HTHType hth, Can_PduType *pduInfo ) {
  Can_ReturnType rv = CAN_OK;
  const Can_HardwareObjectType *hohObj;
  uint32 controller;
  imask_t irq_state;
  uint8_t busid;

  VALIDATE( (Can_Global.initRun == CAN_READY), 0x6, CAN_E_UNINIT );
  VALIDATE( (pduInfo != NULL), 0x6, CAN_E_PARAM_POINTER );
  VALIDATE( (pduInfo->length <= 64), 0x6, CAN_E_PARAM_DLC );
  VALIDATE( (hth < NUM_OF_HTHS ), 0x6, CAN_E_PARAM_HANDLE );

  hohObj = Can_FindHoh(hth, &controller);
  if (hohObj == NULL)
  {
    return CAN_NOT_OK;
  }

  Can_UnitType *canUnit = GET_PRIVATE_DATA(controller);

  #ifndef __AS_CAN_BUS__
  while(FALSE == RPmsg_IsOnline())
  {
	  return CAN_NOT_OK;;	/* make sure rpmsg is online */
  }
  #endif

  for (busid=0; busid < CAN_CTRL_CONFIG_CNT; busid++)
  {
	  if(controller==Can_Global.config->CanConfigSet->CanController->CanControllerId)
	  {
		  break;
	  }
  }
  if(busid >= CAN_CTRL_CONFIG_CNT)
  {
	  return CAN_NOT_OK;
  }
  if(CANIF_CS_STARTED == canUnit->state)
  {
	  Irq_Save(irq_state);
	  if(CAN_EMPTY_MESSAGE_BOX == canUnit->swPduHandle)	/* check for any free box */
	  {
		#ifndef __AS_CAN_BUS__
		  Can_RPmsgPduType rpmsg;
		  Std_ReturnType ercd;
		  rpmsg.bus = busid;
		  rpmsg.id = pduInfo->id;
		  rpmsg.length = pduInfo->length;
		  memcpy(rpmsg.sdu,pduInfo->sdu,pduInfo->length);
		  do {
			ercd = RPmsg_Send(RPMSG_CHL_CAN,&rpmsg,sizeof(rpmsg));
		  } while(ercd != E_OK);
		#else

		  if(FALSE == can_write(busid,pduInfo->id,pduInfo->length,pduInfo->sdu))
		  {
			  asAssert(0);
		  }
		#endif	/* __AS_CAN_BUS__ */

		  ASLOG(CAN, ("CAN ID=0x%08X LEN=%d DATA=[%02X %02X %02X %02X %02X %02X %02X %02X]\n", 
				  pduInfo->id,pduInfo->length,pduInfo->sdu[0],pduInfo->sdu[1],pduInfo->sdu[2],pduInfo->sdu[3],
				  pduInfo->sdu[4],pduInfo->sdu[5],pduInfo->sdu[6],pduInfo->sdu[7]));

		  canUnit->swPduHandle = pduInfo->swPduHandle;
		  // Increment statistics
		  #if (USE_CAN_STATISTICS == STD_ON)
		  canUnit->stats.txSuccessCnt++;
		  #endif
	  }
	  else
	  {
		  rv = CAN_BUSY;
	  }
	  Irq_Restore(irq_state);
  } else {
    rv = CAN_NOT_OK;
  }

  return rv;
}

void Can_MainFunction_Read( void ) {
	#ifdef __AS_CAN_BUS__
	unsigned long busid,canid,dlc;
	unsigned char data[64];
	int ercd;

	for(busid=0;busid<CAN_CTRL_CONFIG_CNT;busid++)
	{
		ercd = can_read(busid,-1,&canid,&dlc,data);
		if(TRUE == ercd)
		{
			uint16 Hrh = 0xFFFF;

			const Can_HardwareObjectType  *hoh = Can_Global.config->CanConfigSet->CanController[busid].Can_Arc_Hoh;
			hoh --;
			do{
				hoh ++;
				if(CAN_OBJECT_TYPE_RECEIVE == hoh->CanObjectType)
				{
					Hrh = hoh->CanObjectId;
					break;
				}
			}while(FALSE == hoh->Can_Arc_EOL);
			asAssert(0xFFFF != Hrh);
			asAssert(Can_Global.config->CanConfigSet->CanCallbacks->RxIndication);
			Can_Global.config->CanConfigSet->CanCallbacks->RxIndication(Hrh,(Can_IdType)canid,(uint8)dlc,(uint8*)data);
		}
	}
	#endif
}

void Can_MainFunction_BusOff( void ) {
  /* Bus-off polling events */

	/* NOT SUPPORTED */
}

void Can_MainFunction_Wakeup( void ) {
  /* Wakeup polling events */

	/* NOT SUPPORTED */
}


void Can_MainFunction_Write( void ) {
	Can_SimulatorRunning();
}

void Can_MainFunction_Error( void ) {
    /* NOT SUPPORTED */
}


/**
 * Get send/receive/error statistics for a controller
 *
 * @param controller The controller
 * @param stats Pointer to data to copy statistics to
 */

#if (USE_CAN_STATISTICS == STD_ON)
void Can_Arc_GetStatistics( uint8 controller, Can_Arc_StatisticsType *stats)
{
  Can_UnitType *canUnit = GET_PRIVATE_DATA(controller);
  *stats = canUnit->stats;
}
#endif
/*
 * Called by KSM to simulate CAN TX ISR and RX ISR
 */
void Can_SimulatorRunning(void)
{
	Can_UnitType *canUnit;
	const Can_ControllerConfigType *canHwConfig;
	uint8 ctlrId;
	if(Can_Global.initRun == CAN_READY)
	{
		/* Tx Confirmation Process*/
		for (int configId=0; configId < CAN_CTRL_CONFIG_CNT; configId++)
		{
			canHwConfig = GET_CONTROLLER_CONFIG(configId);
			ctlrId = canHwConfig->CanControllerId;

			canUnit = GET_PRIVATE_DATA(ctlrId);
			if(CAN_EMPTY_MESSAGE_BOX != canUnit->swPduHandle)
			{
				PduIdType swPduHandle = canUnit->swPduHandle;
				canUnit->swPduHandle = CAN_EMPTY_MESSAGE_BOX;
				if(NULL != Can_Global.config->CanConfigSet->CanCallbacks->TxConfirmation)
				{
					Can_Global.config->CanConfigSet->CanCallbacks->TxConfirmation(swPduHandle);
					ASLOG(CAN, ("TxComfirm(%d, %d)\n", configId, swPduHandle));
				}
			}
		}
		#ifndef __AS_CAN_BUS__
		/* Rx Process */
		for (int configId=0; configId < CAN_CTRL_CONFIG_CNT; configId++)
		{
			canHwConfig = GET_CONTROLLER_CONFIG(configId);
			ctlrId = canHwConfig->CanControllerId;

			canUnit = GET_PRIVATE_DATA(ctlrId);
			if(STAILQ_EMPTY(&canUnit->rQ.pduHead))
			{
			}
			else
			{
				uint16 Hrh = 0xFFFF;
				struct Can_RPmsgPud_s* pdu;

				(void)pthread_mutex_lock(&canUnit->rQ.w_lock);
				pdu = STAILQ_FIRST(&canUnit->rQ.pduHead);
				STAILQ_REMOVE_HEAD(&canUnit->rQ.pduHead,pduEntry);
				(void)pthread_mutex_unlock(&canUnit->rQ.w_lock);

				const Can_HardwareObjectType  *hoh = Can_Global.config->CanConfigSet->CanController[pdu->msg.bus].Can_Arc_Hoh;
				hoh --;
				do{
					hoh ++;
					if(CAN_OBJECT_TYPE_RECEIVE == hoh->CanObjectType)
					{
						Hrh = hoh->CanObjectId;
						break;
					}
				}while(FALSE == hoh->Can_Arc_EOL);
				asAssert(0xFFFF != Hrh);
				asAssert(Can_Global.config->CanConfigSet->CanCallbacks->RxIndication);
				Can_Global.config->CanConfigSet->CanCallbacks->RxIndication(Hrh,pdu->msg.id,pdu->msg.length,pdu->msg.sdu);

				free(pdu);
			}
		}
		#endif
	}
}
#ifndef __AS_CAN_BUS__
void Can_RPmsg_RxNotitication(RPmsg_ChannelType chl,void* data, uint16 len)
{
	Can_UnitType *canUnit;
	const Can_ControllerConfigType *canHwConfig;
	uint8 ctlrId;
	Can_RPmsgPduType* pduInfo = data;
	asAssert(len==sizeof(Can_RPmsgPduType));
	asAssert(chl == RPMSG_CHL_CAN);

	if(pduInfo->bus < CAN_CTRL_CONFIG_CNT)
	{
		canHwConfig = GET_CONTROLLER_CONFIG(pduInfo->bus);
		ctlrId = canHwConfig->CanControllerId;
		canUnit = GET_PRIVATE_DATA(ctlrId);
		if(CANIF_CS_STARTED == canUnit->state)
		{
			struct Can_RPmsgPud_s* pdu = malloc(sizeof(struct Can_RPmsgPud_s));
			if(pdu)
			{
				memcpy(&(pdu->msg),pduInfo,sizeof(Can_RPmsgPduType));
				(void)pthread_mutex_lock(&canUnit->rQ.w_lock);
				STAILQ_INSERT_TAIL(&canUnit->rQ.pduHead,pdu,pduEntry);
				(void)pthread_mutex_unlock(&canUnit->rQ.w_lock);
			}
			else
			{
				ASWARNING("CAN RX malloc failed\n");
			}
		}
		else
		{
			ASWARNING("CAN is not on-line!\n");
		}
	}
	else
	{
		ASWARNING("CAN RX bus <%d> out of range, busid < %d is support only\n",pduInfo->bus,CAN_CTRL_CONFIG_CNT);
	}

}
void Can_RPmsg_TxConfirmation(RPmsg_ChannelType chl)
{
	(void)chl;
}
#endif

#ifdef USE_SHELL
#include "shell.h"
void statCan(void)
{
	Can_UnitType *canUnit;
	const Can_ControllerConfigType *canHwConfig;
	uint8 ctlrId;
	if(Can_Global.initRun == CAN_READY)
	{
		/* Tx Confirmation Process*/
		for (int configId=0; configId < CAN_CTRL_CONFIG_CNT; configId++)
		{
			canHwConfig = GET_CONTROLLER_CONFIG(configId);
			ctlrId = canHwConfig->CanControllerId;

			canUnit = GET_PRIVATE_DATA(ctlrId);
			SHELL_printf("CAN[%d] HRH is %d, TXCNT is %d\n",
					ctlrId, canUnit->swPduHandle, canUnit->stats.txSuccessCnt);
		}
	}
}
#endif


