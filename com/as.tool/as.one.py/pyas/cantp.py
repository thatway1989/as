__lic__ = '''
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
 '''
try:
    from .can import *
except:
    from can import *
import time


__all__ = ['cantp','DFTBUS']

ISO15765_TPCI_MASK =  0x30
ISO15765_TPCI_SF = 0x00         #/* Single Frame */
ISO15765_TPCI_FF = 0x10         #/* First Frame */
ISO15765_TPCI_CF = 0x20         #/* Consecutive Frame */
ISO15765_TPCI_FC = 0x30         #/* Flow Control */
ISO15765_TPCI_DL = 0x7          #/* Single frame data length mask */
ISO15765_TPCI_FS_MASK = 0x0F    #/* Flow control status mask */


ISO15765_FLOW_CONTROL_STATUS_CTS    =    0
ISO15765_FLOW_CONTROL_STATUS_WAIT   =    1
ISO15765_FLOW_CONTROL_STATUS_OVFLW  =    2

CANTP_ST_IDLE = 0
CANTP_ST_START_TO_SEND = 1
CANTP_ST_SENDING = 2
CANTP_ST_WAIT_FC = 3
CANTP_ST_WAIT_CF = 4
CANTP_ST_SEND_CF = 5
CANTP_ST_SEND_FC = 6


class cantp():
    __state_name = {
        0:'Idle',
        1:'start to send',
        2:'sending',
        3:'wait flow control',
        4:'wait consecutive frame',
        5:'sending consecutive frame',
        6:'sending flow control'
    }

    def __init__(self,**kwargs):
        self.canbus  = kwargs['busid']
        self.rxid = kwargs['rxid']
        self.txid = kwargs['txid']
        self.padding = kwargs.get('padding', 0x55)
        self.state = CANTP_ST_IDLE
        self.SN = 0
        self.t_size = 0
        self.STmin = 0
        self.BS=0
        self.cSTmin = kwargs.get('STmin', 1)
        self.cBS = kwargs.get('BS', 8)
        self.NA = kwargs.get('NA', None)
        self.cfgSTmin = 0
        self.ll_dl = kwargs.get('ll_dl', 8)
        if(self.ll_dl not in [8, 64]):
            raise
        self.timeout = 5
        self.init()

    def init(self):
        if(self.ll_dl > 8):
            if(self.NA != None):
                self.MAX_SF = self.ll_dl-3
                self.MAX_FF = self.ll_dl-7
                self.MAX_CF = self.ll_dl-1
            else:
                self.MAX_SF = self.ll_dl-2
                self.MAX_FF = self.ll_dl-6
                self.MAX_CF = self.ll_dl-2
        else:
            if(self.NA != None):
                self.MAX_SF = 6
                self.MAX_FF = 5
                self.MAX_CF = 6
            else:
                self.MAX_SF = 7
                self.MAX_FF = 6
                self.MAX_CF = 7

    def reset(self):
        return can_reset(self.canbus)

    def __sendSF_clasic(self,request):
        length = len(request)
        data = []
        if(self.NA != None):
            data.append(self.NA)
        data.append(ISO15765_TPCI_SF | (length&0x0F))
        for i,c in enumerate(request):
            data.append(c&0xFF)
        i = len(data)
        while(i<8):
            data.append(self.padding)
            i += 1
        return can_write(self.canbus,self.txid,data)
    
    def __sendSF_ll(self,request):
        length = len(request)
        data = []
        if(self.NA != None):
            data.append(self.NA)
        data.append(ISO15765_TPCI_SF)
        data.append(length)
        for i,c in enumerate(request):
            data.append(c&0xFF)
        i = len(data)
        while(i<self.ll_dl):
            data.append(self.padding)
            i += 1
        return can_write(self.canbus,self.txid,data)

    def __sendSF__(self,request):
        classic_MAX_SF = 6 if self.NA != None else 7 
        if(len(request) <= classic_MAX_SF):
            r = self.__sendSF_clasic(request)
        else:
            r = self.__sendSF_ll(request)
        return r
    
    def __sendFF_clasic(self,data):
        length = len(data)
        pdu = []
        if(self.NA != None):
            pdu.append(self.NA)
        pdu.append(ISO15765_TPCI_FF | ((length>>8)&0x0F))
        pdu.append(length&0xFF)
  
        for d in data[:self.MAX_FF]:
            pdu.append(d)
  
        self.SN = 0
        self.t_size = self.MAX_FF
        self.state = CANTP_ST_WAIT_FC
  
        return can_write(self.canbus,self.txid,pdu)

    def __sendFF_ll(self,data):
        length = len(data)
        pdu = []
        if(self.NA != None):
            pdu.append(self.NA)
        pdu.append(ISO15765_TPCI_FF | 0)
        pdu.append(0)
        pdu.append((length>>24)&0xFF)
        pdu.append((length>>16)&0xFF)
        pdu.append((length>>8)&0xFF)
        pdu.append(length&0xFF)

        for d in data[:self.MAX_FF]:
            pdu.append(d)
  
        self.SN = 0
        self.t_size = self.MAX_FF
        self.state = CANTP_ST_WAIT_FC
  
        return can_write(self.canbus,self.txid,pdu)

    def __sendFF__(self,request):
        if(self.ll_dl <= 8):
            r = self.__sendFF_clasic(request)
        else:
            r = self.__sendFF_ll(request)
        return r

    def __sendCF__(self,request): 
        sz = len(request)
        t_size = self.t_size
        pdu = []
        if(self.NA != None):
            pdu.append(self.NA)
        self.SN += 1
        if (self.SN > 15):
            self.SN = 0
            
        l_size = sz - t_size  #  left size 
        if (l_size > self.MAX_CF):
            l_size = self.MAX_CF
  
        pdu.append(ISO15765_TPCI_CF | self.SN)
  
        for i in range(l_size):
          pdu.append(request[t_size+i])
  
        i = len(pdu)
        while(i<self.ll_dl):
            pdu.append(self.padding)
            i = i + 1
  
        self.t_size += l_size
  
        if (self.t_size == sz):
            self.state = CANTP_ST_IDLE
        else:
            if (self.BS > 0):
                self.BS -= 1
                if (0 == self.BS):
                    self.state = CANTP_ST_WAIT_FC
                else:
                    self.state = CANTP_ST_SEND_CF
            else:
              self.state = CANTP_ST_SEND_CF
  
        self.STmin = self.cfgSTmin
  
        return can_write(self.canbus,self.txid,pdu)
   
    def __handleFC__(self,request):
        ercd,data = self.__waitRF__()
        if (True == ercd):
            if ((data[0]&ISO15765_TPCI_MASK) == ISO15765_TPCI_FC):
                if ((data[0]&ISO15765_TPCI_FS_MASK) == ISO15765_FLOW_CONTROL_STATUS_CTS): 
                    self.cfgSTmin = data[2]
                    self.BS = data[1]
                    self.STmin = 0   # send the first CF immediately
                    self.state = CANTP_ST_SEND_CF
                elif ((data[0]&ISO15765_TPCI_FS_MASK) == ISO15765_FLOW_CONTROL_STATUS_WAIT):
                    self.state = CANTP_ST_WAIT_FC
                elif ((data[0]&ISO15765_TPCI_FS_MASK) == ISO15765_FLOW_CONTROL_STATUS_OVFLW):
                    print("cantp buffer over-flow, cancel...")
                    ercd = False
                else:
                    print("FC error as reason %X,invalid flow status"%(data[0]))
                    ercd = False
            else:
                print("FC error as reason %X,invalid PCI"%(data[0]))
                ercd = False 
        return ercd
    
    def __schedule_tx__(self,request):
        length = len(request)

        ercd = self.__sendFF__(request)  # FF sends 6 bytes
  
        if (True == ercd):
            while(self.t_size < length):
                if(self.state == CANTP_ST_WAIT_FC):
                    ercd = self.__handleFC__(request)
                elif(self.state == CANTP_ST_SEND_CF):
                    if(self.STmin > 0):
                        self.STmin = self.STmin - 1
                        time.sleep(0.001)
                    if(self.STmin == 0):
                      ercd = self.__sendCF__(request)
                else:
                    print("cantp: transmit unknown state ", self.__state_name[self.state])
                    ercd = False
                if(ercd == False):
                    break
  
        return ercd
         
    def transmit(self,request):
        assert(len(request) < 4096)
        result = True
        while(result):
            result,canid,data= can_read(self.canbus,self.rxid)
            if(result):
                print('cantp: there is unconsumed message 0x%X %s, drop it...'%(canid, data))
        self.state = CANTP_ST_IDLE
        if(len(request) <= self.MAX_SF):
            ercd = self.__sendSF__(request)
        else:
            ercd = self.__schedule_tx__(request)
        return ercd

    def __waitRF__(self):
        ercd = False
        data=None
        pre = time.time()
        while ( ((time.time() -pre) < self.timeout) and (ercd == False)):
            result,canid,data= can_read(self.canbus,self.rxid)
            if((True == result) and (self.rxid == canid)):
                if(self.NA != None):
                    if(self.NA != data[0]):
                        print('ignore Frame: NA 0x%X != 0x%x'%(self.NA, data[0]))
                        time.sleep(0.001)
                    else:
                        data = data[1:]
                        ercd = True
                        break
                else:
                    ercd = True
                    break
            else:
                time.sleep(0.001) # sleep 1 ms
        
        if (False == ercd):
            print("cantp timeout when receiving a frame! elapsed time = %s ms"%(time.time() -pre))
            print("state is %s"%(self.__state_name[self.state]))
        else:
            if((len(data) in [63, 64]) and (self.ll_dl != 64)):
                self.ll_dl = 64
                self.init()
                print('switch CANTP to CANFD mode!')

        return ercd,data
   
    def __waitSForFF__(self,response):
        ercd,data = self.__waitRF__()
        finished = False
        if (True == ercd):
            if ((data[0]&ISO15765_TPCI_MASK) == ISO15765_TPCI_SF):
                lsize = data[0]&ISO15765_TPCI_DL
                rPos = 1
                if(lsize == 0):
                    lsize = data[1]
                    rPos = 2
                for i in range(lsize):
                    response.append(data[rPos+i])
                ercd = True
                finished = True
            elif ((data[0]&ISO15765_TPCI_MASK) == ISO15765_TPCI_FF):
                self.t_size = ((data[0]&0x0F)<<8) + data[1]
                rPos = 2
                if(self.t_size == 0):
                    self.t_size = (data[2]<<24) + (data[3]<<16) + (data[4]<<8) + (data[5]<<0)
                    rPos = 6
                for d in data[rPos:]:
                    response.append(d)
                self.state = CANTP_ST_SEND_FC
                self.SN = 0
                ercd = True
                finished = False
        else:
            ercd = False
            finished = True
 
        return ercd,finished

    def __waitCF__(self,response): 
        sz = len(response)
        t_size = self.t_size
   
        ercd,data = self.__waitRF__()
   
        finished = False
        if (True == ercd ):
            if ((data[0]&ISO15765_TPCI_MASK) == ISO15765_TPCI_CF):
                self.SN += 1
                if (self.SN > 15):
                    self.SN = 0
       
                SN = data[0]&0x0F
                if (SN == self.SN):
                    l_size = t_size - sz  # left size
                    if (l_size > (self.ll_dl-1)):
                        l_size = self.ll_dl-1
                    for i in range(l_size):
                        response.append(data[i+1])
         
                    if ((sz+l_size) == t_size):
                        finished = True
                    else:
                        if (self.BS > 0):
                            self.BS -= 1
                            if (0 == self.BS):
                                self.state = CANTP_ST_SEND_FC
                            else:
                                self.state = CANTP_ST_WAIT_CF
                        else:
                            self.state = CANTP_ST_WAIT_CF
                else:
                    ercd = False
                    finished = True
                    print("cantp: wrong sequence number!",SN,self.SN)
            else:
                print("invalid PCI mask %02X when wait CF"%(data[0]))
                ercd = False
                finished = True
        else:
            print('response size = %s, %s'%(len(response),response))
        return ercd,finished

    def __sendFC__(self):
        pdu = []
        if(self.NA != None):
            pdu.append(self.NA)
        pdu.append(ISO15765_TPCI_FC | ISO15765_FLOW_CONTROL_STATUS_CTS)
        pdu.append(self.cBS)
        pdu.append(self.cSTmin)
   
        i = len(pdu)
        while(i<8):
            pdu.append(self.padding)
            i += 1
        self.BS = self.cBS
        self.state = CANTP_ST_WAIT_CF
   
        return can_write(self.canbus,self.txid,pdu)

    def receive(self, timeout=5):
        ercd = True
        response = []
        self.timeout = timeout
  
        finished = False
  
        ercd,finished = self.__waitSForFF__(response)

        while ((True == ercd) and (False == finished)):
            if (self.state == CANTP_ST_SEND_FC):
                ercd = self.__sendFC__()
            elif (self.state == CANTP_ST_WAIT_CF):
                ercd,finished = self.__waitCF__(response)
            else:
                print("cantp: receive unknown state ",self.state)
                ercd = False

        return ercd,response

if(__name__ == '__main__'):
    # open COM4
    can_open(0,'serial',3,115200)
    tp  = cantp(0,0x732,0x731)
    tp.transmit([0x10,0x03])
    tp.receive()