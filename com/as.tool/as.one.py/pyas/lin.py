__lic__ = '''
/**
 * AS - the open source Automotive Software on https://github.com/parai
 *
 * Copyright (C) 2021  AS <parai@foxmail.com>
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
import os
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

__all__ = ['lin_open','lin_write','lin_read','lin_close']

dev__ = asdev()

__fdMap = {}
__lastPid = {}

def __bit(v, pos):
    return (v>>pos) & 0x01

def __get_pid(pid):
    pid = pid & 0x3F
    p0 = __bit(pid, 0) ^ __bit(pid, 1) ^ __bit(pid, 2) ^ __bit(pid, 4)
    p1 = ~(__bit(pid, 1) ^ __bit(pid, 3) ^ __bit(pid, 4) ^ __bit(pid, 5))
    pid = pid | (p0<<6) | (p1<<7);
    return pid&0xFF

def __get_checksum(pid, data, enhanced):
    checksum = 0
    if(enhanced):
        checksum += __get_pid(pid)
    for d in data:
        checksum += d
    return ((~checksum)&0xFF)

def lin_open(busid, device='socket', port='0', baudrate=115200, enhanced=True):
    if(busid not in __fdMap):
        device = 'lin/%s/%s'%(device, port)
        option = '%s'%(baudrate)
        fd = dev__.open(device.encode('utf-8'), option.encode('utf-8'))
        if(fd >= 0):
            __fdMap[busid] = (fd, enhanced)
        else:
            return False
    return True

def lin_write(busid, pid=None, data=None):
    fd, enhanced = __fdMap[busid]
    if(pid is None):
        ds = [ord('D')]
        ds.extend(data)
        checksum = __get_checksum(__lastPid[busid], data, enhanced)
        ds.append(checksum)
    elif(data is None):
        pid = __get_pid(pid)
        ds = [ord('H'), pid]
        __lastPid[busid] = pid
    else:
        pid = __get_pid(pid)
        ds = [ord('F'), pid]
        ds.extend(data)
        checksum = __get_checksum(pid, data, enhanced)
        ds.append(checksum)
        __lastPid[busid] = pid
    r = dev__.write(fd, bytes(bytearray(ds)), len(ds))
    if(r == len(ds)):
        return True
    else:
        return False

def lin_read(busid):
    fd, enhanced = __fdMap[busid]
    size,data= dev__.read(fd)
    if(size > 0):
        if((data[0] == ord('H')) and (size == 2)):
            pid = __get_pid(data[1])
            __lastPid[busid] = pid
            if(pid == data[1]):
                return True, pid&0x3F, None
            else:
                print('warning: invalid Pid', data)
        elif((data[0] == ord('F')) and (size > 3)):
            pid = __get_pid(data[1])
            __lastPid[busid] = pid
            if(pid == data[1]):
                d = []
                for i in range(size-3):
                    d.append(data[2+i])
                checksum = __get_checksum(pid, d, enhanced)
                if(checksum == data[size-1]):
                    return True, pid&0x3F, d
                else:
                    print('warning: invalid checksum', data)
            else:
                print('warning: invalid Pid', data)
        elif((data[0] == ord('D')) and (size > 2)):
            pid = __lastPid[busid]
            d = []
            for i in range(size-2):
                d.append(data[1+i])
            checksum = __get_checksum(pid, d, enhanced)
            if(checksum == data[size-1]):
                return True, pid&0x3F, d
            else:
                print('warning: invalid checksum', data)
        else:
            print('warning: invalid data', data)
    return False, None, None

def lin_close(busid):
    fd, _ = __fdMap[busid]
    dev__.close(fd)

if __name__ == '__main__':
    import time
    lin_open(0)
    lin_write(0, 0x3C, [0x11, 0x02, 0x10, 0x02, 0x55, 0x55, 0x55, 0x55])
    time.sleep(0.020)
    lin_write(0, 0x3d)
    time.sleep(1)
    ercd, pid, data = lin_read(0)
    print(ercd, pid, data)
    lin_close(0)
