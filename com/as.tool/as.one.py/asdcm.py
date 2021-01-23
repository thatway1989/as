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
from PyQt5 import QtCore, QtGui
from PyQt5.QtGui import *
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
from serial import Serial
from time import sleep
from time import ctime
from binascii import hexlify, unhexlify
import sys,os
import xml.etree.ElementTree as ET
from pyas.dcm import *
import traceback
import re

from bitarray import bitarray

__all__ = ['UIDcm']

def FormatMessage(res):
    ss = "["
    for v in res.toarray():
        ss += '%02X,'%(v)
    ss+=']'
    return ss

class dcmbits():
    def __init__(self):
        self.bits = bitarray()

    def append(self,d, num=1):
        for i in range(num):
            if((d&(1<<(num-1-i))) != 0):
                self.bits.append(True)
            else:
                self.bits.append(False)
    
    def toint(self,pos,num):
        bits = self.bits[pos:pos+num]
        bytes = int((num+4)/8)
        left = bytes*8 - num
        v = 0
        for b in bits.tobytes():
            v = (v<<8) + b
        v =v >> left
        return v
        
    def toarray(self):
        return self.bits.tobytes()

def str2int(sstr):
    if(sstr[:2].lower() == '0x'):
        return int(sstr,16)
    elif(sstr[:2].lower() == '0b'):
        return int(sstr,2)
    else:
        return int(sstr,10)

__dcm__ = dcm(protocal='CAN',busid=DFTBUS,rxid=0x732,txid=0x731)

def switch_to_protocol(protocol):
    global __dcm__

    if(protocol == 'DOIP'):
        if(os.getenv('DOIP_IP')):
            ip = os.getenv('DOIP_IP')
        else:
            ip = '172.18.0.200'
        __dcm__ = dcm(protocal='DOIP',ip=ip,port=8989)
        print("switch to UDS on DoIP mode on %s:8989"%(ip))
    elif(protocol == 'J1939TP'):
        config = {'busid':0,
                  'TxFcNPdu':0x751,
                  'TXDtNPdu':0x752,
                  'TXCmNPdu':0x753,
                  'TXDirectNPdu':0x754,
                  'RXCmNPdu':0x761,
                  'RXDtNPdu':0x762,
                  'RXFcNPdu':0x763,
                  'RXDirectNPdu':0x764,
                  'STmin':10, # 10ms delay
                  }
        __dcm__ = dcm(protocal='J1939TP', **config)
        print("switch to UDS on J1939TP mode")
    elif(protocol == 'CANFD'):
        __dcm__ = dcm(protocal='CAN',busid=DFTBUS,rxid=0x732,txid=0x731,ll_dl=64)
        print("switch to UDS on CANFD mode")
    else:
        __dcm__ = dcm(protocal='CAN',busid=DFTBUS,rxid=0x732,txid=0x731)
        print("switch to UDS on CAN mode")

def Dcm_TransmitMessage(req):
    ercd,res = __dcm__.transmit(req)
    if((ercd==True) or (res is not None)):
        res2 = dcmbits()
        for d in res:
            res2.append(d, 8)
        return res2

    QMessageBox(QMessageBox.Critical, 'Error', 'Communication Error or Timeout').exec_();
    return None

def Dcm_GetResponse():
    return __dcm__.last_reponse

def Dcm_GetLastError():
    return __dcm__.last_error

class wDataUS(QComboBox):
    '''Data UxxSelect, 0<xx<=32'''
    def __init__(self,xml,parent=None):
        super(QComboBox, self).__init__(parent)
        self.Data = xml
        self.OptionInvalid= 0
        list = []
        for select in xml:
            list.append(select.attrib['name'])
            self.OptionInvalid += 1
        list.append('Invalid')
        self.addItems(list)
        
        try:
            default = str2int(xml.attrib['default'])
            self.setCurrentIndex(default)
        except:
            pass
        
    def getValue(self,data):
        index = self.currentIndex()
        
        si = 0
        svalue = None
        for select in self.Data:
            if(index == si):
                svalue = select.attrib['value']
                break
            else:
                si = si + 1
        assert(svalue)

        d = str2int(svalue)
        a = dcmbits()
        num = str2int(self.Data.attrib['type'][1:-6])
        data.append(d, num)
    
    def setValue(self,data,start):
        try:
            num = str2int(self.Data.attrib['type'][1:-6])
            value = data.toint(start,num)
            start += num
            
        except IndexError:
            QMessageBox(QMessageBox.Critical, 'Error', 'Data record witn Invalid Length  %s.'%(Dcm_GetResponse())).exec_();
            return
        index = 0
        for select in self.Data:
            if(str2int(select.attrib['value']) == value):
                break
            else:
                index += 1
        self.setCurrentIndex(index)
        return start
    
class wDataU(QLineEdit):
    '''Data Uxx UxxArray 0<xx<=32'''
    def __init__(self,xml,parent=None):
        super(QLineEdit, self).__init__(parent)
        self.Data = xml          
        try:
            self.setText(xml.attrib['default'])
        except:
            pass
    def setValue(self,data,start):
        try:
            if(self.Data.attrib['type'][-5:]=='Array'):
                num = str2int(self.Data.attrib['type'][1:-5])
                value = '[ '
                size = str2int(self.Data.attrib['size'])
                for i in range(0,size):
                    v = data.toint(start,num)
                    if(self.Data.attrib['display'] == 'hex'):
                        value += '0x%X,'%(v)
                    else:
                        value += '%d,'%(v)
                    start += num
                value = value[:-1] + ' ]'
                
            else:
                num = str2int(self.Data.attrib['type'][1:])
                value = data.toint(start,num)
                start += num
        except IndexError:
            QMessageBox(QMessageBox.Critical, 'Error', 'Data record witn Invalid Length  %s.'%(Dcm_GetResponse())).exec_();
            return
        if(self.Data.attrib['type'][-5:]=='Array'):
            self.setText(value)
        else:
            if(self.Data.attrib['display'] == 'hex'):
                self.setText('0x%X'%(value))
            else:
                self.setText('%d'%(value))
        return start
    def getValue(self,data):
        stype = self.Data.attrib['type']
        if(stype[-5:]=='Array'):
            num = str2int(stype[1:-5])
            assert(num<=32)
            size = str2int(self.Data.attrib['size'])
            string = str(self.text())
            if(string[:5]=='text='):
                va = []
                for c in string[5:]:
                    va.append(ord(c))
            else:
                string = string.replace('[', '').replace(']', '')
                grp = string.split(',')
                va = []
                for g in grp:
                    va.append(str2int(g))
            for i in range(0,size):
                try:
                    data.append(va[i],num)
                except:
                    #print(traceback.format_exc())
                    data.append(0,num)
        else:
            d = str2int(str(self.text()))
            num = str2int(stype[1:])
            assert(num<=32)
            data.append(d,num)

class UIInputOutputControl(QGroupBox):
    def __init__(self,xml,parent=None):
        super(QGroupBox, self).__init__(xml.attrib['name'],parent)
        self.IOC = xml
        grid = QGridLayout()
        
        self.leDatas = []
        row=0; col=0;
        for data in xml:
            if(data.attrib['type'][-6:] == 'Select'):
                leData = wDataUS(data)
            else:
                leData = wDataU(data)
            self.leDatas.append(leData)
            grid.addWidget(QLabel(data.attrib['name']), row, col+0)
            grid.addWidget(leData, row, col+1)
            col += 2
            
            if(col >= 8):
                row += 1
                col = 0
        row += 1
            
        self.btnStart = QPushButton('Start')
        self.btnRtce = QPushButton('Return Ctrl to ECU')
        grid.addWidget(self.btnStart, 2, 0)
        grid.addWidget(self.btnRtce, 2, 1)
        self.btnStart.clicked.connect(self.on_btnStart_clicked)
        self.btnRtce.clicked.connect(self.on_btnRtce_clicked) 
        
        self.setLayout(grid)
        
    def on_btnStart_clicked(self):
        data = dcmbits()
        data.append(0x2F,8)
        did = str2int(self.IOC.attrib['ID'])
        data.append(did,16)
        data.append(0x03,8)
        
        for leData in self.leDatas:
            leData.getValue(data)

        res = Dcm_TransmitMessage(data.toarray()) 
        if(res==None):return
        if(res.toarray()[0]!=0x6F):
            QMessageBox(QMessageBox.Critical, 'Error', 'IOC Start Failed!  %s.'%(Dcm_GetLastError())).exec_();
              
    def on_btnRtce_clicked(self):
        data = dcmbits()
        data.append(0x2F,8)
        did = str2int(self.IOC.attrib['ID'])
        data.append(did,16)
        data.append(0x00,8)
        res = Dcm_TransmitMessage(data.toarray())  
        if(res==None):return 
        if(res.toarray()[0]!=0x6F):
            QMessageBox(QMessageBox.Critical, 'Error', 'IOC Return Control to ECU Failed!  %s.'%(Dcm_GetLastError())).exec_();

class UIDataIdentifier(QGroupBox):
    def __init__(self,xml,parent=None):
        super(QGroupBox, self).__init__(xml.attrib['name'],parent)
        self.DID = xml
        grid = QGridLayout()
        
        self.leDatas = []
        row=0; col=0;
        for data in xml:
            if(data.attrib['type'][-6:] == 'Select'):
                leData = wDataUS(data)
            else:
                leData = wDataU(data)
            self.leDatas.append(leData)
            grid.addWidget(QLabel(data.attrib['name']), row, col+0)
            grid.addWidget(leData, row, col+1)
            col += 2
            
            if(col >= 8):
                row += 1
                col = 0
        row += 1      
        self.btnRead = QPushButton('Read')
        self.btnWrite = QPushButton('Write')
        grid.addWidget(self.btnRead, row, 0)
        grid.addWidget(self.btnWrite, row, 1)
        self.btnRead.clicked.connect(self.on_btnRead_clicked)
        self.btnWrite.clicked.connect(self.on_btnWrite_clicked)
        
        if(xml.attrib['attribute']=='r'): # read-only
            self.btnWrite.setDisabled(True)
            
        if(xml.attrib['attribute']=='w'): # write-only
            self.btnRead.setDisabled(True)
        
        self.setLayout(grid)
    def on_btnRead_clicked(self):
        data = dcmbits()
        data.append(0x22,8)
        did = str2int(self.DID.attrib['ID'])
        data.append(did,16)
       
        res = Dcm_TransmitMessage(data.toarray()) 
        if(res==None):return
        start = 3
        if(res.toarray()[0]!=0x62):
            QMessageBox(QMessageBox.Critical, 'Error', 'DID Start Failed!  %s.'%(Dcm_GetLastError())).exec_();
        else:
            for leData in self.leDatas:
                start = leData.setValue(res,start)
    def on_btnWrite_clicked(self):
        data = dcmbits()
        data.append(0x2E,8)
        did = str2int(self.DID.attrib['ID'])
        data.append(did,16)
        for leData in self.leDatas:
            leData.getValue(data)
        res = Dcm_TransmitMessage(data.toarray())  
        if(res==None):return 
        if(res.toarray()[0]!=0x6E):
            QMessageBox(QMessageBox.Critical, 'Error', 'DID Write Failed!  %s.'%(Dcm_GetLastError())).exec_();

class UIRoutineControl(QGroupBox):
    def __init__(self,xml,parent=None):
        super(QGroupBox, self).__init__(xml.attrib['name'],parent)
        self.SRI = xml
        grid = QGridLayout()
        
        self.leDatas = []
        row=0; col=0;
        for data in xml:
            if(data.attrib['type'][-6:] == 'Select'):
                leData = wDataUS(data)
            else:
                leData = wDataU(data)
            self.leDatas.append(leData)
            grid.addWidget(QLabel(data.attrib['name']), row, col+0)
            grid.addWidget(leData, row, col+1)
            col += 2
            
            if(col >= 8):
                row += 1
                col = 0
        row += 1      
        
        grid.addWidget(QLabel('Result:'), row, 0)
        self.leResult = QLineEdit('No Result')
        self.leResult.setEnabled(False)
        grid.addWidget(self.leResult, row, 1)
            
        self.btnStart = QPushButton('Start')
        self.btnStop = QPushButton('Stop')
        self.btnResult = QPushButton('Result')
        grid.addWidget(self.btnStart, row+1, 0)
        grid.addWidget(self.btnStop, row+1, 1)
        grid.addWidget(self.btnResult, row+1, 2)
        
        self.btnStart.clicked.connect(self.on_btnStart_clicked)
        self.btnStop.clicked.connect(self.on_btnStop_clicked)
        self.btnResult.clicked.connect(self.on_btnResult_clicked)
        
        try:
            if(xml.attrib['stop']=='true'):
                self.btnStop.setDisabled(False)
            else:
                self.btnStop.setDisabled(True)
        except:
            self.btnStop.setDisabled(True)
            
        try:
            if(xml.attrib['result']=='true'):
                self.btnResult.setDisabled(False)
            else:
                self.btnResult.setDisabled(True)
        except:
            self.btnResult.setDisabled(True)
        
        self.setLayout(grid)
        
    def on_btnStart_clicked(self):
        data = dcmbits()
        data.append(0x3101, 16)
        did = str2int(self.SRI.attrib['ID'])
        data.append(did,16)
        
        for leData in self.leDatas:
            leData.getValue(data)

        res = Dcm_TransmitMessage(data.toarray()) 
        if(res==None):return
        if(res.toarray()[0]!=0x71):
            QMessageBox(QMessageBox.Critical, 'Error', 'SRI Start Failed!  %s.'%(Dcm_GetLastError())).exec_();
        else:
            self.leResult.setText('Please Click Button \'Result\' to Read the Result.')
    
    def on_btnStop_clicked(self):
        data = dcmbits()
        data.append(0x3102, 16)
        did = str2int(self.SRI.attrib['ID'])
        data.append(did,16)
        res = Dcm_TransmitMessage(data.toarray())  
        if(res==None):return 
        if(res.toarray()[0]!=0x71):
            QMessageBox(QMessageBox.Critical, 'Error', 'SRI Stop Failed!  %s.'%(Dcm_GetLastError())).exec_();   
    def on_btnResult_clicked(self):
        data = dcmbits()
        data.append(0x3103,16)
        did = str2int(self.SRI.attrib['ID'])
        data.append(did,16)
        res = Dcm_TransmitMessage(data.toarray())  
        if(res==None):return 
        if(res.toarray()[0]!=0x71):
            QMessageBox(QMessageBox.Critical, 'Error', 'SRI Request Result Failed!  %s.'%(Dcm_GetLastError())).exec_();
        else:
            self.leResult.setText('check it by the raw response: %s'%(FormatMessage(res)))

class UISessionControl(QGroupBox):
    def __init__(self,xml,parent=None):
        super(QGroupBox, self).__init__(xml.tag,parent)
        self.XML = xml
        self.cmbxSessions = QComboBox()
        for obj in xml:
            self.cmbxSessions.addItem(obj.attrib['name'])
        grid = QGridLayout()
        grid.addWidget(QLabel('Session:'), 0, 0)
        self.btnEnter = QPushButton('Enter')
        grid.addWidget(self.cmbxSessions,0,1)
        grid.addWidget(self.btnEnter,0,2)
        self.setLayout(grid)
        self.btnEnter.clicked.connect(self.on_btnEnter_clicked)

    def on_btnEnter_clicked(self):
        data = dcmbits()
        data.append(0x10,8)
        session = str(self.cmbxSessions.currentText())
        for obj in self.XML:
            if(session==obj.attrib['name']):
                data.append(str2int(obj.attrib['id']),8)
                break
        res = Dcm_TransmitMessage(data.toarray())  
        if(res==None):return 
        if(res.toarray()[0]!=0x50):
            QMessageBox(QMessageBox.Critical, 'Error', 'SessionControl Failed!  %s.'%(Dcm_GetLastError())).exec_()
        else:
            QMessageBox(QMessageBox.Information,'Info', 'SessionControl okay with response %s'%(FormatMessage(res))).exec_()

class UISecurityAccess(QGroupBox):
    def __init__(self,xml,parent=None):
        super(QGroupBox, self).__init__(xml.tag,parent)
        self.XML = xml
        self.cmbxSecurityLevels = QComboBox()
        for obj in xml:
            self.cmbxSecurityLevels.addItem(obj.attrib['name'])
        grid = QGridLayout()
        grid.addWidget(QLabel('Security Level:'), 0, 0)
        self.btnUnlock = QPushButton('Unlock')
        grid.addWidget(self.cmbxSecurityLevels,0,1)
        grid.addWidget(self.btnUnlock,0,2)
        self.setLayout(grid)
        self.btnUnlock.clicked.connect(self.on_btnUnlock_clicked)

    def Seed2Key(self, Level, res):
        fp = open('./SeedToKey.py','w')
        alg = Level.attrib['algorithm']
        if(alg.startswith('def')):
            alg = alg.replace('\\n','\n')
            alg = alg.replace('$LSL','>>')
            alg = alg.replace('$LSR','<<')
            alg = alg.replace('$AND','&')
            fp.write(alg)
            fp.close()
            import SeedToKey
            return SeedToKey.CalculateKey(res.toarray())
        else:
            assert(0)

    def on_btnUnlock_clicked(self):
        data = dcmbits()
        data.append(0x27,8)
        level = str(self.cmbxSecurityLevels.currentText())
        for obj in self.XML:
            if(level==obj.attrib['name']):
                levelV = str2int(obj.attrib['id'])
                Level = obj
                break
        data.append(levelV,8)
        res = Dcm_TransmitMessage(data.toarray())  
        if(res==None):return 
        if(res.toarray()[0]!=0x67):
            QMessageBox(QMessageBox.Critical, 'Error', 'SecurityAccess request seed Failed!  %s.'%(Dcm_GetLastError())).exec_();
            return
        unlocked = True
        for v in res.toarray()[2:]:
            if(v != 0):
                unlocked = False
        if(unlocked):
            QMessageBox(QMessageBox.Information,'Info', 'SecurityAccess okay with 0 seed, already unlocked!').exec_()
            return
        data = dcmbits()
        data.append(0x27,8)
        data.append(levelV+1,8)
        key = self.Seed2Key(Level,res)
        for v in key:
            data.append(v,8)
        res = Dcm_TransmitMessage(data.toarray())  
        if(res==None):return 
        if(res.toarray()[0]!=0x67):
            QMessageBox(QMessageBox.Critical, 'Error', 'SecurityAccess send key Failed!  %s.'%(Dcm_GetLastError())).exec_()
        else:
            QMessageBox(QMessageBox.Information,'Info', 'SecurityAccess okay with response %s'%(FormatMessage(res))).exec_()

class UIDTC(QGroupBox):
    def __init__(self,xml,parent=None):
        super(QGroupBox, self).__init__(xml.tag,parent)
        self.XML = xml
        self.subfn=[self.reportNumberOfDTCByStatusMask,
                    self.reportNumberOfDTCBySeverityMaskRecord,
                    self.reportNumberOfMirrorMemoryDTCByStatusMask,
                    self.reportNumberOfEmissionRelatedOBDDTCByStatusMask,
                    self.reportDTCByStatusMask,
                    ]
        self.subfn_leParam_defaut=['0xFF',
                    '[0xE0,0xFF]',
                    '0xFF',
                    '0xFF',
                    '0xFF',
                    ]
        self.cmbxSubfn = QComboBox()
        for fn in self.subfn:
            self.cmbxSubfn.addItem(fn.__name__)
        vbox = QVBoxLayout()
        grid = QGridLayout()
        grid.addWidget(QLabel('Function:'), 0, 0)
        self.btnRead = QPushButton('Read')
        grid.addWidget(self.cmbxSubfn,0,1)
        self.leParam = QLineEdit('0xFF')
        self.leParam.setToolTip('''StatusMask: 8 bits, eg 0xFF
SeverityMask:16 bits [8 bits SeverityMask, 8 bits StatusMask], eg [0xFF,0xFF]''')
        grid.addWidget(self.leParam,0,2)
        grid.addWidget(self.btnRead,0,3)
        vbox.addLayout(grid)
        self.txtInfo=QTextEdit()
        self.txtInfo.setMinimumSize(600, 400)
        self.txtInfo.setReadOnly(True)
        vbox.addWidget(self.txtInfo)
        self.setLayout(vbox)
        self.btnRead.clicked.connect(self.on_btnRead_clicked)
        self.cmbxSubfn.currentIndexChanged.connect(self.on_cmbxSubfn_currentIndexChanged)

    def on_cmbxSubfn_currentIndexChanged(self,index):
        self.leParam.setText(self.subfn_leParam_defaut[index])

    def addInfo(self,info):
        self.txtInfo.append(info)

    def reportDTCCommon(self,req=[]):
        data = dcmbits()
        data.append(0x19,8)
        for v in req:
            data.append(v,8)
        res = Dcm_TransmitMessage(data.toarray())  
        if(res==None):return None
        if(res.toarray()[0]!=0x59):
            QMessageBox(QMessageBox.Critical, 'Error', 'Read DTC Failed! %s.'%(Dcm_GetLastError())).exec_()
            return None
        else:
            return res

    def showNumberOfDTCCommon(self,res):
        res = res.toarray()
        self.addInfo('\tStatusAvailabilityMask = 0x%02X'%(res[2]))
        self.addInfo('\tFormatIdentifier       = 0x%02X'%(res[3]))
        self.addInfo('\tNumber                 = %s'%((res[4]<<8)+res[5]))

    def reportNumberOfDTCByStatusMask(self,fnid=0x01):
        statusMask = str2int(str(self.leParam.text()))
        res = self.reportDTCCommon([fnid,statusMask]) 
        if(res==None):return
        self.showNumberOfDTCCommon(res)

    def reportNumberOfDTCBySeverityMaskRecord(self):
        reMatch = re.compile(r'^\s*\[\s*(\w+)\s*,\s*(\w+)\s*\]\s*$')
        data = [0x07]
        param = str(self.leParam.text())
        m = reMatch.search(param)
        if(m):
            data.append(str2int(m.groups()[0])) # SeverityMask
            data.append(str2int(m.groups()[1])) # StatusMask
        else:
            QMessageBox(QMessageBox.Critical, 'Error', 'Invalid parameter!').exec_()
            return
        res = self.reportDTCCommon(data) 
        if(res==None):return
        self.showNumberOfDTCCommon(res)

    def reportNumberOfMirrorMemoryDTCByStatusMask(self):
        self.reportNumberOfDTCByStatusMask(0x11)

    def reportNumberOfEmissionRelatedOBDDTCByStatusMask(self):
        self.reportNumberOfDTCByStatusMask(0x12)

    def strStatusMask(self,mask):
        ss = ''
        if(mask&0x01):
            ss += '\n\t\tTEST_FAILED'
        if(mask&0x02):
            ss += '\n\t\tTEST_FAILED_THIS_OPERATION_CYCLE'
        if(mask&0x04):
            ss += '\n\t\tPENDING_DTC,'
        if(mask&0x08):
            ss += '\n\t\tCONFIRMED_DTC'
        if(mask&0x10):
            ss += '\n\t\tTEST_NOT_COMPLETED_SINCE_LAST_CLEAR'
        if(mask&0x20):
            ss += '\n\t\tTEST_FAILED_SINCE_LAST_CLEAR'
        if(mask&0x40):
            ss += '\n\t\tTEST_NOT_COMPLETED_THIS_OPERATION_CYCLE'
        if(mask&0x80):
            ss += '\n\t\tWARNING_INDICATOR_REQUESTED'
        return ss

    def strDtcName(self,id):
        for dtc in self.XML:
            if(str2int(dtc.attrib['ID'])==id):
                return dtc.attrib['name']
        return 'unknown'

    def showDTCCommon(self,res):
        res = res.toarray()
        self.addInfo('\tStatusAvailabilityMask = 0x%02X'%(res[2]))
        for dtc in range(int((len(res)-3)/4)):
            id = (res[3+4*dtc]<<16)+(res[3+4*dtc+1]<<8)+(res[3+4*dtc+2]<<0)
            status = res[3+4*dtc+3]
            self.addInfo('\tDTC ID=0x%06X (%s)'%(id,self.strDtcName(id)))
            self.addInfo('\tStatusMask = 0x%02X%s'%(status,self.strStatusMask(status)))

    def reportDTCByStatusMask(self):
        statusMask = str2int(str(self.leParam.text()))
        res = self.reportDTCCommon([0x02,statusMask]) 
        if(res==None):return
        self.showDTCCommon(res)

    def on_btnRead_clicked(self):
        fn = str(self.cmbxSubfn.currentText())
        for f in self.subfn:
            if(f.__name__ == fn):
                self.addInfo('%s:'%(fn)) 
                f()
                break

class UIGroup(QScrollArea):
    def __init__(self, xml,parent=None):
        super(QScrollArea, self).__init__(parent)
        wd = QWidget()
        vBox = QVBoxLayout()
        for service in xml:
            if(service.tag=='InputOutputControl'):
                vBox.addWidget(UIInputOutputControl(service))
            elif(service.tag=='DataIdentifier'):
                vBox.addWidget(UIDataIdentifier(service))
            elif(service.tag=='RoutineControl'):
                vBox.addWidget(UIRoutineControl(service))
            elif(service.tag=='SessionControl'):
                vBox.addWidget(UISessionControl(service))
            elif(service.tag=='SecurityAccess'):
                vBox.addWidget(UISecurityAccess(service))
            elif(service.tag=='DTC'):
                vBox.addWidget(UIDTC(service))
        wd.setLayout(vBox)
        self.setWidget(wd)

class UIDcm(QWidget):
    def __init__(self, parent=None):
        super(QWidget, self).__init__(parent)
        self.vbox = QVBoxLayout()
        
        grid = QGridLayout()
        grid.addWidget(QLabel('load diagnostic description file:'),0,0)
        self.leDml= QLineEdit()
        grid.addWidget(self.leDml,0,1)
        self.btnOpenDml = QPushButton('...')
        grid.addWidget(self.btnOpenDml,0,2)

        grid.addWidget(QLabel("Protocol"),1,0)
        self.cmbxProtocol = QComboBox()
        self.cmbxProtocol.addItems(['CAN','CANFD','J1939TP','DOIP'])
        grid.addWidget(self.cmbxProtocol,1,1)

        self.cbxTesterPresent = QCheckBox("Tester Present")
        grid.addWidget(self.cbxTesterPresent,1,3)

        self.vbox.addLayout(grid)
        self.tabWidget = QTabWidget(self)
        self.vbox.addWidget(self.tabWidget)
        self.setLayout(self.vbox)
        
        default_dml = os.path.abspath('%s/../../../com/as.application/common/diagnostic.dml'%(os.curdir))

        self.leDml.setText(default_dml)
        self.loadDml(default_dml)

        self.btnOpenDml.clicked.connect(self.on_btnOpenDml_clicked)
        self.cmbxProtocol.currentIndexChanged.connect(self.on_cmbxProtocol_currentIndexChanged)
        self.cbxTesterPresent.stateChanged.connect(self.on_cbxTesterPresent_stateChanged)

    def on_cmbxProtocol_currentIndexChanged(self,index):
        protocol = str(self.cmbxProtocol.currentText())
        switch_to_protocol(protocol)

    def on_cbxTesterPresent_stateChanged(self,state):
        if(state):
            self.TPtimer = self.startTimer(3000)
        else:
            self.killTimer(self.TPtimer)

    def timerEvent(self,event):
        Dcm_TransmitMessage([0x3e,0x80])

    def loadDml(self,dml):
        self.tabWidget.clear()
        root = ET.parse(dml).getroot()
        for mm in root:
            self.tabWidget.addTab(UIGroup(mm), mm.tag)
        
    def on_btnOpenDml_clicked(self):
        rv = QFileDialog.getOpenFileName(None,'diagnostic description file', '','diagnostic description file (*.dml)')
        if(rv[0] != ''):
            self.leDml.setText(rv[0])
            self.loadDml(rv[0])

if(__name__ == '__main__'):
    a = dcmbits()
    a.append(0xdeadbeef,32)
    print(a.toarray())
    print(a.tovalue(4, 4))
    