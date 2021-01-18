import sys,os
import autosar

class LinApp(autosar.Template):
   @classmethod
   def apply(cls, ws):
      componentName = cls.__name__
      package = ws.getComponentTypePackage()
      if package.find(componentName) is None:
         swc = package.createApplicationSoftwareComponent(componentName)
         cls.addPorts(swc)
         cls.addBehavior(swc)

   @classmethod
   def addPorts(cls, swc):
      componentName = cls.__name__


   @classmethod
   def addBehavior(cls, swc):
      componentName = cls.__name__
      swc.behavior.createRunnable(componentName+'_Init')
      swc.behavior.createRunnable(componentName+'_Exit')
      swc.behavior.createRunnable(componentName+'_Run', portAccess=[p.url for p in swc.requirePorts+swc.providePorts])
      swc.behavior.createTimerEvent(componentName+'_Run', 20)

if(__name__ == '__main__'):
    autosar.asSWCGen(LinApp)

