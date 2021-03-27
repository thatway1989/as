__lic__ = '''
/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2017  AS <parai@foxmail.com>
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

__all__ = ['doip']

import socket
import time

class doip():
    #Generic doip header negative acknowledge codes
    __generalAck = {0x00:'DOIP_E_INCORRECT_PATTERN_FORMAT', 0x01:'DOIP_E_UNKNOWN_PAYLOAD_TYPE',
                  0x02:'DOIP_E_MESSAGE_TOO_LARGE',         0x03:'DOIP_E_OUT_OF_MEMORY',
                  0x04:'DOIP_E_INVALID_PAYLOAD_LENGTH' }
    #Diagnostic message negative acknowledge codes
    __diagAck = { 0x02:'DOIP_E_DIAG_INVALID_SA',  0x03:'DOIP_E_DIAG_UNKNOWN_TA',
                0x04:'DOIP_E_DIAG_MESSAGE_TO_LARGE',0x05:'DOIP_E_DIAG_OUT_OF_MEMORY',
                0x06:'DOIP_E_DIAG_TARGET_UNREACHABLE',0x07:'DOIP_E_DIAG_UNKNOWN_NETWORK',
                0x08:'DOIP_E_DIAG_TP_ERROR'
    }
    __rapc={0x00:'Routing activation rejected due to unknown source address',
            0x02:'Routing activation denied because an SA different from the table connection entry was received on the already activated TCP_DATA socket',
            0x04:'Routing activation rejected due to missing authentication',
            0x06:'Routing activation denied due to unsupported routing activation type',
            0x10:'Routing successfully activated',
            0x11:'Routing will be activated; confirmation required',
            0x7e:'Vehicle manufacturer-specific',}
    def __init__(self, **kwargs):
        self.uri = kwargs.get('ip', '172.18.0.200')
        self.port = kwargs.get('port', 8989)
        self.udp = kwargs.get('udp', True)
        self.startup()

    def startup(self):
        self.online = False
        if(self.udp):
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        else:
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.settimeout(5)
        try:
            self.sock.connect((self.uri, self.port))
        except Exception as e:
            self.sock.close()
            self.sock = None
            print('DoIP connect: %s'%(e))
            return
        self.sock.settimeout(None)
        # do Routing Activation request, sa=0xbeef, activationType=0xda, and padding 0x00 to payloadLength to 7
        res = self.transmit([0xbe,0xef,0xda,0x00,0x00,0x00,0x00],0x0005)
        if(res == False):
            self.sock.close()
            print('DoIP Routing Activation failed')
            return
        ercd,data=self.checkResponse(res)
        if(ercd==False):
            self.sock.close()
            self.sock = None
            raise Exception('  >> DoIP: do Routing Activation request failed as %s!'%(self.__generalAck[data]))
        else:
            ackcode=data[0]
            data=data[1]
            assert(ackcode==6) # 0x0006->Routing Activation Response
            self.sa = (data[0]<<8)+data[1]
            assert(self.sa==0xbeef)
            self.ta = 0xfeed
            self.DoIpNodeLogicalAddress = (data[2]<<8)+data[3]
            self.routingActivationResponseCode=data[4]
            if(self.routingActivationResponseCode != 0x10):
                raise Exception('  >> DoIP: do Routing Activation request failed as %s!'%(self.__rapc[self.routingActivationResponseCode]))
        print('DoIP online!')
        self.online = True

    def reset(self):
        self.sock.close()
        time.sleep(2)
        self.startup()

    def __delete__(self):
        self.sock.close()

    def set_ll_dl(self,v):
        pass

    def checkResponse(self,res):
        ackcode = (res[2] << 8) + res[3]
        length  = (res[4] << 24) + (res[5] << 16) + (res[6] << 8) + res[7]
        assert(length+8 == len(res))
        if((ackcode == 0) and (length == 1)): # 0 : Negative Acknowledge
            nackCode = res[8]
            return False,nackCode
        return True,(ackcode,res[8:])

    def write(self,payloadType,payload):
        s = [2,(~2)&0xFF] # version
        s += [(payloadType>>8)&0xFF,payloadType&0xFF]
        length = len(payload)
        s += [(length>>24)&0xFF,(length>>16)&0xFF,(length>>8)&0xFF,(length>>0)&0xFF]
        for i in payload:
            try:
                s.append(ord(i)&0xFF)
            except TypeError:
                s.append(i&0xFF)
        return self.sock.send(bytes(s))

    def read(self):
        return self.sock.recv(4096)

    def receive(self):
        ''' for UDS only '''
        # DoIP protocal response
        try:
            res = self.sock.recv(4096)
        except Exception as e:
            print('DoIP receive: %s'%(e))
            return False,None
        ercd,data=self.checkResponse(res)
        if(ercd==False):
            raise Exception('  >> DoIP: do UDS request failed as %s!'%(self.__generalAck[data]))
            return False,None
        else:
            ackcode=data[0]
            data=data[1]
            if(0x8003 == ackcode): #0x8003->Diagnostic Message Negative Acknowledge
                nackCode=data[4]
                raise Exception('  >> DoIP: do UDS request failed as %s!'%(self.__diagAck[nackCode]))
        # Dcm protocol response
        res = self.sock.recv(4096)
        ercd,data=self.checkResponse(res)
        if(ercd==False):
            raise Exception('  >> DoIP: do UDS request failed as %s!'%(self.__generalAck[data]))
            return False,None
        else:
            ackcode=data[0]
            data=data[1]
            assert(0x8001 == ackcode) #// 0x8001->Diagnostic message
            sa = (data[0] << 8) + data[1]
            ta = (data[2] << 8) + data[3]
            assert(ta == self.sa and sa == self.ta)
            return True,data[4:]

    def transmit(self,payload,payloadType=0x8001):
        ''' generally for UDS purpose '''
        if(None == self.sock):
            return False
        if(payloadType==0x8001):
            try:
                payload = [(self.sa>>8)&0xFF,self.sa&0xFF,(self.ta>>8)&0xFF,self.ta&0xFF] + payload
            except TypeError:
                data = [(self.sa>>8)&0xFF,self.sa&0xFF,(self.ta>>8)&0xFF,self.ta&0xFF]
                for i in payload:
                    data.append(i)
                payload = data
        len = self.write(payloadType, payload)
        if(payloadType==0x8001):
            if(len > 0):
                return True
            else:
                return False
        else:
            return self.read()
