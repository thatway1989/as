__h__ = '''
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
from . import ascyacc
import sys,os

__all__ = ['ASCP']

__self_node_name__ = 'AS'

def get_period(p,msg):
    id = msg['bo']['id']
    if('baList' not in p):
        return None
    for ba in p['baList']:
        if((ba[1]=='"GenMsgCycleTime"') and (ba[2]=='BO_')):
            if(ba[3]==id):
                return ba[4]
    return None

def get_init(p,sig):
    name = sig['sg']['name']
    if('baList' not in p):
        return None
    for ba in p['baList']:
        if((ba[1]=='"GenSigStartValue"') and (ba[2]=='SG_')):
            if(ba[4]==name):
                return ba[5]
    return None

def get_comment(p,sig):
    name = sig['sg']['name']
    if('cmList' not in p):
        return None
    for cm in p['cmList']:
        if((cm[1]=='SG_') and (cm[3]==name)):
            return cm[4]
    return None
def post_process_period(p):
    for msg in p['boList']:
        period = get_period(p,msg)
        if(period!=None):
            msg['bo'].update({'period':period})

def post_process_init(p):
    for msg in p['boList']:
        for sig in msg['bo']['sgList']:
            init = get_init(p,sig)
            if(init!=None):
                sig['sg'].update({'init':init})
            comment = get_comment(p,sig)
            if(comment!=None):
                sig['sg'].update({'comment':comment})

def parse(file):
    fp = open(file,'r')
    data = fp.read()
    fp.close()
    p = ascyacc.parse(data)
    post_process_period(p)
    post_process_init(p)
    return p

def generateComH(p):
    fp = open('Com_Cfg.h','w')
    fp.write(__h__)
    fp.write('\n#ifndef COM_CFG_H\n#define COM_CFG_H\n\n')
    fp.write('/* ============================ [ INCLUDES  ] ====================================================== */\n')
    fp.write('#include "Std_Types.h"\n\n')
    fp.write('/* ============================ [ MACROS    ] ====================================================== */\n')
    fp.write('/* ============================ [ TYPES     ] ====================================================== */\n')
    for msg in p['boList']:
        msg = msg['bo']
        cstr = 'typedef struct \n{\n'
        for sig in msg['sgList']:
            sig = sig['sg']
            if(sig['size'] <= 8):
                cstr += '\tuint8   %s;\n'%(sig['name'])
            elif(sig['size'] <= 16):
                cstr += '\tuint16  %s;\n'%(sig['name'])
            elif(sig['size'] <= 32):
                cstr += '\tuint32  %s;\n'%(sig['name'])
            else:
                raise Exception('signal %s invalid size'%(sig)) 
        cstr += '\tboolean missing;\n'                               
        cstr += '} Com_Msg%sType;\n'%(msg['name'])
        fp.write(cstr)
    fp.write('/* ============================ [ DATAS     ] ====================================================== */\n')
    fp.write('/* ============================ [ DECLARES  ] ====================================================== */\n')
    fp.write('/* ============================ [ LOCALS    ] ====================================================== */\n')
    fp.write('/* ============================ [ FUNCTIONS ] ====================================================== */\n')
    for msg in p['boList']:
        msg = msg['bo']
        if(msg['node'] == __self_node_name__):
            fp.write('extern void Com_Msg%sRxIndication ( uint8* data, uint8 length );\n'%(msg['name']))
        else:
            fp.write('extern void Com_Msg%sTxRequest ( uint8* data, uint8 length );\n'%(msg['name']))
    fp.write('\n#endif\n\n')
    fp.close()
    
def generateComC(p):
    pass

def generate(p):
    generateComH(p)
    generateComC(p)

def ASCP(file,args):
    ''' AS Can Process'''
    global __self_node_name__
    __self_node_name__ = args[0]
    p = parse(file)
    generate(p)