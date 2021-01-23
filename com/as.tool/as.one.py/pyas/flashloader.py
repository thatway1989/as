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
 
from .dcm import *
from .s19 import *
from .xcp import *
import traceback


from PyQt5 import QtCore, QtGui
from PyQt5.QtGui import *
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *

import os
import glob
import time

from asserial import *

__all__ = ['UIFlashloader']

class AsFlashloader(QThread):
    infor = QtCore.pyqtSignal(str)
    progress = QtCore.pyqtSignal(int)
    def __init__(self,parent=None):
        super(QThread, self).__init__(parent)
        self.steps = [ (self.enter_extend_session,True), (self.security_extds_access,True),
                  (self.enter_program_session,True),(self.reset_transimit_protocol,False),(self.security_prgs_access,True),
                  (self.download_flash_driver,True),(self.check_flash_driver,False),
                  (self.routine_erase_flash,True), (self.download_application,True),
                  (self.check_application,False), (self.launch_application,True) ]
        self.stepsXcp = [ (self.dummy,False), (self.dummy,False),
                  (self.enter_program_session_xcp,True),(self.reset_transimit_protocol,False),(self.security_prgs_access_xcp,True),
                  (self.download_flash_driver_xcp,True),(self.check_flash_driver_xcp,False),
                  (self.routine_erase_flash_xcp,True), (self.download_application_xcp,True),
                  (self.check_application_xcp,False), (self.launch_application_xcp,True) ]
        self.stepsCmd = [ (self.dummy,False), (self.dummy,False),
                  (self.dummy,False),(self.reset_transimit_protocol,False),(self.dummy,False),
                  (self.download_flash_driver_cmd,True),(self.dummy,False),
                  (self.routine_erase_flash_cmd,True), (self.download_application_cmd,True),
                  (self.dummy,False), (self.dummy,False) ]
        self.enable = []
        for s in self.steps:
            self.enable.append(s[1])
        self.app = None
        self.flsdrv = None
        self.ability = 4096
        self.protocol = 'UDS on CAN'

    def add_progress(self,sz):
        self.txSz += sz
        self.step_progress((self.txSz*100)/self.sumSz)

    def dummy(self):
        return False,None

    def runcmd(self,cmd):
        ercd,msg = self.serial.runcmd(cmd)
        if(ercd == False):
            self.infor.emit('%s\n%s'%(cmd,msg))
        #print(cmd,msg)
        return ercd,msg

    def open_cmd(self):
        settings = {}
        settings['port'] = self.port
        settings['baund'] = 115200
        settings['bytesize'] = 8
        settings['parity']='N'
        settings['stopbits']=1
        settings['timeout'] = 0.1
        self.serial = AsSerial()
        ercd,msg = self.serial.open(settings,False)
        if(False == ercd):
            self.infor.emit(msg)
        return ercd,None

    def close_cmd(self):
        self.serial.close()
        return True,None

    def download_one_section_cmd(self,address,size,data,identifier):
        FLASH_WRITE_SIZE = self.writeProperty
        left_size = size
        pos = 0
        # FIXME: according to the shell command implementation
        ability = 2000
        # round up
        size2 = int((left_size+FLASH_WRITE_SIZE-1)/FLASH_WRITE_SIZE)*FLASH_WRITE_SIZE
        ercd = True
        while(left_size>0 and ercd==True):
            cmd = '%s %s '%(identifier,hex(address+pos))
            if(left_size > ability):
                sz = ability
                left_size = left_size - ability
            else:
                sz = int((left_size+FLASH_WRITE_SIZE-1)/FLASH_WRITE_SIZE)*FLASH_WRITE_SIZE
                left_size = 0
            for i in range(sz):
                if((pos+i)<size):
                    cmd += '%02X'%(data[pos+i])
                else:
                    cmd += 'FF'
            ercd,res = self.runcmd(cmd)
            if(ercd == False):return ercd,res
            self.add_progress(sz)
            pos += sz
        return ercd,res

    def download_flash_driver_cmd(self):
        flsdrv = self.flsdrvs
        ary = flsdrv.getData()
        for ss in ary:
            ercd,res = self.download_one_section_cmd(ss['address'],ss['size'],ss['data'],'load')
            if(ercd == False):return ercd,res
        return ercd,res

    def routine_erase_flash_cmd(self):
        saddr, eaddr = self.get_app_erase_range()
        cmd = 'erase %s %s'%(hex(saddr), hex(eaddr-saddr))
        return self.runcmd(cmd)

    def download_application_cmd(self):
        app = self.apps
        ary = app.getData(True)
        for ss in ary:
            ercd,res = self.download_one_section_cmd(ss['address'],ss['size'],ss['data'],'write')
            if(ercd == False):return ercd,res
        return ercd,res

    def transmit_xcp(self, req, ignore=False):
        res = self.tp.transmit(req.toarray())
        if((res != None) and (res.toarray()[0] == 0xFF)):
            ercd = True
            if(not ignore): self.infor.emit('  success')
        else:
            ercd = False
            self.infor.emit('  failed')
        return ercd,res

    def enter_program_session_xcp(self):
        req = xcpbits()
        req.append(0xFF,8)
        req.append(0x00,8) # normal mode
        ercd,res = self.transmit_xcp(req)

        return ercd,res

    def security_prgs_access_xcp(self):
        self.infor.emit(' == unlock PGM ==')
        req = xcpbits()
        req.append(0xF8,8)
        req.append(0x00,8) # normal mode
        req.append(0x10,8) # PGM
        self.infor.emit(' request seed')
        ercd,res = self.transmit_xcp(req)

        if(ercd == True):
            res = res.toarray()
            if((res[1] != 4) or (len(res) < 6)):
                self.infor.emit('  invalid seed size')
                return False,None
            seed = (res[2]<<24)+(res[3]<<16)+(res[4]<<8)+(res[5]<<0)
            key = seed
            req = xcpbits()
            req.append(0xF7,8)
            req.append(4,8)
            req.append((key>>24)&0xFF,8)
            req.append((key>>16)&0xFF,8)
            req.append((key>>8)&0xFF,8)
            req.append((key>>0)&0xFF,8)
            self.infor.emit(' send key')
            ercd,res = self.transmit_xcp(req)

        if(ercd == True):
            self.infor.emit(' == unlock CALPAG ==')
            req = xcpbits()
            req.append(0xF8,8)
            req.append(0x00,8) # normal mode
            req.append(0x01,8) # CALPAG
            self.infor.emit(' request seed')
            ercd,res = self.transmit_xcp(req)

        if(ercd == True):
            res = res.toarray()
            if((res[1] != 4) or (len(res) < 6)):
                self.infor.emit('  invalid seed size')
                return False,None
            seed = (res[2]<<24)+(res[3]<<16)+(res[4]<<8)+(res[5]<<0)
            key = seed
            req = xcpbits()
            req.append(0xF7,8)
            req.append(4,8)
            req.append((key>>24)&0xFF,8)
            req.append((key>>16)&0xFF,8)
            req.append((key>>8)&0xFF,8)
            req.append((key>>0)&0xFF,8)
            self.infor.emit(' send key')
            ercd,res = self.transmit_xcp(req)

        if(ercd == True):
            req = xcpbits()
            req.append(0xD2,8)
            self.infor.emit(' program start')
            ercd,res = self.transmit_xcp(req)

        return ercd,res

    def download_one_section_xcp(self,address,size,data,identifier):
        req = xcpbits()
        req.append(0xF6,8)
        req.append(0x00,16)      # reserved
        req.append(identifier,8) # extensition
        req.append(address,32)   # address
        self.infor.emit(' set MTA address %s, type %s'%(hex(address), identifier))
        ercd,res = self.transmit_xcp(req)

        self.infor.emit(' downloading data...')
        ability = self.tp.get_max_cto()-2
        left = size
        rPos = 0
        while((left > 0) and (ercd == True)):
            doSz = left
            if(doSz > ability):
                doSz = ability
            req = xcpbits()
            req.append(0xF0,8)
            req.append(doSz,8)
            for i in range(doSz):
                req.append(data[rPos+i],8)
            rPos += doSz
            left -= doSz
            ercd,res = self.transmit_xcp(req,True)
            self.add_progress(doSz)
        if(ercd == True): self.infor.emit('  success')
        return ercd,res

    def upload_one_section_xcp(self,address,size,identifier):
        req = xcpbits()
        req.append(0xF6,8)
        req.append(0x00,16)      # reserved
        req.append(identifier,8) # extensition
        req.append(address,32)   # address
        self.infor.emit(' set MTA address %s, type %s'%(hex(address), identifier))
        ercd,res = self.transmit_xcp(req)

        data = []
        left = size

        self.infor.emit(' uploading data...')
        ability = self.tp.get_max_cto()-1
        left = size
        while((left > 0) and (ercd == True)):
            doSz = left
            if(doSz > ability):
                doSz = ability
            req = xcpbits()
            req.append(0xF5,8)
            req.append(doSz,8)
            left -= doSz
            ercd,res = self.transmit_xcp(req,True)
            if(ercd == True):
                res = res.toarray()
                for i in range(doSz):
                    data.append(res[1+i])
                self.add_progress(doSz)
        if(ercd == True): self.infor.emit('  success')
        return ercd,res,data

    def download_flash_driver_xcp(self):
        flsdrv = self.flsdrvs
        ary = flsdrv.getData()
        for ss in ary:
            ercd,res = self.download_one_section_xcp(ss['address'],ss['size'],ss['data'],0x00)
            if(ercd == False):return ercd,res
        return ercd,res

    def check_flash_driver_xcp(self):
        flsdrv = self.flsdrvs
        ary = flsdrv.getData()
        flsdrvr = s19()
        for ss in ary:
            ercd,res,up = self.upload_one_section_xcp(ss['address'],ss['size'],0x00)
            flsdrvr.append(ss['address'],up)
            if(ercd and self.compare(ss['data'], up)):
                self.infor.emit('  check flash driver pass!')
            else:
                self.infor.emit('  check flash driver failed!')
                flsdrvr.dump('read_%s'%(os.path.basename(self.flsdrv)))
                return False,res
        flsdrvr.dump('read_%s'%(os.path.basename(self.flsdrv)))
        return ercd,res

    def routine_erase_flash_xcp(self):
        saddr, eaddr = self.get_app_erase_range()
        req = xcpbits()
        req.append(0xF6,8)
        req.append(0x00,16)      # reserved
        req.append(0x01,8)       # extensition FLASH
        req.append(saddr,32)     # address
        ercd,res = self.transmit_xcp(req, True)

        if(ercd == True):
            self.infor.emit(' erase @ address %s, length %s'%(hex(saddr),hex(eaddr-saddr)))
            req = xcpbits()
            req.append(0xD1,8)
            req.append(0x00,8)       # absolute access mode
            req.append(0x00,16)      # reserved
            req.append(eaddr-saddr,32)     # length
            ercd,res = self.transmit_xcp(req)

        return ercd,res

    def program_one_section_xcp(self,address,size,data,identifier):
        # TODO: should use command program instead of download
        return self.download_one_section_xcp(address,size,data,identifier)

    def download_application_xcp(self):
        app = self.apps
        ary = app.getData(True)
        for ss in ary:
            ercd,res = self.program_one_section_xcp(ss['address'],ss['size'],ss['data'],0x01)
            if(ercd == False):return ercd,res
        return ercd,res

    def check_application_xcp(self):
        app = self.apps
        ary = app.getData(True)
        appr = s19()
        for ss in ary:
            ercd,res,up = self.upload_one_section_xcp(ss['address'],ss['size'],0x01)
            appr.append(ss['address'],up)
            if(ercd and self.compare(ss['data'], up)):
                self.infor.emit('  check application pass!')
            else:
                self.infor.emit('  check application failed!')
                appr.dump('read_%s'%(os.path.basename(self.app)))
                return False,res
        appr.dump('read_%s'%(os.path.basename(self.app)))
        return ercd,res

    def launch_application_xcp(self):
        req = xcpbits()
        req.append(0xCF,8)
        return self.transmit_xcp(req, True)

    def set_protocol(self,p):
        self.protocol = p
        if(p == 'UDS on DOIP'):
            self.ability = 1400
        else:
            self.ability = 4096

    def start_protocol(self):
        p = self.protocol
        if(p == 'UDS on CAN'):
            self.tp = dcm(protocal='CAN',busid=DFTBUS,rxid=0x732,txid=0x731)
        elif(p == 'UDS on USBCAN'):
            self.tp = dcm(protocal='CAN',busid=DFTBUS,rxid=0x732,txid=0x731)
        elif(p == 'UDS on CANFD'):
            self.tp = dcm(protocal='CAN',busid=DFTBUS,rxid=0x732,txid=0x731, ll_dl=64)
        elif(p == 'UDS on DOIP'):
            ip,port=self.misc.split(':')
            port = eval(port)
            self.infor.emit('start DoIP over %s:%s'%(ip, port))
            self.tp = dcm(protocal='DOIP',ip=ip,port=port)
        elif(p == 'XCP on CAN'):
            self.tp = xcp(DFTBUS, 0x554, 0x555)
        elif(p.startswith('CMD on COM')):
            self.port = p.split(' ')[2]
        else:
            self.protocol = 'unknown protocol %s'%(p)
            self.infor.emit(self.protocol)

    def is_download_application_enabled(self):
        return self.enable[8]
    def is_check_application_enabled(self):
        return self.enable[9]
    def is_download_flash_driver_enabled(self):
        return self.enable[5]
    def is_check_flash_driver_enabled(self):
        return self.enable[6]
    def setTarget(self,app,flsdrv=None, eraseProperty='512', writeProperty='8', signature='8', busPorperty='4096', misc=''):
        self.app = app
        self.flsdrv = flsdrv
        self.eraseProperty = eval(str(eraseProperty))
        self.writeProperty = eval(str(writeProperty))
        self.flsSignature = eval(str(signature))
        self.ability = eval(str(busPorperty))
        self.misc = misc

    def GetSteps(self):
        ss = []
        if('XCP' in self.protocol):
            steps = self.stepsXcp
        elif('CMD' in self.protocol):
            steps = self.stepsCmd
        else:
            steps = self.steps
        for s in steps:
            ss.append((s[0].__name__.replace('_',' '),s[1]))
        return ss
    
    def SetEnable(self,step,enable):
        for id,s in enumerate(self.GetSteps()):
            if(step == s[0]):
                self.enable[id] = enable

    def step_progress(self,v):
        self.progress.emit(v)

    def transmit(self,req,exp,ignore=False):
        ercd,res = self.tp.transmit(req)
        if(ercd == True):
            if(len(res)>=len(exp)):
                for i in range(len(exp)):
                    if((res[i]!=exp[i]) and (exp[i]!=-1)):
                        ercd = False
                        break
            else:
                ercd = False
        if(ercd == True):
            if(not ignore): self.infor.emit('  success')
        else:
            self.infor.emit('  failed')
        return ercd,res

    def enter_extend_session(self):
        return self.transmit([0x10,0x03], [0x50,0x03])
    def security_extds_access(self):
        ercd,res = self.transmit([0x27,0x01], [0x67,0x01,-1,-1,-1,-1])
        if(ercd):
            seed = (res[2]<<24) + (res[3]<<16) + (res[4]<<8) +(res[5]<<0)
            key = (seed^0x78934673)
            self.infor.emit(' send key %X from seed %X'%(key,seed))
            ercd,res = self.transmit([0x27,0x02,(key>>24)&0xFF,(key>>16)&0xFF,(key>>8)&0xFF,(key>>0)&0xFF],[0x67,0x02])
        return ercd,res
    def enter_program_session(self):
        return self.transmit([0x10,0x02], [0x50,0x02])

    def reset_transimit_protocol(self):
        self.infor.emit('reset protocol...')
        self.tp.reset()
        return True,None

    def security_prgs_access(self):
        ercd,res = self.transmit([0x27,0x03], [0x67,0x03,-1,-1,-1,-1])
        if(ercd):
            seed = (res[2]<<24) + (res[3]<<16) + (res[4]<<8) +(res[5]<<0)
            key = (seed^0x94586792)
            self.infor.emit(' send key %X from seed %X'%(key,seed))
            ercd,res = self.transmit([0x27,0x04,(key>>24)&0xFF,(key>>16)&0xFF,(key>>8)&0xFF,(key>>0)&0xFF],[0x67,0x04])
        return ercd,res
    def request_download(self,address,size,identifier):
        self.infor.emit(' request download')
        return self.transmit([0x34,0x00,0x44,     \
                            (address>>24)&0xFF,(address>>16)&0xFF,(address>>8)&0xFF,(address>>0)&0xFF,   \
                            (size>>24)&0xFF,(size>>16)&0xFF,(size>>8)&0xFF,(size>>0)&0xFF,  \
                            identifier],[0x74])

    def request_upload(self,address,size,identifier):
        self.infor.emit(' request upload')
        return self.transmit([0x35,0x00,0x44,     \
                            (address>>24)&0xFF,(address>>16)&0xFF,(address>>8)&0xFF,(address>>0)&0xFF,   \
                            (size>>24)&0xFF,(size>>16)&0xFF,(size>>8)&0xFF,(size>>0)&0xFF,  \
                            identifier],[0x75])
    
    def request_transfer_exit(self):
        self.infor.emit(' request transfer exit')
        return self.transmit([0x37],[0x77])
    
    def download_one_section(self,address,size,data,identifier):
        FLASH_WRITE_SIZE = self.writeProperty
        blockSequenceCounter = 1
        left_size = size
        pos = 0
        ability = int(((self.ability-5)/FLASH_WRITE_SIZE)) * FLASH_WRITE_SIZE
        # round up
        size2 = int((left_size+FLASH_WRITE_SIZE-1)/FLASH_WRITE_SIZE)*FLASH_WRITE_SIZE
        ercd,res = self.request_download(address,size2,identifier)
        if(ercd == False):return ercd,res
        while(left_size>0 and ercd==True):
            req = [0x36,blockSequenceCounter,0,identifier]
            if(left_size > ability):
                sz = ability
                left_size = left_size - ability
            else:
                sz = int((left_size+FLASH_WRITE_SIZE-1)/FLASH_WRITE_SIZE)*FLASH_WRITE_SIZE
                left_size = 0
            for i in range(sz):
                if((pos+i)<size):
                    req.append(data[pos+i])
                else:
                    req.append(0xFF)
            ercd,res = self.transmit(req,[0x76,blockSequenceCounter],True)
            self.add_progress(sz)
            if(ercd == False):return ercd,res
            blockSequenceCounter = (blockSequenceCounter + 1)&0xFF
            pos += sz
        ercd,res = self.request_transfer_exit()
        if(ercd == False):return ercd,res
        return ercd,res

    def upload_one_section(self,address,size,identifier):
        FLASH_READ_SIZE = 4
        blockSequenceCounter = 1
        left_size = size
        ability = int(((self.ability-5)/FLASH_READ_SIZE)) * FLASH_READ_SIZE
        # round up
        size2 = int((left_size+FLASH_READ_SIZE-1)/FLASH_READ_SIZE)*FLASH_READ_SIZE
        ercd,res = self.request_upload(address,size2,identifier)
        if(ercd == False):return ercd,res,None
        data = []
        while(left_size>0 and ercd==True):
            req = [0x36,blockSequenceCounter,0,identifier]
            ercd,res = self.transmit(req,[0x76,blockSequenceCounter],True)
            if(ercd == False):return ercd,res,None
            blockSequenceCounter = (blockSequenceCounter + 1)&0xFF
            sz = len(res)-2
            self.add_progress(sz)
            if (left_size > sz):
                left_size = left_size - sz
            else:
                left_size = 0
            for b in res[2:]:
                data.append(b)
        ercd,res = self.request_transfer_exit()
        if(ercd == False):return ercd,res,None
        return ercd,res,data
    
    def compare(self,d1,d2):
        for i,b in enumerate(d1):
            if(b!=d2[i]):
                return False
        return True

    def download_flash_driver(self):
        flsdrv = self.flsdrvs
        ary = flsdrv.getData()
        for ss in ary:
            ercd,res = self.download_one_section(ss['address']-ary[0]['address'],ss['size'],ss['data'],0xFD)
            if(ercd == False):return ercd,res
        return ercd,res

    def check_flash_driver(self):
        flsdrv = self.flsdrvs
        ary = flsdrv.getData()
        flsdrvr = s19()
        for ss in ary:
            ercd,res,up = self.upload_one_section(ss['address']-ary[0]['address'],ss['size'],0xFD)
            if(ercd == True):
                flsdrvr.append(ss['address'],up)
            if(ercd and self.compare(ss['data'], up)):
                self.infor.emit('  check flash driver pass!')
            else:
                self.infor.emit('  check flash driver failed!')
                flsdrvr.dump('read_%s'%(os.path.basename(self.flsdrv)))
                return False,res
        flsdrvr.dump('read_%s'%(os.path.basename(self.flsdrv)))
        return ercd,res

    def get_app_erase_range(self, section=None):
        if(section != None):
            saddr = section['address']
            eaddr = section['address'] + section['size']
        else:
            app = self.apps
            ary = app.getData(True)
            saddr = ary[0]['address']
            eaddr = ary[0]['address'] + ary[0]['size']
            for ss in ary:
                if(ss['address']< saddr):
                    saddr = ss['address']
                if(ss['address']+ss['size'] > eaddr):
                    eaddr = ss['address']+ss['size']
        if(type(self.eraseProperty) == list):
            for addr in self.eraseProperty:
                if(eaddr <= addr):
                    eaddr = addr
                    break
        else:
            saddrAligned = int(saddr/self.eraseProperty)*self.eraseProperty
            if(saddrAligned != saddr):
                saddr = saddrAligned
            eaddr = int((eaddr+self.eraseProperty-1)/self.eraseProperty)*self.eraseProperty
        return saddr, eaddr

    def routine_erase_flash(self):
        if(self.apps.hasS3):
            saddr, eaddr = self.get_app_erase_range()
            eaddr = eaddr - saddr # get the length
            return self.transmit([0x31,0x01,0xFF,0x01,
                                  (saddr>>24)&0xFF,(saddr>>16)&0xFF,(saddr>>8)&0xFF,(saddr>>0)&0xFF,
                                  (eaddr>>24)&0xFF,(eaddr>>16)&0xFF,(eaddr>>8)&0xFF,(eaddr>>0)&0xFF,
                                  0xFF],[0x71,0x01,0xFF,0x01])
        else:
            # if no S3 record, generally it's a 16 bit banked MCU
            # and the FLASH address is not continious
            ercd,res = False,None
            app = self.apps
            ary = app.getData(True)
            for section in ary:
                saddr, eaddr = self.get_app_erase_range(section)
                eaddr = eaddr - saddr # get the length
                ercd,res = self.transmit([0x31,0x01,0xFF,0x01,
                                      (saddr>>24)&0xFF,(saddr>>16)&0xFF,(saddr>>8)&0xFF,(saddr>>0)&0xFF,
                                      (eaddr>>24)&0xFF,(eaddr>>16)&0xFF,(eaddr>>8)&0xFF,(eaddr>>0)&0xFF,
                                      0xFF],[0x71,0x01,0xFF,0x01])
                if(ercd != True):
                    break
            return ercd,res
    
    def download_application(self):
        app = self.apps
        ary = app.getData(True)
        for id,ss in enumerate(ary):
            if((id==0)  and (self.flsSignature>0)):
                if(self.flsSignature > ss['size']):
                    self.flsSignature = ss['size']
                addr = ss['address']+self.flsSignature
                data = ss['data'][self.flsSignature:]
                size = ss['size']-self.flsSignature
                if(size == 0):
                    continue
            else:
                addr = ss['address']
                data = ss['data']
                size = ss['size']
            ercd,res = self.download_one_section(addr,size,data,0xFF)
            if(ercd == False):return ercd,res
        # write the signature at last
        if(self.flsSignature>0):
            addr = ary[0]['address']
            data = ary[0]['data'][:self.flsSignature]
            size = self.flsSignature
            ercd,res = self.download_one_section(addr,size,data,0xFF)
        return ercd,res

    def check_application(self):
        app = self.apps
        ary = app.getData(True)
        appr = s19()
        for ss in ary:
            ercd,res,up = self.upload_one_section(ss['address'],ss['size'],0xFF)
            if(ercd == True):
                appr.append(ss['address'],up)
            if(ercd and self.compare(ss['data'], up)):
                self.infor.emit('  check application pass!')
            else:
                self.infor.emit('  check application failed!')
                appr.dump('read_%s'%(os.path.basename(self.app)))
                return False,res
        appr.dump('read_%s'%(os.path.basename(self.app)))
        return ercd,res

    def launch_application(self):
        return self.transmit([0x31,0x01,0xFF,0x03], [0x71,0x01,0xFF,0x03])

    def run_common(self, steps):
        flsSz = 0
        if(self.is_download_flash_driver_enabled()):
            self.flsdrvs = s19(self.flsdrv)
            flsSz = self.flsdrvs.size
        self.apps = s19(self.app)
        appSz = self.apps.size
        self.sumSz = appSz + flsSz
        if(self.is_check_application_enabled()):
            self.sumSz += appSz
        if(self.is_check_flash_driver_enabled()):
            self.sumSz += flsSz
        self.txSz = 0
        self.infor.emit('summary transfer size is %s bytes(app %s, flsdrv %s)!'%(
                        self.sumSz,appSz,flsSz))
        pre = time.time()
        for id,s in enumerate(steps):
            if((self.enable[id] == True) and (s[0].__name__ != 'dummy')):
                self.infor.emit('>> '+s[0].__name__.replace('_',' '))
                try:
                    ercd,res = s[0]()
                except Exception as e:
                    ercd = False
                    self.infor.emit('%s'%(e))
                    print(traceback.format_exc())
                if(ercd == False):
                    self.infor.emit("\n\n  >> boot failed <<\n\n")
                    return
        cost = time.time() - pre
        speed = int(self.sumSz/cost)
        self.infor.emit('cost %ss, speed is %sbps'%(cost, speed))
        self.progress.emit(100)

    def run_uds(self):
        self.run_common(self.steps)

    def run_xcp(self):
        self.run_common(self.stepsXcp)

    def run_cmd(self):
        self.open_cmd()
        self.run_common(self.stepsCmd)
        self.close_cmd()

    def run(self):
        self.start_protocol()
        self.infor.emit('starting with protocol "%s"... '%(self.protocol))
        if('UDS' in self.protocol):
            self.run_uds()
        elif('CMD' in self.protocol):
            self.run_cmd()
        elif('XCP' in self.protocol):
            self.run_xcp()
        else:
            self.infor.emit('invalid protocol')

class AsStepEnable(QCheckBox):
    enableChanged=QtCore.pyqtSignal(str,bool)
    def __init__(self,text,parent=None):
        super(QCheckBox, self).__init__(text,parent)
        self.stateChanged.connect(self.on_stateChanged)
    def on_stateChanged(self,state):
        self.enableChanged.emit(self.text(),state)
        
class UIFlashloader(QWidget):
    def __init__(self, parent=None):
        super(QWidget, self).__init__(parent)
        
        self.loader = AsFlashloader()
        self.loader.infor.connect(self.on_loader_infor)
        self.loader.progress.connect(self.on_loader_progress)

        vbox = QVBoxLayout()
        
        grid = QGridLayout()
        grid.addWidget(QLabel('Application'),0,0)
        self.leApplication = QLineEdit()
        grid.addWidget(self.leApplication,0,1)
        self.btnOpenApp = QPushButton('...')
        grid.addWidget(self.btnOpenApp,0,2)

        grid.addWidget(QLabel('Flash Driver'),1,0)
        self.leFlsDrv = QLineEdit()
        grid.addWidget(self.leFlsDrv,1,1)
        self.btnOpenFlsDrv = QPushButton('...')
        grid.addWidget(self.btnOpenFlsDrv,1,2)

        grid.addWidget(QLabel('Progress'),2,0)
        self.pgbProgress = QProgressBar()
        self.pgbProgress.setRange(0,100)
        grid.addWidget(self.pgbProgress,2,1)
        self.cmbxProtocol = QComboBox()
        items = ['UDS on CAN','UDS on CANFD','UDS on DOIP','XCP on CAN','UDS on USBCAN']
        for i in search_serial_ports():
            items.append('CMD on COM%s'%(i))
        self.cmbxProtocol.addItems(items)
        self.cmbxProtocol.setEditable(True)
        grid.addWidget(self.cmbxProtocol,2,2)
        self.btnStart=QPushButton('Start')
        grid.addWidget(self.btnStart,2,3)
        self.lblMisc = QLabel()
        self.leMisc = QLineEdit()
        self.leMisc.setDisabled(True)
        grid.addWidget(self.lblMisc,3,0)
        grid.addWidget(self.leMisc,3,1)
        vbox.addLayout(grid)

        grid.addWidget(QLabel('Erase Property:'),4,0)
        self.leFlsEraseProperty = QLineEdit()
        grid.addWidget(self.leFlsEraseProperty,4,1)
        grid.addWidget(QLabel('Bus Property:'),4,2)
        self.leFlsBusProperty = QLineEdit()
        self.leFlsBusProperty.setText('4096')
        grid.addWidget(self.leFlsBusProperty,4,3)
        self.leFlsEraseProperty.setToolTip('Sector start address list or the smallest sector size\nfor example:\n  list:[0,128*1024,...]\n  size: 512')
        self.leFlsEraseProperty.setText('128*1024')
        grid.addWidget(QLabel('Write Property:'),5,0)
        self.leFlsWriteProperty = QLineEdit()
        grid.addWidget(self.leFlsWriteProperty,5,1)
        self.leFlsWriteProperty.setToolTip('the smallest page size')
        self.leFlsWriteProperty.setText('8')
        grid.addWidget(QLabel('Signature:'),5,2)
        self.leFlsSignature = QLineEdit()
        self.leFlsSignature.setMaximumWidth(120)
        grid.addWidget(self.leFlsSignature,5,3)
        self.leFlsSignature.setToolTip('the signature size at the begining of Image')
        self.leFlsSignature.setText('8')
        hbox = QHBoxLayout()
        vbox2 = QVBoxLayout()
        self.cbxEnableList = []
        for s in self.loader.GetSteps():
            cbxEnable = AsStepEnable(s[0])
            cbxEnable.setChecked(s[1])
            cbxEnable.enableChanged.connect(self.on_enableChanged)
            vbox2.addWidget(cbxEnable)
            self.cbxEnableList.append(cbxEnable)
        hbox.addLayout(vbox2)
        self.leinfor = QTextEdit()
        self.leinfor.setReadOnly(True)
        hbox.addWidget(self.leinfor)
        
        vbox.addLayout(hbox)
        
        self.setLayout(vbox)
        
        self.btnOpenApp.clicked.connect(self.on_btnOpenApp_clicked)
        self.btnOpenFlsDrv.clicked.connect(self.on_btnOpenFlsDrv_clicked)
        self.btnStart.clicked.connect(self.on_btnStart_clicked)
        self.cmbxProtocol.currentIndexChanged.connect(self.on_cmbxProtocol_currentIndexChanged)
        aspath = os.path.abspath('%s/../../..'%(os.curdir))
        default_app='%s/com/as.application/board.mpc56xx/MPC5634M_MLQB80/Project/bin/internal_FLASH.mot'%(aspath)
        default_flsdrv='%s/com/as.application/board.mpc56xx/MPC5634M_MLQB80/FlsDrv/bin/internal_FLASH.mot'%(aspath)
        if(not os.path.exists(default_app)):
            default_app = ''
        if(not os.path.exists(default_flsdrv)):
            default_flsdrv = ''
        if(os.path.exists(aspath)):
            for ss in glob.glob('%s/build/*/*/ascore/*.s19'%(aspath)):
                default_app = ss
                self.leFlsEraseProperty.setText('2*1024')
                self.leFlsSignature.setText('0x500')
                break
            for ss in glob.glob('%s/build/*/*/asboot/*-flsdrv*.s19'%(aspath)):
                default_flsdrv = ss
                break
        if(os.path.exists(default_app)):
            self.leApplication.setText(default_app)
        if(os.path.exists(default_flsdrv)):
            self.leFlsDrv.setText(default_flsdrv)

    def on_cmbxProtocol_currentIndexChanged(self,index):
        protocol = str(self.cmbxProtocol.currentText())
        self.loader.set_protocol(protocol)
        self.leFlsBusProperty.setText('%s'%(self.loader.ability))

        if('UDS on DOIP' == protocol):
            self.leMisc.setDisabled(False)
            self.leMisc.setText('172.18.0.200:8989')
            self.lblMisc.setText('DoIP Address:')
        else:
            self.leMisc.setDisabled(True)
            self.leMisc.setText('')
            self.lblMisc.setText('')

        for id,s in enumerate(self.loader.GetSteps()):
            self.cbxEnableList[id].setText(s[0])
            self.cbxEnableList[id].setChecked(s[1])

    def on_enableChanged(self,step,enable):
        self.loader.SetEnable(step, enable)

    def on_loader_infor(self,text):
        self.leinfor.append(text)
    
    def on_loader_progress(self,prg):
        self.pgbProgress.setValue(prg)

    def on_btnOpenApp_clicked(self):
        rv = QFileDialog.getOpenFileName(None,'application file', '','application (*.s19 *.bin *.mot)')
        self.leApplication.setText(rv[0])

    def on_btnOpenFlsDrv_clicked(self):
        rv = QFileDialog.getOpenFileName(None,'flash driver file', '','flash driver (*.s19 *.bin *.mot)')
        self.leFlsDrv.setText(rv[0])

    def on_btnStart_clicked(self):
        if(os.path.exists(str(self.leApplication.text()))):
            self.pgbProgress.setValue(1)
            self.loader.setTarget(str(self.leApplication.text()), str(self.leFlsDrv.text()),
                                  str(self.leFlsEraseProperty.text()),str(self.leFlsWriteProperty.text()),
                                  str(self.leFlsSignature.text()),str(self.leFlsBusProperty.text()),
                                  str(self.leMisc.text()))
            self.loader.start()
        else:
            QMessageBox.information(self, 'Tips', 'Please load a valid application first!')
