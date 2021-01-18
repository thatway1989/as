"""/* Copyright(C) 2013, OpenSAR by Fan Wang(parai). All rights reserved.
 *
 * This file is part of OpenSAR.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Email: parai@foxmail.com
 * Source Open At: https://github.com/parai/OpenSAR/
 */
"""
import sys,os
from .GCF import *

__all__ = ['GenCom']


__dir = '.'

def GenCom(root,dir):
    global __dir
    GLInit(root)
    __dir = '%s'%(dir)
    if(len(GLGet('IPduList')) == 0):return
    GenH()
    GenC()
    GenRTE()
    print('    >>> Gen Com DONE <<<')

def GetPduList():
    L = []
    for pdu in GLGet('IPduList'):
        if(GAGet(pdu,'Direction')=='RECEIVE'):
            L.append(pdu)
    for pdu in GLGet('IPduList'):
        if(GAGet(pdu,'Direction')!='RECEIVE'):
            L.append(pdu)
    return L

def GenRTE():
    fp = open('%s/BSWCOM.py'%(__dir),'w')
    fp.write('import autosar\n\n')
    sigL = []
    for pdu in GetPduList():
        for sig in GLGet(pdu,'SignalList'):
            sigL.append(sig)
        for grpSig in GLGet(pdu,'GroupSignalList'):
            for sig in GLGet(grpSig,'SignalList'):
                sigL.append(sig)
    for sig in sigL:
        fp.write("C_{0}_IV = autosar.createConstantTemplateFromPhysicalType('C_{0}_IV', autosar.{1}_T, {2})\n".format(GAGet(sig,'Name'),GAGet(sig,'Type').upper(),GAGet(sig,'InitialValue')))
    fp.write('\n')
    fp.write('COM_D = []\n')
    for sig in sigL:
        fp.write("COM_D.append(autosar.createDataElementTemplate('{0}', autosar.{1}_T))\n".format(GAGet(sig,'Name'),GAGet(sig,'Type').upper()))
    fp.write('\n')
    fp.write("COM_I = autosar.createSenderReceiverInterfaceTemplate('Com_I', COM_D)\n")
    fp.write('\n')
    for sig in sigL:
        fp.write("{0} = autosar.createSenderReceiverPortTemplate('Com', COM_I, C_{0}_IV, aliveTimeout=30, elemName='{0}')\n".format(GAGet(sig,'Name')))
    fp.close()

def toSignal(sig,pdu,isGroupSignal=False):
    if(GAGet(pdu,'Direction')=='RECEIVE'):
        period = 'COM_MAIN_FUNCTION_RX_PERIOD'
        FirstTimeoutFactor = '(%s+%s-1)/%s'%(GAGet(sig,'FirstTimeoutFactor').replace('TBD','0xDB'),period,period)
        TimeoutFactor = '(%s+%s-1)/%s'%(GAGet(sig,'TimeoutFactor').replace('TBD','0xDB'),period,period)
        TimeoutNotification = GAGet(sig,'TimeoutNotification')
    else:
        period = 'COM_MAIN_FUNCTION_TX_PERIOD'
        TimeoutFactor = 0
        FirstTimeoutFactor = 0
        TimeoutNotification = 'NULL'
    if(isGroupSignal):
        ComSignalType = 'COM_SIGNAL_TYPE_UINT8_N'
        Com_Arc_IsSignalGroup = 'TRUE'
        SignalInitValue = '%s_InitValue'%(GAGet(sig,'Name'))
        ComGroupSignal = '%s_GrpSignalRefs'%(GAGet(sig,'Name'))
        Com_Arc_ShadowBuffer = '%s_ShadowBuffer'%(GAGet(sig,'Name'))
        Com_Arc_ShadowBuffer_Mask = '%s_ShadowBufferMask'%(GAGet(sig,'Name'))
    else:
        ComSignalType =  'COM_SIGNAL_TYPE_%s'%(GAGet(sig,'Type').upper())
        Com_Arc_IsSignalGroup = 'FALSE'
        if(GAGet(sig,'Type')=='uint8' or GAGet(sig,'Type')=='uint16' or GAGet(sig,'Type')=='uint32'):
            SignalInitValue = '&%s_InitValue'%(GAGet(sig,'Name'))
        else:
            SignalInitValue = '%s_InitValue'%(GAGet(sig,'Name'))
        ComGroupSignal = 'NULL'
        Com_Arc_ShadowBuffer = 'NULL'
        Com_Arc_ShadowBuffer_Mask = 'NULL'
    if(GAGet(pdu,'Direction')=='RECEIVE'):
        TimeoutAction = GAGet(sig,'TimeoutAction')
    else:
        TimeoutAction = 'NONE'
    cstr = """
    {
        #if defined(USE_SHELL)
        .name = "%s",
        #endif
        .ComBitPosition = %s,
        .ComBitSize = %s,
        .ComErrorNotification = NULL,
        .ComFirstTimeoutFactor = %s,
        .ComHandleId = COM_SID_%s,
        .ComNotification = %s,
        .ComRxDataTimeoutAction = COM_TIMEOUT_DATA_ACTION_%s,
        .ComSignalEndianess = COM_%s,
        .ComSignalInitValue = %s,
        .ComSignalType = %s,
        .ComTimeoutFactor = %s,
        .ComTimeoutNotification = %s,
        .ComTransferProperty = COM_%s,
        .ComUpdateBitPosition = %s,
        .ComSignalArcUseUpdateBit = %s,
        .Com_Arc_IsSignalGroup = %s,
        .ComGroupSignal = %s,
        .Com_Arc_ShadowBuffer = %s,
        .Com_Arc_ShadowBuffer_Mask = %s,
        .ComIPduHandleId = COM_ID_%s,
        .Com_Arc_EOL = FALSE
    },\n"""%(GAGet(sig,'Name'),
             GAGet(sig,'StartBit'),
             GAGet(sig,'Size'),
             FirstTimeoutFactor,
             GAGet(sig,'Name'),
             GAGet(sig,'ReceivedNotification'),
             TimeoutAction,
             GAGet(sig,'Endianess'),
             SignalInitValue,
             ComSignalType,
             TimeoutFactor,
             TimeoutNotification,
             GAGet(sig,'TransferProperty'),
             GAGet(sig,'UpdateBitPosition').replace('TBD','0xDB'),
             GAGet(sig,'UpdateBitUsed').upper(),
             Com_Arc_IsSignalGroup,
             ComGroupSignal,
             Com_Arc_ShadowBuffer,
             Com_Arc_ShadowBuffer_Mask,
             GAGet(pdu,'PduRef')
             )
    return cstr

# big endian bits map
_bebm = []
for i in range(64):
    for j in range(8):
        _bebm.append(i*8 + 7-j)

def beSet(data, start, size, value):
    # for big endian only
    rBit = size-1
    nBit = _bebm.index(start)
    wByte = 0
    wBit = 0
    for i in range(size):
        wBit = _bebm[nBit]
        wByte = int(wBit/8)
        wBit  = wBit%8
        if(value&(1<<rBit) != 0):
            data[wByte] |= 1<<wBit
        else:
            data[wByte] &= ~(1<<wBit)
        nBit += 1
        rBit -= 1

def leSet(data, start, size, value):
    rBit = size-1
    nBit = start+size-1
    wByte = 0
    wBit = 0
    for i in range(size):
        wBit = nBit
        wByte = int(wBit/8)
        wBit  = wBit%8
        if(value&(1<<rBit) != 0):
            data[wByte] |= 1<<wBit
        else:
            data[wByte] &= ~(1<<wBit)
        nBit -= 1
        rBit -= 1

def toGSignalMaskAndInitValue(gsig, pduSize):
    mask = []
    init = []
    for i in range(pduSize):
        init.append(0)
        mask.append(0)
    for sig in GLGet(gsig,'SignalList'):
        InitialValue = eval(GAGet(sig,'InitialValue'))
        StartBit = int(GAGet(sig,'StartBit'))
        Size = int(GAGet(sig,'Size'))
        if(GAGet(sig, 'Endianess') == 'BIG_ENDIAN'):
            beSet(mask, StartBit, Size, 0xFFFFFFFF)
            beSet(init, StartBit, Size, InitialValue)
        else:
            leSet(mask, StartBit, Size, 0xFFFFFFFF)
            leSet(init, StartBit, Size, InitialValue)
    cstrM = ''
    for d in mask:
        cstrM += '0x%02X,'%(d)
    cstrI = ''
    for d in init:
        cstrI += '0x%02X,'%(d)
    return cstrM,cstrI

def GenH():
    global __dir
    # =========================  PduR_Cfg.h ==================
    fp = open('%s/Com_Cfg.h'%(__dir),'w')
    fp.write(GHeader('Com'))
    sigNbr=sigGrpNbr=0
    for pdu in GetPduList():
        sigNbr+=len(GLGet(pdu,'SignalList'))+len(GLGet(pdu,'GroupSignalList'))
        for grpSig in GLGet(pdu,'GroupSignalList'):
            sigGrpNbr+=len(GLGet(grpSig,'SignalList'))
    fp.write("""

#if !(((COM_SW_MAJOR_VERSION == 1) && (COM_SW_MINOR_VERSION == 2)))
#error Com: Configuration file expected BSW module version to be 1.2.*
#endif

#ifndef COM_CFG_H_
#define COM_CFG_H_

#define COM_MODULE_ID 20
#define COM_INSTANCE_ID 1
#define COM_E_INVALID_FILTER_CONFIGURATION 101
#define COM_E_INITIALIZATION_FAILED 102
#define COM_E_INVALID_SIGNAL_CONFIGURATION 103
#define COM_INVALID_PDU_ID 104
#define COM_INVALID_SIGNAL_ID 109
#define COM_ERROR_SIGNAL_IS_SIGNALGROUP 105
#define COM_E_TOO_MANY_IPDU 106
#define COM_E_TOO_MANY_SIGNAL 107
#define COM_E_TOO_MANY_GROUPSIGNAL 108

#define COM_DEV_ERROR_DETECT STD_OFF

#define COM_N_IPDUS          %s
#define COM_N_SIGNALS        %s
#define COM_N_GROUP_SIGNALS  %s

#define ComConfigurationTimeBase 0
#define ComVersionInfoApi

/* unit in ms */
#define COM_MAIN_FUNCTION_RX_PERIOD  10
#define COM_MAIN_FUNCTION_TX_PERIOD  10

#endif /*COM_CFG_H*/
    """%(len(GetPduList()),sigNbr,sigGrpNbr))
    fp.close()
    # ====================
    fp = open('%s/Com_PbCfg.h'%(__dir),'w')
    fp.write(GHeader('Com'))
    fp.write("""
#if !(((COM_SW_MAJOR_VERSION == 1) && (COM_SW_MINOR_VERSION == 2)) )
#error Com: Configuration file expected BSW module version to be 1.2.*
#endif

#ifndef COM_PBCFG_H
#define COM_PBCFG_H

#include "Com_Types.h"
extern const Com_ConfigType ComConfiguration;

// PDU group id definitions
#define COM_DEFAULT_IPDU_GROUP  0\n\n""")
    id = 0
    for grp in GLGet('PduGroupList'):
        fp.write('#define COM_IPDU_GROUP_%-32s %s\n'%(GAGet(grp,'Name'),id))
        id += 2
    cstr = ''
    id = 0
    for pdu in GetPduList():
        cstr += '#define COM_ID_%-32s %s\n'%(GAGet(pdu,'PduRef'),id)
        id += 1
    fp.write("""
//  COM IPDU Id Defines.
%s \n\n"""%(cstr))
    id1 = id2 = 0
    cstr1 = cstr2 = cstr3 = cstr4 =''
    for pdu in GetPduList():
        if(GAGet(pdu,'Callout')!='NULL'):
            cstr4 += 'extern boolean %s(PduIdType, const uint8*);/* %s Callout */\n'%(GAGet(pdu,'Callout'),GAGet(pdu,'Name'))
        for sig in GLGet(pdu,'SignalList'):
            cstr1 += '#define COM_SID_%-32s %s\n'%(GAGet(sig,'Name'),id1)
            id1 += 1
            if(GAGet(pdu,'Direction')=='RECEIVE'):
                if(GAGet(sig,'TimeoutNotification')!='NULL'):
                    cstr3+= 'extern void %s(void); /* %s TimeoutNotification */\n'%(GAGet(sig,'TimeoutNotification'),GAGet(sig,'Name'))
                if(GAGet(sig,'ReceivedNotification')!='NULL'):
                    cstr3+= 'extern void %s(void); /* %s ReceivedNotification */\n'%(GAGet(sig,'ReceivedNotification'),GAGet(sig,'Name'))
        for grpSig in GLGet(pdu,'GroupSignalList'):
            cstr1 += '#define COM_SID_%s %s\n'%(GAGet(grpSig,'Name'),id1)
            id1 += 1
            for sig in GLGet(grpSig,'SignalList'):
                cstr2 += '#define COM_GSID_%-32s %s\n'%(GAGet(sig,'Name'),id2)
                id2 += 1
            if(GAGet(pdu,'Direction')=='RECEIVE'):
                if(GAGet(sig,'TimeoutNotification')!='NULL'):
                    cstr3+= 'extern void %s(void); /* %s TimeoutNotification */\n'%(GAGet(sig,'TimeoutNotification'),GAGet(sig,'Name'))
                if(GAGet(sig,'ReceivedNotification')!='NULL'):
                    cstr3+= 'extern void %s(void); /* %s ReceivedNotification */\n'%(GAGet(sig,'ReceivedNotification'),GAGet(sig,'Name'))
    fp.write("""
//General Signal (Group) Id defines
%s

//Group Signal Id defines
%s

// Notifications
%s

// Callouts
%s
#endif /* COM_PBCFG_H */    
    """%(cstr1,cstr2,cstr3,cstr4))
    fp.close()

def GenC():
    fp = open('%s/Com_PbCfg.c'%(__dir),'w')
    fp.write(GHeader('Com'))
    fp.write("""
#ifdef USE_COM
#include "Com.h"
#include "Com_Internal.h"
#if defined(USE_PDUR)
#include "PduR.h"
#endif 

//Signal init values.\n""")
    for pdu in GetPduList():
        for sig in GLGet(pdu,'SignalList'):
            if(GAGet(sig,'Type')=='uint8' or GAGet(sig,'Type')=='uint16' or GAGet(sig,'Type')=='uint32'):
                fp.write('static const %s %s_InitValue = %s;\n'%(GAGet(sig,'Type'),GAGet(sig,'Name'),
                                                           GAGet(sig,'InitialValue')))
            else:
                fp.write('static const %s %s_InitValue[%s] = {%s};\n'%(GAGet(sig,'Type')[:-1]+'t',GAGet(sig,'Name'),
                                                                 int((Integer(GAGet(sig,'Size'))+7)/8),
                                                                 GAGet(sig,'InitialValue')))
        for gsig in GLGet(pdu,'GroupSignalList'):
            mask,init = toGSignalMaskAndInitValue(gsig, int(GAGet(pdu,'PduSize')))
            fp.write('static uint8 %s_ShadowBuffer[%s];\n'%(GAGet(gsig,'Name'),GAGet(pdu,'PduSize')))
            fp.write('static const uint8 %s_InitValue[%s] = {%s};\n'%(GAGet(gsig,'Name'),GAGet(pdu,'PduSize'),init))
            fp.write('static const uint8 %s_ShadowBufferMask[%s]={%s};\n'%(GAGet(gsig,'Name'),GAGet(pdu,'PduSize'),mask))
            for sig in GLGet(gsig,'SignalList'):
                if(GAGet(sig,'Type')=='uint8' or GAGet(sig,'Type')=='uint16' or GAGet(sig,'Type')=='uint32'):
                    fp.write('static const %s %s_InitValue = %s;\n'%(GAGet(sig,'Type'),GAGet(sig,'Name'),
                                                               GAGet(sig,'InitialValue')))
                else:
                    fp.write('static const %s %s_InitValue[%s] = {%s};\n'%(GAGet(sig,'Type')[:-1]+'t',GAGet(sig,'Name'),
                                                                 int((Integer(GAGet(sig,'Size'))+7)/8),
                                                                 GAGet(sig,'InitialValue')))
    sigNbr=sigGrpNbr=0
    for pdu in GetPduList():
        sigNbr+=len(GLGet(pdu,'SignalList'))+len(GLGet(pdu,'GroupSignalList'))
        for grpSig in GLGet(pdu,'GroupSignalList'):
            sigGrpNbr+=len(GLGet(grpSig,'SignalList'))
    cstr1 = cstr2 = ''
    Number = -1
    for pdu in GetPduList():
        for gsig in GLGet(pdu,'GroupSignalList'):
            for sig in GLGet(gsig,'SignalList'):
                Number += 1
                if(Number==sigGrpNbr):
                    isEol = 'TRUE'
                else:
                    isEol = 'FALSE'
                cstr1 += """    
    {
        #if defined(USE_SHELL)
        .name = "%s",
        #endif
        .ComBitPosition= %s,
        .ComBitSize= %s,
        .ComHandleId= COM_GSID_%s,
        .ComSignalEndianess= COM_%s,
        .ComSignalInitValue= &%s_InitValue,
        .ComSignalType= COM_SIGNAL_TYPE_%s,
        .Com_Arc_EOL= %s
    },\n"""%(GAGet(sig,'Name'),
             GAGet(sig,'StartBit'),
             GAGet(sig,'Size'),
             GAGet(sig,'Name'),
             GAGet(sig,'Endianess'),
             GAGet(sig,'Name'),
             GAGet(sig,'Type').upper(),
             isEol)
    for pdu in GetPduList():
        for gsig in GLGet(pdu,'GroupSignalList'):
            cstr2+='static const ComGroupSignal_type * const %s_GrpSignalRefs[] =\n{\n'%(GAGet(gsig,'Name'))
            for sig in GLGet(gsig,'SignalList'):
                cstr2 += '\t&ComGroupSignal[ COM_GSID_%s ],\n'%(GAGet(sig,'Name'))
            cstr2 += '\tNULL\n};\n'
    if(sigGrpNbr>0):
        fp.write("""
static const ComGroupSignal_type ComGroupSignal[] = {
%s
};
//SignalGroup GroupSignals lists.
%s

//IPdu buffers and signal group buffers\n"""%(cstr1,cstr2)) 
    for pdu in GetPduList(): 
        fp.write('static uint8 %s_IPduBuffer[%s];\n'%(GAGet(pdu,'PduRef'),GAGet(pdu,'PduSize')))
        if(GAGet(pdu,'RxSignalProcessing')=='DEFERRED' and GAGet(pdu,'Direction')=='RECEIVE'):
            fp.write('static uint8 %s_IPduDefferredBuffer[%s];\n'%(GAGet(pdu,'PduRef'),GAGet(pdu,'PduSize')))
    cstr = ''
    id = 0
    for pdu in GetPduList():
        for sig in GLGet(pdu,'SignalList'):
            cstr += toSignal(sig, pdu)
        for sig in GLGet(pdu,'GroupSignalList'):
            cstr += toSignal(sig, pdu, True)
    fp.write("""
//Signal definitions
static const ComSignal_type ComSignal[] = {
%s
    {
         .Com_Arc_EOL =  True,
    }
};\n\n    """%(cstr))
    cstr = ''
    Number = -1
    for grp in GLGet('PduGroupList'):
        Number += 1
        if(Number == len(GLGet('PduGroupList'))):
            isEol = 'TRUE'
        else:
            isEol = 'FALSE'
        cstr += """
    {
        .ComIPduGroupHandleId =  COM_IPDU_GROUP_%s, 
        .Com_Arc_EOL =  %s
    },\n"""%(GAGet(grp,'Name'),isEol)
    fp.write("""
//I-PDU group definitions
static const ComIPduGroup_type ComIPduGroup[] = {
%s
};\n\n"""%(cstr))
    cstr = ''
    for pdu in GetPduList():
        cstr += 'static const ComSignal_type * const %s_SignalRefs[] = {\n'%(GAGet(pdu,'PduRef'))
        for sig in GLGet(pdu,'SignalList'):
            cstr += '\t&ComSignal[ COM_SID_%s ],\n'%(GAGet(sig,'Name'))
        for sig in GLGet(pdu,'GroupSignalList'):
            cstr += '\t&ComSignal[ COM_SID_%s ],\n'%(GAGet(sig,'Name'))
        cstr += '\tNULL\n};\n'        
    fp.write("""
//IPdu signal lists.
%s    
    """%(cstr))
    cstr = ''
    for pdu in GetPduList():
        if(GAGet(pdu,'RxSignalProcessing')=='DEFERRED' and GAGet(pdu,'Direction')=='RECEIVE'):
            dbf='%s_IPduDefferredBuffer'%(GAGet(pdu,'PduRef'))
        else:
            dbf='NULL'
        cstr += """
    {
        #if defined(USE_SHELL)
        .name = "%s",
        #endif
        .ComIPduCallout =  %s,
        .ArcIPduOutgoingId =  PDUR_ID_%s,
        .ComIPduSignalProcessing =  COM_%s,
        .ComIPduSize =  %s,
        .ComIPduDirection =  COM_%s,
        .ComIPduGroupRef =  COM_IPDU_GROUP_%s,
        .ComTxIPdu ={
            .ComTxIPduMinimumDelayFactor =  %s,
            .ComTxIPduUnusedAreasDefault =  %s,
            .ComTxModeTrue ={
                .ComTxModeMode =   COM_%s,
                .ComTxModeNumberOfRepetitions =   %s,
                .ComTxModeRepetitionPeriodFactor =   %s,
                .ComTxModeTimeOffsetFactor =   (%s+COM_MAIN_FUNCTION_TX_PERIOD-1)/COM_MAIN_FUNCTION_TX_PERIOD,
                .ComTxModeTimePeriodFactor =   (%s+COM_MAIN_FUNCTION_TX_PERIOD-1)/COM_MAIN_FUNCTION_TX_PERIOD,
            },
        },
        .ComIPduDataPtr =  %s_IPduBuffer,
        .ComIPduDeferredDataPtr =  %s,
        .ComIPduSignalRef =  %s_SignalRefs,
        .ComIPduDynSignalRef =  NULL,
        .Com_Arc_EOL =  FALSE,
    },\n"""%(GAGet(pdu,'Name'),
             GAGet(pdu,'Callout'),
             GAGet(pdu,'PduRef'),
             GAGet(pdu,'RxSignalProcessing'),
             GAGet(pdu,'PduSize'),
             GAGet(pdu,'Direction'),
             GAGet(pdu,'PduGroupRef'),
             GAGet(pdu,'TxMinimumDelayFactor'),
             GAGet(pdu,'DefaultValueForUnusedArea'),
             GAGet(pdu,'TxMode'),
             GAGet(pdu,'NumberOfRepetions'),
             GAGet(pdu,'RepetionsPeriodFactor'),
             GAGet(pdu,'TimeOffsetFactor'),
             GAGet(pdu,'TimePeriodFactor'),
             GAGet(pdu,'PduRef'),
             dbf,
             GAGet(pdu,'PduRef'))
    
    fp.write("""
//I-PDU definitions
static const ComIPdu_type ComIPdu[] = {
%s
    {
        .Com_Arc_EOL =  TRUE
    }
};

const Com_ConfigType ComConfiguration = {
    .ComConfigurationId =  1,
    .ComIPdu =  ComIPdu,
    .ComIPduGroup =  ComIPduGroup,
    .ComSignal =  ComSignal,
#if(COM_N_GROUP_SIGNALS > 0)
    .ComGroupSignal =  ComGroupSignal
#else
    .ComGroupSignal =  NULL
#endif
};\n\n"""%(cstr))
    cstr = 'static Com_Arc_IPdu_type Com_Arc_IPdu[] = {\n'
    for pdu in GetPduList():
        cstr += """
    { // %s
        .Com_Arc_TxIPduTimers ={
            .ComTxIPduNumberOfRepetitionsLeft =  0,
            .ComTxModeRepetitionPeriodTimer =  0,
            .ComTxIPduMinimumDelayTimer =  0,
            .ComTxModeTimePeriodTimer =  0
        },
        .Com_Arc_IpduStarted =  0
    },\n"""%(GAGet(pdu,'PduRef'))
    cstr += '};\n\n'
    cstr += 'Com_Arc_Signal_type Com_Arc_Signal[] = {\n'
    for pdu in GetPduList():
        if(GAGet(pdu,'Direction')!='RECEIVE'):
            break
        for sig in GLGet(pdu,'SignalList'):
            cstr += """
    { // %s
        .Com_Arc_DeadlineCounter =  0,
        .ComSignalUpdated =  0,
    },\n"""%(GAGet(sig,'Name'))
        for grpSig in GLGet(pdu,'GroupSignalList'):
            cstr += """
    { // %s
        .Com_Arc_DeadlineCounter =  0,
        .ComSignalUpdated =  0,
    },\n"""%(GAGet(grpSig,'Name'))
    cstr += '};\n\n'   
    fp.write(cstr)
    fp.write("""   
#if(COM_N_GROUP_SIGNALS > 0)
static Com_Arc_GroupSignal_type Com_Arc_GroupSignal[COM_N_GROUP_SIGNALS];
#endif

const Com_Arc_Config_type Com_Arc_Config = {
    .ComIPdu =  Com_Arc_IPdu,
    .ComSignal =  Com_Arc_Signal,
#if(COM_N_GROUP_SIGNALS > 0)
    .ComGroupSignal =  Com_Arc_GroupSignal
#else
    .ComGroupSignal =  NULL
#endif
};
#endif /* USE_COM */\n""")
    fp.close()

