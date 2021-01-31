from autosar.workspace import Workspace
import autosar.component
import autosar.behavior
import autosar.portinterface
import autosar.datatype
import autosar.constant
import autosar.signal
import autosar.package
import autosar.rte
from abc import (ABC,abstractmethod)
from autosar.base import splitRef
import autosar.bsw.com
import autosar.bsw.os

import ntpath
import os
import xml.etree.ElementTree as ElementTree

def workspace(version=3.0, patch = 2, schema=None, attributes=None, useDefaultWriters=True):
   if schema is None and ( (version == 3.0 and patch == 2) or (version == "3.0.2") ):
      schema = 'autosar_302_ext.xsd'
   return autosar.Workspace(version, patch, schema, attributes, useDefaultWriters)

def splitRef(ref):
   return autosar.base.splitRef(ref)

def DataElement(name, typeRef, isQueued=False, softwareAddressMethodRef=None, swCalibrationAccess=None, swImplPolicy = None, parent=None, adminData=None):
   return autosar.portinterface.DataElement(name, typeRef, isQueued, softwareAddressMethodRef, swCalibrationAccess, swImplPolicy, parent, adminData)

def ApplicationError(name, errorCode, parent=None, adminData=None):
   return autosar.portinterface.ApplicationError(name, errorCode, parent, adminData)

def ModeGroup(name, typeRef, parent=None, adminData=None):
   return autosar.portinterface.ModeGroup(name, typeRef, parent, adminData)

def CompuMethodConst(name, elements, parent=None, adminData=None):
   return autosar.datatype.CompuMethodConst(name, elements, parent, adminData)

def Parameter(name, typeRef, swAddressMethodRef=None, swCalibrationAccess=None, parent=None, adminData=None):
   return autosar.portinterface.Parameter(name, typeRef, swAddressMethodRef, swCalibrationAccess, parent, adminData)


#template support
class Template(ABC):
   
   usageCount = 0 #number of times this template have been applied
   
   @classmethod
   def get(cls, ws):
      ref = cls.ref(ws)
      if ws.find(ref) is None:
         ws.apply(cls)
      return ws.find(ref)
   
   @classmethod
   @abstractmethod
   def apply(cls, ws, **kwargs):
      """
      Applies this class template to the workspace ws
      """

#### Default Standard Types
def __createDataTypeFromTemplate(name, min, max):
   @classmethod
   def apply(cls, ws):
      package = ws.getDataTypePackage()
      if package.find(cls.__name__) is None:
         package.createIntegerDataType(cls.__name__, min=cls.minValue, max=cls.maxValue, offset=0, scaling=1, unit='1')
   return type(name, (autosar.Template,), dict(minValue=min, maxValue=max, apply=apply))

def __createArrayDataTypeFromTemplate(name, typeRef, arraySize=8):
   @classmethod
   def apply(cls, ws):
      package = ws.getDataTypePackage()
      if package.find(cls.__name__) is None:
         typeDef = package.find(typeRef.__name__)
         package.createArrayDataType(cls.__name__, typeDef.ref, arraySize)
   return type(name, (autosar.Template,), dict(typeRef=typeRef, apply=apply))

SINT8_T = __createDataTypeFromTemplate('SINT8_T', -128, 127)
UINT8_T = __createDataTypeFromTemplate('UINT8_T', 0, 255)
SINT16_T = __createDataTypeFromTemplate('SINT16_T', -32768, 32767)
UINT16_T = __createDataTypeFromTemplate('UINT16_T', 0, 65535)
SINT32_T = __createDataTypeFromTemplate('SINT32_T', -2147483648, 2147483647)
UINT32_T = __createDataTypeFromTemplate('UINT32_T', 0, 4294967295)
UINT8_N_T = __createArrayDataTypeFromTemplate('UINT8_N_T',UINT8_T)
UINT16_N_T = __createArrayDataTypeFromTemplate('UINT16_N_T',UINT16_T)
UINT32_N_T = __createArrayDataTypeFromTemplate('UINT32_N_T',UINT32_T)
SINT8_N_T = __createArrayDataTypeFromTemplate('SINT8_N_T',SINT8_T)
SINT16_N_T = __createArrayDataTypeFromTemplate('SINT16_N_T',SINT16_T)
SINT32_N_T = __createArrayDataTypeFromTemplate('SINT32_N_T',SINT32_T)
#### Constant Helpers ####
def createConstantTemplateFromEnumerationType(name, dataTypeTemplate, default=None):
   @classmethod
   def apply(cls, ws):
      package = ws.getConstantPackage()
      if package.find(cls.__name__) is None:
         ws.apply(cls.dataTypeTemplate)
         if(cls.default is None):
             cls.default = len(cls.dataTypeTemplate.valueTable)-1
         package.createConstant(cls.__name__, cls.dataTypeTemplate.__name__, cls.default)
   return type(name, (autosar.Template,), dict(dataTypeTemplate=dataTypeTemplate, apply=apply, default=default))

def createConstantTemplateFromPhysicalType(name, dataTypeTemplate, default=None):
   @classmethod
   def apply(cls, ws):
      package = ws.getConstantPackage()
      if package.find(cls.__name__) is None:
         ws.apply(cls.dataTypeTemplate)
         if(cls.default is None):
             cls.default = cls.dataTypeTemplate.maxValue
         package.createConstant(cls.__name__, cls.dataTypeTemplate.__name__, cls.default)
   return type(name, (autosar.Template,), dict(dataTypeTemplate=dataTypeTemplate, apply=apply, default=default))
#### DataElement Helpers ####
def createDataElementTemplate(name, dataTypeTemplate, default=None):   @classmethod
   def apply(cls, ws):
       ws.apply(cls.dataTypeTemplate)
       return DataElement(name, dataTypeTemplate.__name__)   return type(name, (autosar.Template,), dict(dataTypeTemplate=dataTypeTemplate, apply=apply))

#### Port Interface Helpers, name is the data name ####
def createSenderReceiverInterfaceTemplate(name, dataTypeTemplate, dataName=None):
   @classmethod
   def apply(cls, ws):
      package = ws.getPortInterfacePackage()
      if package.find(name) is None:
         if(cls.dataName is None):
             cls.dataName = name
         if(type(cls.dataTypeTemplate) == list):
             dataElements = []
             for dataTypeTemplate in cls.dataTypeTemplate:
                 dataElements.append(ws.apply(dataTypeTemplate))
             package.createSenderReceiverInterface(name, dataElements)
         else:
             ws.apply(cls.dataTypeTemplate)
             package.createSenderReceiverInterface(name, autosar.DataElement(cls.dataName, cls.dataTypeTemplate.__name__))
   return type(name, (autosar.Template,), dict(dataTypeTemplate=dataTypeTemplate, apply=apply, dataName=dataName))
#### Signal Helpers ####
def _createProvidePortHelper(swc, name, portInterfaceTemplate, initValueTemplate=None, elemName=None):
   ws = swc.rootWS()
   ws.apply(portInterfaceTemplate)
   if initValueTemplate is not None:
      ws.apply(initValueTemplate)
      swc.createProvidePort(name, portInterfaceTemplate.__name__, elemName, initValueRef=initValueTemplate.__name__)
   else:
      swc.createProvidePort(name, portInterfaceTemplate.__name__)

def _createRequirePortHelper(swc, name, portInterfaceTemplate, initValueTemplate=None, aliveTimeout=0, elemName=None):
   ws = swc.rootWS()
   ws.apply(portInterfaceTemplate)
   if initValueTemplate is not None:
      ws.apply(initValueTemplate)
      swc.createRequirePort(name, portInterfaceTemplate.__name__, elemName, initValueRef=initValueTemplate.__name__, aliveTimeout=aliveTimeout)
   else:
      swc.createRequirePort(name, portInterfaceTemplate.__name__, aliveTimeout=aliveTimeout)

def _createProvidePortTemplate(innerClassName, templateName, portInterfaceTemplate, initValueTemplate, elemName=None):
   @classmethod
   def apply(cls, swc):
      _createProvidePortHelper(swc, cls.name, cls.portInterfaceTemplate, cls.initValueTemplate, cls.elemName)
   return type(innerClassName, (autosar.Template,), dict(name=templateName, portInterfaceTemplate=portInterfaceTemplate, initValueTemplate=initValueTemplate, apply=apply, elemName=elemName))

def _createRequirePortTemplate(innerClassName, templateName, portInterfaceTemplate, initValueTemplate, aliveTimeout=0, elemName=None):
   @classmethod
   def apply(cls, swc):
      _createRequirePortHelper(swc, cls.name, cls.portInterfaceTemplate, cls.initValueTemplate, cls.aliveTimeout, cls.elemName)
   return type(innerClassName, (autosar.Template,), dict(name=templateName, portInterfaceTemplate=portInterfaceTemplate, initValueTemplate=initValueTemplate, aliveTimeout=aliveTimeout, apply=apply, elemName=elemName))

# name is the port name
def createSenderReceiverPortTemplate(name, portInterfaceTemplate, initValueTemplate=None, aliveTimeout=0, elemName=None):
   return type(name, (), dict(Provide=_createProvidePortTemplate('Provide', name, portInterfaceTemplate, initValueTemplate, elemName),
                              Send=_createProvidePortTemplate('Send', name, portInterfaceTemplate, initValueTemplate, elemName),
                              Require=_createRequirePortTemplate('Require', name, portInterfaceTemplate, initValueTemplate, aliveTimeout, elemName),
                              Receive=_createRequirePortTemplate('Receive', name, portInterfaceTemplate, initValueTemplate, aliveTimeout, elemName)))

def asSWCGen(cls):
    ws = autosar.workspace()
    ws.apply(cls)
    partition = autosar.rte.Partition()
    pkg = ws.getComponentTypePackage()
    partition.addComponent(pkg[cls.__name__])
    rtegen = autosar.rte.TypeGenerator(partition)
    rtegen.generate('.')
    rtegen = autosar.rte.ComponentHeaderGenerator(partition)
    rtegen.generate('.')
    print('GEN RTE Interface for',cls.__name__)