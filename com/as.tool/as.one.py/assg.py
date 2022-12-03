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
from tkinter import W
from PyQt5 import QtCore, QtGui
from PyQt5.QtGui import *
from PyQt5.QtCore import *
from PyQt5.QtWidgets import *
from serial import Serial
import time
from binascii import hexlify, unhexlify
import sys,os
import xml.etree.ElementTree as ET
from pyas.dcm import *
from pyas.asdev import *
import traceback,re
import time
__all__ = ['UISg']

reSgBMP = re.compile(r'SgBMP\{(\d+)\}')
reSgTXT = re.compile(r'SgTXT\{(\d+)\}')

# [x,y,Offset,Length,HeadWidth,TailWidth,Color,Start,Range]
iX          = 0
iY          = 1
iOffset     = 2
iLength     = 3
iHeadWidth  = 4
iTailWidth  = 5
iColor      = 6
iStart      = 7     # Start Degree
iRange      = 8     # Span Range Degree

cMechanicalZero = 1000 # relative to iStart, unit in 0.01 degree

class SgItem(QGraphicsItem):
    def __init__(self,path,xml):
        super(SgItem, self).__init__()
        self.xmlpath = path
        self.xml = xml
        self.cfg =[int(xml.attrib['x']),
                   int(xml.attrib['y']),
                   int(xml.attrib['h']),
                   int(xml.attrib['w'])]
        self.setFlag(QGraphicsItem.ItemSendsGeometryChanges)
        self.setCacheMode(QGraphicsItem.DeviceCoordinateCache)
        self.setZValue(1)
        self.img=[]
        self.iim = 0
        for p in xml:
            if(p.tag=='SgBMP'):
                bg = '%s/%s'%(self.xmlpath,p.attrib['locate'])
                self.img.append(QImage(bg))

        self.setPos(self.cfg[iX],self.cfg[iY])
        self.Layer = 1
        self.Degree = 0

    def boundingRect(self):
        # x,y,width,height
        cfg = self.cfg
        return QRectF(-cfg[iLength]-cfg[iOffset],-cfg[iLength]-cfg[iOffset],(cfg[iLength]+cfg[iOffset])*2,(cfg[iLength]+cfg[iOffset])*2)

    def setPosDegree(self,Degree):
        self.Degree = Degree

    def setPosLayer(self,Layer):
        self.Layer = Layer
        if(self.Layer == 255):
            self.setVisible(False);
        else:
            self.setVisible(True);

    def paint(self, painter, option, widget):
        if(self.Layer == 255):
            self.setVisible(False);
            return
        else:
            self.setVisible(True);
        Image = self.img[self.iim]
        painter.drawImage(0,0,Image)
        
class SgPointer(QGraphicsItem):
    Degree = 0  # unit in 0.01 -- 
    def __init__(self,xml):
        super(SgPointer, self).__init__()
        self.xml = xml
        self.cfg =[int(xml.attrib['x']),
                   int(xml.attrib['y']),
                   int(xml.attrib['h']), # offset
                   int(xml.attrib['w'])*4/5, # length
                   int(xml.attrib['h']), # head_width
                   int(xml.attrib['h'])/4, # tail_width
                   0x7453A2,
                   0,
                   360]
        #self.setFlag(QGraphicsItem.ItemIsMovable)
        self.setFlag(QGraphicsItem.ItemSendsGeometryChanges)
        self.setCacheMode(QGraphicsItem.DeviceCoordinateCache)
        self.setZValue(1)
        self.setPos(self.cfg[iX],self.cfg[iY])
        self.Layer = 1
        self.Degree = 0

    def boundingRect(self):
        # x,y,width,height
        cfg = self.cfg
        return QRectF(-cfg[iLength]-cfg[iOffset],-cfg[iLength]-cfg[iOffset],(cfg[iLength]+cfg[iOffset])*2,(cfg[iLength]+cfg[iOffset])*2)

    def getPosDegree(self):
        return self.Degree

    def setPosLayer(self,Layer):
        self.Layer = Layer        
        if(self.Layer == 255):
            self.setVisible(False);
        else:
            self.setVisible(True);

    def setPosDegree(self,Degree):
        cfg = self.cfg
        self.Degree = Degree+18000+cMechanicalZero
        # Set Degree 
        self.setRotation((self.Degree-cMechanicalZero)/100+cfg[iStart])

    def paint(self, painter, option, widget):
        if(self.Layer == 255):
            self.setVisible(False);
            return
        else:
            self.setVisible(True);
        cfg = self.cfg
        painter.setPen(QtCore.Qt.NoPen)
        painter.setBrush(QtGui.QBrush(QtGui.QColor((cfg[iColor]>>16)&0xFF,(cfg[iColor]>>8)&0xFF,(cfg[iColor]>>0)&0xFF)))
        points = QtGui.QPolygon([
            QtCore.QPoint(-cfg[iOffset],                 cfg[iHeadWidth]/2),
            QtCore.QPoint(-cfg[iOffset]-cfg[iLength],    cfg[iTailWidth]/2),
            QtCore.QPoint(-cfg[iOffset]-cfg[iLength],   -cfg[iTailWidth]/2),
            QtCore.QPoint(-cfg[iOffset],                -cfg[iHeadWidth]/2),
        ])  
        painter.drawConvexPolygon(points);
        if(cfg[iOffset] < 0):
            radius = -cfg[iOffset]
        elif(cfg[iOffset] > 0):
            radius = cfg[iOffset]
        else:
            radius = cfg[iHeadWidth]
        radius += 2
        painter.drawEllipse(-radius,-radius,radius*2,radius*2)
        painter.setBrush(QtGui.QBrush(QtGui.QColor(0,0,0))) #black
        radius2 = radius*2/3
        if(radius2 > cfg[iHeadWidth]):
            radius2 = radius-cfg[iHeadWidth]*2/3
        painter.drawEllipse(-radius2,-radius2,radius2*2,radius2*2)

class SgWidget(QGraphicsView):
    def __init__(self,xml):
        super(SgWidget, self).__init__()
        scene = QGraphicsScene(self)
        scene.setItemIndexMethod(QGraphicsScene.NoIndex)
        self.setScene(scene)
        self.setCacheMode(QGraphicsView.CacheBackground)
        self.setViewportUpdateMode(QGraphicsView.BoundingRectViewportUpdate)
        self.setRenderHint(QPainter.Antialiasing)
        self.setTransformationAnchor(QGraphicsView.AnchorUnderMouse)
        self.setResizeAnchor(QGraphicsView.AnchorViewCenter)
        self.loadXml(xml)

    def loadXml(self,xml):
        self.xmlpath = os.path.abspath('%s/..'%(os.path.dirname(xml)))
        self.xmlroot = ET.parse(xml).getroot()
        self.widgets = {}
        for w in self.xmlroot:
            if((w.tag != 'SgWidget')):continue
            if(w.attrib['name'][-10:]=='Background'):
                for p in w:
                    if(p.tag=='SgBMP'):bg = '%s/%s'%(self.xmlpath,p.attrib['locate'])
                self.widgets['Background'] = {'sg':QImage(bg), 'layer':0 }
                continue
            if(w.attrib['name'][-7:]=='Pointer'):
                sg = SgPointer(w)
                self.widgets[w.attrib['name']]={'sg':sg, 'layer':int(w.attrib['layer'])}
                sg.setPosDegree(cMechanicalZero)
            else:
                sg = SgItem(self.xmlpath,w)
                self.widgets[w.attrib['name']]={'sg':sg, 'layer':int(w.attrib['layer'])}
        for name, sg in sorted(self.widgets.items(),key=lambda sg: sg[1]['layer']):
            if(name=='Background'): continue
            self.scene().addItem(sg['sg'])

    def refresh(self,obj):
        for name,ite in self.widgets.items():
            if(name=='Background'): continue
            wobj = obj[name]
            wgt = ite['sg']
            wgt.setPosDegree(int(wobj['degree']))
            wgt.setPosLayer(int(wobj['layer']))

    def drawBackground(self,painter,rect ):
        Image = self.widgets['Background']['sg']
        self.scene().setSceneRect(0, 0, Image.size().width(), Image.size().height())
        painter.drawImage(0,0,Image); 

    def keyPressEvent(self, event):
        pass

class UISg(QWidget):
    def __init__(self, parent=None):
        super(QWidget, self).__init__(parent)
        self.vbox = QVBoxLayout()
        grid = QGridLayout()
        grid.addWidget(QLabel('load Sg xml file:'),0,0)
        self.leXml= QLineEdit()
        grid.addWidget(self.leXml,0,1)
        self.btnOpenXml = QPushButton('...')
        grid.addWidget(self.btnOpenXml,0,2)
        grid.addWidget(QLabel('websock server:'),1,0)
        self.leWsj= QLineEdit('127.0.0.1:8080')
        self.leWsj.setDisabled(True)
        grid.addWidget(self.leWsj,1,1)
        self.vbox.addLayout(grid)
        self.aws = aws()
        self.fd = self.aws.server('127.0.0.1',8080)
        default_xml = os.path.abspath('%s/../../../release/ascore/SgDesign/virtual_cluster/Sg.xml'%(os.curdir))
        self.sgWidget = SgWidget(default_xml)
        self.vbox.addWidget(self.sgWidget)
        self.setLayout(self.vbox)
        self.leXml.setText(default_xml)

        self.btnOpenXml.clicked.connect(self.on_btnOpenXml_clicked)
        
        self.startTimer(5)

    def timerEvent(self,event):
        msg = self.aws.poll(self.fd)
        while(msg != None):
            self.sgWidget.refresh(msg['obj'])
            self.aws.reply(self.fd,msg,{'hi':'this is as GUI daemon!'})
            msg = self.aws.poll(self.fd)

    def loadXml(self,xml):
        self.sgWidget.loadXml(xml)

    def on_btnOpenXml_clicked(self):
        rv = QFileDialog.getOpenFileName(None,'Sg xml file', '','Sg xml description file (*.xml)')
        if(rv[0] != ''):
            self.leXml.setText(rv[0])
            self.loadXml(rv[0])

if(__name__ == '__main__'):
    app = QApplication(sys.argv)
    mWain = QMainWindow()
    mWain.setCentralWidget(UISg())
    mWain.show()
    sys.exit(app.exec_()) 