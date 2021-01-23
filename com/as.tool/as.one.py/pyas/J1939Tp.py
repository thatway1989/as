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


__all__ = ['J1939Tp']

class J1939Tp():
    def __init__(self, **kwargs):
        self.canbus =  kwargs['busid']
        self.TxFcNPdu = kwargs['TxFcNPdu']
        self.TXDirectNPdu = kwargs['TXDirectNPdu']
        self.TXCmNPdu = kwargs['TXCmNPdu']
        self.TXDtNPdu = kwargs['TXDtNPdu']
        self.RXCmNPdu = kwargs['RXCmNPdu']
        self.RXFcNPdu = kwargs['RXFcNPdu']
        self.RXDirectNPdu = kwargs['RXDirectNPdu']
        self.RXDtNPdu = kwargs['RXDtNPdu']
        self.STmin = kwargs['STmin']
        self.ll_dl = 8

    def __padding(self,req):
        data = list(req)
        if(len(data) < self.ll_dl):
            for i in range(len(data), self.ll_dl):
                data.append(0x55)
        return data

    def __send_df(self,request):
        return can_write(self.canbus, self.TXDirectNPdu, request)

    def __wait_cts(self):
        ercd,canid,data = self.__waitRF__([self.RXFcNPdu])
        # 0x11 is command CTS
        if( (ercd == True) and ( (canid != self.RXFcNPdu) or (data[0] != 0x11) ) ):
            ercd = False
            print('J1939TP: not a CTS:', hex(canid), data)
        elif(ercd == True):
            self.PACKETS_PER_BLOCK = data[1]
            if(self.NextPacketSeqNum != data[2]):
                print('J1939TP: invalid sequence number %s != %s'%(self.NextPacketSeqNum, data[2]))
            self.PGN = data[5] + (data[6]<<8) + (data[7]<<16)
            #print('J1939 CTS:', self.PACKETS_PER_BLOCK, data[2], self.PGN)
        return ercd

    def __send_rts(self,request):
        # 0x10 is Command RTS
        length = len(request)
        data = [0x10, length&0xFF, (length>>8)&0xFF, int((length+self.ll_dl-2)/(self.ll_dl-1))]
        ercd = can_write(self.canbus, self.TXCmNPdu, data)
        self.t_size = 0
        self.NextPacketSeqNum = 1

        if(ercd == True):
            ercd = self.__wait_cts()
        return ercd

    def __send_cts(self):
        # REVERSE_CM
        data = [0x11, self.PACKETS_PER_BLOCK, self.NextPacketSeqNum, 0xFF, 0xFF,
                self.PGN&0xFF, (self.PGN>>8)&0xFF, (self.PGN>>16)&0xFF]
        return can_write(self.canbus, self.TxFcNPdu, data)

    def __send_ack(self, response):
        length = len(response)
        data = [0x13, (length>>8)&0xFF, length&0xFF,
                int((length+self.ll_dl-1)/(self.ll_dl-1)), 0xFF,
                self.PGN&0xFF, (self.PGN>>8)&0xFF, (self.PGN>>16)&0xFF]
        return can_write(self.canbus, self.TxFcNPdu, data)

    def __wait_ack(self):
        ercd,canid,data = self.__waitRF__([self.RXFcNPdu])
        # 0x13 ENDOFMSGACK_CONTROL_VALUE
        if( (ercd == True) and ( (canid != self.RXFcNPdu) or (data[0] != 0x13) ) ):
            ercd = False
        print(ercd,canid,data)
        return ercd;

    def __send_dt(self,request):
        length = len(request)
        data = [self.NextPacketSeqNum]
        left = length - self.t_size
        if(left > (self.ll_dl-1)):
            left = self.ll_dl-1
        for i in range(left):
            data.append(request[self.t_size+i])
        ercd = can_write(self.canbus, self.TXDtNPdu, data)
        self.NextPacketSeqNum += 1
        if(ercd == True):
            time.sleep(0.001*self.STmin)
            self.t_size += left
        return ercd

    def __schedule_tx__(self,request):
        length = len(request)
        ercd = self.__send_rts(request)
        while((ercd == True) and (self.t_size < length)):
            ercd = self.__send_dt(request)
            if(self.t_size >= length):
                ercd = self.__wait_ack()
            elif(ercd and ((self.NextPacketSeqNum-1)>0) and (((self.NextPacketSeqNum-1)%self.PACKETS_PER_BLOCK)==0)):
                ercd = self.__wait_cts()
        return ercd

    def transmit(self,request):
        if(len(request) <= self.ll_dl):
            ercd = self.__send_df(request)
        else:
            ercd = self.__schedule_tx__(request)

        return ercd

    def __waitRF__(self, idlist = None):
        if(idlist == None):
            idlist = [self.RXDirectNPdu,self.RXFcNPdu,self.RXCmNPdu,self.RXDtNPdu]
        ercd = False
        canid = data = None
        pre = time.time()
        while ( ((time.time() -pre) < 1) and (ercd == False)): # 1s timeout
            for rxid in idlist:
                ercd,canid,data= can_read(self.canbus, rxid)
                if(ercd == True): break
            time.sleep(0.001) # sleep 1 ms
        
        if(ercd == False):
            print('J1939TP: wait Response Frame %s %s s timeout!'%(idlist, time.time() -pre))

        return ercd,canid,data

    def receive(self):
        response = []  
        finished = False

        # response must be either DF or RTS
        ercd,canid,data = self.__waitRF__([self.RXDirectNPdu,self.RXCmNPdu])

        if(canid == self.RXDirectNPdu): # DF
            finished = True
            response.extend(data)
        elif((self.RXCmNPdu == canid) and (0x10==data[0])): # RTS
            left = data[1] + (data[2]<<8)
            if( (data[3]*(self.ll_dl-1)-left) >= (self.ll_dl-1) ):
                print('J1939TP: invalid length<%s> and number of packets<%s>'%(left, data[3]))
            self.PGN = data[5] + (data[6]<<8) + (data[7]<<16)
            self.NextPacketSeqNum = 1
            self.PACKETS_PER_BLOCK = 8
        else:
            print('J1939TP: invalid RF %s %s'%(canid, data))
            ercd = False

        while ((True == ercd) and (False == finished)):
            if((self.NextPacketSeqNum-1)%self.PACKETS_PER_BLOCK == 0):
                ercd = self.__send_cts()
            ercd,canid,data = self.__waitRF__([self.RXDtNPdu])
            if(True == ercd):
                if(self.NextPacketSeqNum != data[0]):
                    ercd = False
                    print('J1939TP: invalid sequence number %s != %s'%(self.NextPacketSeqNum, data[0]))
                    break
                self.NextPacketSeqNum += 1
                doCopy = len(data[1:])
                if(left < doCopy):
                    doCopy = left
                left -= doCopy
                response.extend(data[1:doCopy+1])
                if(left <= 0):
                    finished = True
                    self.__send_ack(response)

        return ercd,response

    def reset(self):
        pass

if(__name__ == '__main__'):
    can_open(0,'socket',1,1000000)
    can_write(0,0x754,[0x00,0x11,0x22,0x33,0x44,0x55,0x66,0x77])
