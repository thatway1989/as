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
try:
    from .AS import *
except:
    from AS import *

import time
import json

__all__ = ['aws']

dev__ = asdev()
index__=0

class aws():
    def server(self,uri,port):
        global index__,dev__
        #device='websock%d'%(index__)
        device='websock'
        option='%s\0%d\0%d'%(uri,port,1)
        print("aws device: ",device)
        print("aws option: ",option)
        index__+=1
        print("aws: ",dev__)
        return dev__.open(device.encode('utf-8'), option.encode('utf-8'))

    def client(self,uri,port):
        global index__,dev__
        device='websock%d'%(index__)
        option='%s\0%d\0%d'%(uri,port,0)
        index__+=1
        return dev__.open(device.encode('utf-8'), option.encode('utf-8'))

    def call(self,fd,api,verb,obj):
        global dev__
        ds = 'c%s\0%s\0%s'%(api,verb,json.dumps(obj))
        return dev__.write(fd,ds.encode('utf-8'),-1)

    def pend(self,fd):
        global dev__
        data = None
        while(True):
            len,data= dev__.read(fd)
    
            if(len > 0):
                break
            else:
                time.sleep(0.001) # sleep 1 ms

        return json.loads(str(data, encoding = "utf-8"))

    def poll(self,fd):
        global dev__
        len,data= dev__.read(fd)
    
        if(len > 0):
            return json.loads(str(data, encoding = "utf-8"))

        return None

    def reply(self,fd,msg,robj):
        self.reply_okay(fd,msg,robj)

    def reply_okay(self,fd,msg,robj):
        global dev__
        ds = 'r%s\0%s\0%s\0%s'%('okay', json.dumps(robj), msg['param'], msg['msg'])
        return dev__.write(fd,ds.encode('utf-8'),-1)

    def reply_fail(self,fd,msg,robj):
        global dev__
        ds = 'r%s\0%s\0%s\0%s'%('fail', json.dumps(robj), msg['param'], msg['msg'])
        return dev__.write(fd,ds.encode('utf-8'),-1)

    def close(self,fd):
        global dev__
        dev__.close(fd)

if __name__ == '__main__':
    aw = aws()

    s = aw.server('127.0.0.1',8080)
    c = aw.client('127.0.0.1',8080)
    aw.call(c,'hello','ping',{ "hi":"this is an example call" })
    msg = aw.pend(s)
    print(msg)
    robj = {'hi':'this is an example reply'}
    aw.reply(s,msg,robj)
    msg = aw.pend(c)
    print(msg)

