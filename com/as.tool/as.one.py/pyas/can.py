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
import os,sys
CWD = os.path.dirname(__file__)
if(CWD==''): CWD=os.path.abspath('.')
ASROOT = os.path.abspath('%s/../../../../'%(CWD))
PYAS = '%s/build/%s/any/pyas'%(ASROOT, os.name)
if(not os.path.exists('%s/AS.%s'%(CWD, 'pyd' if os.name == 'nt' else 'so'))):
    if(os.name == 'nt'):
        cmd = 'cd %s & set BOARD=any& set ANY=pyas& scons'%(ASROOT)
        cmd += '& cp -v %s/pyas.dll %s/AS.pyd'%(PYAS, CWD)
    else:
        cmd = 'cd %s && export BOARD=any && export ANY=pyas && scons --verbose'%(ASROOT)
        cmd += ' && cp -v %s/libpyas.so %s/AS.so'%(PYAS, CWD)
    os.system(cmd)

try:
    from .AS import *
except:
    from AS import *

__all__ = ['can_open','can_write','can_read','can_close','can_reset','DFTBUS','can_set_default']

__can__ = can()

default_busid = 0

DFTBUS = 0xdeadbeef

def can_set_default(busid):
    global default_busid
    print('change default bus from %s to %s'%(default_busid, busid))
    default_busid = busid

def can_open(busid,device,port,baudrate):
    if(busid == DFTBUS): busid = default_busid
    return __can__.open(busid,device.encode('utf-8'),port,baudrate)

def can_write(busid,canid,data):
    '''can request write on can bus <busid>'''
    if(busid == DFTBUS): busid = default_busid
    return __can__.write(busid, canid, len(data), bytes(bytearray(data)))

def can_read(busid,canid):
    ''' can request read a can frame from <canid> queue of <busid>'''
    if(busid == DFTBUS): busid = default_busid
    result,canid,dlc,data= __can__.read(busid,canid)

    return result,canid,data

def can_close(busid):
    if(busid == DFTBUS): busid = default_busid
    return __can__.close(busid)

def can_reset(busid):
    if(busid == DFTBUS): busid = default_busid
    return __can__.reset(busid)


