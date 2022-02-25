import os
import glob
import hashlib
import sys
import shutil
import string
import re
import cmd

bScons = True
try:
    from SCons.Script import *
except ImportError:
    bScons = False # if used by ctest.py

Env = None

BuildOptions = {}

class Win32Spawn:
    def spawn(self, sh, escape, cmd, args, env):
        # deal with the cmd build-in commands which cannot be used in
        # subprocess.Popen
        if cmd == 'del':
            for f in args[1:]:
                try:
                    os.remove(f)
                except Exception as e:
                    print('Error removing file: %s'%(e))
                    return -1
            return 0

        import subprocess

        newargs = ' '.join(args[1:])
        cmdline = cmd + " " + newargs

        # Make sure the env is constructed by strings
        _e = dict([(k, str(v)) for k, v in env.items()])

        # Windows(tm) CreateProcess does not use the env passed to it to find
        # the executables. So we have to modify our own PATH to make Popen
        # work.
        old_path = os.environ['PATH']
        os.environ['PATH'] = _e['PATH']

        try:
            _e['PATH'] = Env['EXTRAPATH']+';'+_e['PATH']
        except KeyError:
            pass

        try:
            proc = subprocess.Popen(cmdline, env=_e, shell=False)
        except Exception as e:
            print('Error in calling:\n%s'%(cmdline))
            print('Exception: %s: %s'%(e, os.strerror(e.errno)))
            return e.errno
        finally:
            os.environ['PATH'] = old_path

        return proc.wait()

def PrepareRTTHREAD(opt):
    global BuildOptions
    BuildOptions = opt

def IsPlatformWSL():
    err,infor = RunSysCmd('uname -a')
    if('Microsoft' in str(infor)):
        return True
    return False

def IsPlatformWindows():
    bYes = False
    if(os.name == 'nt'):
        bYes = True
    if(sys.platform == 'msys'):
        bYes = True
    return bYes

def AppendPythonPath(lp):
    try:
        pypath = os.environ['PYTHONPATH']
    except KeyError:
        pypath = ''
    sep = ':'
    if(IsPlatformWindows()):
        sep = ';'
    for l in lp:
        pypath += sep+os.path.abspath(l)
        sys.path.append(os.path.abspath(l))
    os.environ['PYTHONPATH'] = pypath

def PrepareEnv():
    ASROOT = os.getenv('ASPATH')
    if((ASROOT==None) or (not os.path.exists(ASROOT))):
        # loop to search the ASROOT
        p = os.curdir
        while(True):
            if(os.path.isdir('%s/com'%(p)) and 
               os.path.isdir('%s/release'%(p)) and
               os.path.isfile('%s/Console.bat'%(p))): break
            p=os.path.abspath('%s/..'%(p))
        ASROOT=p
    ASROOT = os.path.abspath(ASROOT)

    AppendPythonPath(['%s/com/as.tool/config.infrastructure.system'%(ASROOT),
              '%s/com/as.tool/config.infrastructure.system/third_party'%(ASROOT)])

    asenv=Environment(TOOLS=['ar', 'as','gcc','g++','gnulink'])
    os.environ['ASROOT'] = ASROOT
    asenv['ASROOT'] = ASROOT
    asenv['PACKAGES'] = []
    board_list = []
    any_list = []
    release_list = []
    for dir in os.listdir('%s/com/as.application'%(ASROOT)):
        if(dir[:6]=='board.' and 
           os.path.exists('%s/com/as.application/%s/SConscript'%(ASROOT,dir))):
            board_list.append(dir[6:])

    for dir in os.listdir('%s/com/as.application/board.any'%(ASROOT)):
        if(os.path.exists('%s/com/as.application/board.any/%s/SConscript'%(ASROOT,dir))):
            any_list.append(dir)

    for dir in os.listdir('%s/release'%(ASROOT)):
        if(os.path.exists('%s/release/%s/SConscript'%(ASROOT,dir))):
            release_list.append(dir)

    def help():
        if(IsPlatformWindows()):
            set = 'set'
        else:
            set = 'export'
        print('Usage:scons [studio/run]\n\tboard list: %s\n\tany   list: %s\n\trelease list: %s'%(board_list,any_list,release_list))
        print('  studio: optional for launch studio GUI tool')
        print('  run: optional for run the application on the target board')
        print('  use command "%s BOARD=board_name" to choose a board from the board list'%(set))
        print('  use command "%s ANY=any_board_name" to choose a board from the any list if BOARD is any'%(set))
        print('  use command "%s RELEASE=release_name" to choose a release from the release list'%(set))
        print('    for example, not an any board:\n\t{0} BOARD=posix\n\t{0} RELEASE=ascore'.format(set))
        print('    for example, an any board:\n\t{0} BOARD=any\n\t{0} ANY=mc9s12xep100\n\t{0} RELEASE=ascore'.format(set))

    if('help' in COMMAND_LINE_TARGETS):
        help()
        exit(0)

    BOARD = os.getenv('BOARD')
    if(BOARD not in board_list):
        print('Error: no BOARD specified!')
        help()
        exit(-1)

    if(BOARD != 'any'):
        ANY = None
    else:
        ANY = os.getenv('ANY')
        if(ANY not in any_list):
            print('Error: invalid ANY specified!')
            help()
            exit(-1)

    RELEASE = os.getenv('RELEASE')
    if(ANY in ['pyas', 'lua', 'aslib']):
        RELEASE=None
    elif(RELEASE not in release_list):
        print('Error: invalid RELEASE specified!')
        help()
        exit(-1)

    BDIR = 'build/%s/%s'%(os.name, BOARD)
    if(BOARD == 'any'):
        BDIR = '%s/%s'%(BDIR, ANY)
        TARGET = ANY
    else:
        TARGET = BOARD
    if(RELEASE!=None):
        BDIR = '%s/%s'%(BDIR, RELEASE)
    BDIR = os.path.abspath(BDIR)
    MKDir(BDIR)
    asenv['BDIR'] = BDIR
    asenv['target'] = '%s/%s'%(BDIR, TARGET)

    if(IsPlatformWindows()):
        asEnvPath = os.getenv('ASENV')
        if((asEnvPath != None) and os.path.exists(os.path.join(asEnvPath,'Console.bat'))):
            asenv['ASENV'] = asEnvPath
            print('Welcome to the world of asenv for %s!'%(ASROOT))

    asenv['BOARD'] = BOARD
    asenv['RELEASE'] = RELEASE
    Export('asenv')
    PrepareBuilding(asenv)
    return asenv

def PrepareBuilding(env):
    global Env
    Env = env
    GetConfig('%s/.config'%(env['BDIR']),env)
    env['pkgconfig'] = 'pkg-config'
    env['mingw64'] = False
    env['POSTACTION'] = []
    if(IsPlatformWindows()):
        msys2 = os.getenv('MSYS2')
        if(msys2 != None):
            mpath = os.path.abspath(msys2.replace('"',''))
        else:
            mpath = 'C:/msys64'
        err,txt = RunSysCmd('which gcc')
        if(None != err):
            print('ERROR: not msys2 enviroment!')
            exit(-1)
        gcc = os.path.abspath(mpath+txt).strip()
        gccpath = os.path.dirname(gcc)
        if('mingw64' in gcc):
            env['mingw64'] = True
        env['pkgconfig'] = '%s/pkg-config'%(gccpath)
        env['CC'] = gcc
        env['LINK'] = gcc
        env['EXTRAPATH'] = '{0};{1}/usr/bin'.format(gccpath,mpath)
    env['python3'] = 'python3'
    if(IsPlatformWindows()):
        env['python3'] = 'python'
        env.AppendENVPath('PATH', os.getenv('PATH'))
        win32_spawn = Win32Spawn()
        env['SPAWN'] = win32_spawn.spawn
    env['CXX'] = env['CC']
    # add comstr option
    AddOption('--verbose',
            dest='verbose',
            action='store_true',
            default=False,
            help='print verbose information during build')
    AddOption('--force',
            dest='force',
            action='store_true',
            default=False,
            help='force rebuild of all')
    AddOption('--menuconfig',
            dest = 'menuconfig',
            action = 'store_true',
            default = False,
            help = 'make menuconfig for Automotive Software AS')
    AddOption('--splint',
            dest = 'splint',
            action = 'store_true',
            default = False,
            help = 'enanle splint for Automotive Software AS')

    AddOption('--memory',
            dest = 'memory',
            action = 'store_true',
            default = False,
            help = 'show memory usage information')

    if(not GetOption('verbose')):
    # override the default verbose command string
        env.Replace(
          ARCOMSTR = 'AR $TARGET',
          ASCOMSTR = 'AS $SOURCE',
          ASPPCOMSTR = 'AS $SOURCE',
          CCCOMSTR = 'CC $SOURCE',
          CXXCOMSTR = 'CXX $SOURCE',
          LINKCOMSTR = 'LINK $TARGET',
          SHCCCOMSTR = 'SHCC $SOURCE',
          SHCXXCOMSTR = 'SHCXX $SOURCE',
          SHLINKCOMSTR = 'SHLINK $TARGET'
        )
    if(GetOption('menuconfig')):
        menuconfig(env)

def mk_rtconfig(filename):
    try:
        config = open(filename)
    except:
        print('open %s failed'%(filename))
        return
    rtcfg = os.path.abspath('%s/com/as.infrastructure/system/kernel/rtthread/menuconfig/%s/rtconfig.h'%(Env['ASROOT'],Env['BOARD']))
    if(not os.path.exists(rtcfg)):
        print('%s is not exits!'%(rtcfg))
    rtconfig = open(rtcfg, 'w')
    rtconfig.write('#if !defined(RT_CONFIG_H__)\n')
    rtconfig.write('#define RT_CONFIG_H__\n\n')

    empty_line = 1

    for line in config:
        line = line.lstrip(' ').replace('\n', '').replace('\r', '')

        if(len(line) == 0): continue

        if(line[0] == '#'):
            if len(line) == 1:
                if empty_line:
                    continue

                rtconfig.write('\n')
                empty_line = 1
                continue

            comment_line = line[1:]
            if(line.startswith('# CONFIG_')): line = ' ' + line[9:]
            else: line = line[1:]

            rtconfig.write('/*%s */\n' % line)
            empty_line = 0
        else:
            empty_line = 0
            setting = line.split('=')
            if(len(setting) >= 2):
                if(setting[0].startswith('CONFIG_')):
                    setting[0] = setting[0][7:]

                # remove CONFIG_PKG_XX_PATH or CONFIG_PKG_XX_VER
                if(type(setting[0]) == type('a') and (setting[0].endswith('_PATH') or setting[0].endswith('_VER'))):
                    continue

                if(setting[1] == 'y'):
                    rtconfig.write('#define %s\n' % setting[0])
                else:
                    rtconfig.write('#define %s %s\n' % (setting[0], setting[1]))

    rtconfig.write('\n')
    rtconfig.write('#endif\n')
    rtconfig.close()
    print('update %s done!'%(rtcfg))

def GetConfig(cfg,env):
    import re
    if(not os.path.exists(cfg)):
        # None to use default confiuration
        env['MODULES'] = None
        env['CONFIGS'] = None
        return
    env['MODULES'] = []
    env['CONFIGS'] = {}
    reOne = re.compile(r'([^\s]+)\s*=\s*([^\s]+)')
    fp = open(cfg)
    for el in fp.readlines():
        if(reOne.search(el)):
            name,value = reOne.search(el).groups()
            name = name.replace('CONFIG_','')
            if(value=='y'):
                env['MODULES'].append(name)
            else:
                if(value[0]=='"'): value = value[1:]
                if(value[-1]=='"'): value = value[:-1]
                env['CONFIGS'][name]=value
    fp.close()
    print('Modules:',env['MODULES'])
    print('Configs:',env['CONFIGS'])

def menuconfig(env):
    import time
    import xcc
    BDIR = env['BDIR']
    kconfig = '%s/com/as.tool/kconfig-frontends/kconfig-mconf'%(env['ASROOT'])
    if(IsPlatformWindows() and ('ASENV' in Env)):
        kconfig='%s/tools/kconfig-frontends/kconfig-mconf.exe'%(Env['ASENV'])
        cmd = 'set BOARD=%s && set ASROOT=%s && '%(env['BOARD'],env['ASROOT'])
    elif(IsPlatformWindows()):
        kconfig += '.exe'
        cmd2  = 'cd %s/com/as.tool/kconfig-frontends'%(env['ASROOT'])
        kurl = 'http://distortos.org/files/kconfig-frontends-3.12.0-windows.7z'
        cmd2 += ' && curl -O %s'%(kurl)
        for disk in ['C:/','D:/','E:/','F:/']:
            for prg in ['Program Files (x86)','Program Files','ProgramData']:
                _7z = os.path.join(disk, prg, '7-Zip/7z.exe')
                if(os.path.exists(_7z)): break
            if(os.path.exists(_7z)): break
        if(not os.path.exists(_7z)):
            raise Exception('Please Install 7z(https://www.7-zip.org/download.html)')
        cmd2 += ' && "%s" e kconfig-frontends-3.12.0-windows.7z'%(_7z)
        if(not os.path.exists(kconfig)):
            RunCommand(cmd2)
        if(not os.path.exists(kconfig)):
            raise Exception('please download %s and extract it as %s'%(kurl, kconfig))
        cmd = 'set BOARD=%s && set ASROOT=%s && '%(env['BOARD'],env['ASROOT'])
    else:
        cmd = 'export BOARD=%s && export ASROOT=%s && '%(env['BOARD'],env['ASROOT'])
    if(not os.path.exists(kconfig)):
        RunCommand('cd %s/com/as.tool/kconfig-frontends && make'%(env['ASROOT']))
    if(os.path.exists(kconfig)):
        assert(os.path.exists('Kconfig'))
        fn = '%s/.config'%(BDIR)
        cmd += 'rm -f .config && '
        if(os.path.isfile(fn)):
            cmd += 'cp -fv %s .config && '%(fn)
            mtime = os.path.getmtime(fn)
        else:
            mtime = -1
        cmd += kconfig + ' Kconfig && '
        cmd += 'cp -fv .config %s'%(fn)
        if(IsPlatformWindows()):
            cmd = '@echo off\n'+cmd.replace(' && ','\n')
            MKFile('menuconfig.bat', cmd)
            cmd = 'menuconfig.bat'
        RunCommand(cmd)
        if(os.path.isfile(fn)):
            mtime2 = os.path.getmtime(fn)
        else:
            mtime2 = -1
        if(mtime != mtime2):
            GetConfig(fn,env)
            cfgdir = '%s/config'%(BDIR)
            MKDir(cfgdir)
            xcc.XCC(cfgdir,env)
        if('RTTHREAD' in env['MODULES']):
            rtt = '%s/com/as.infrastructure/system/kernel/rtthread'%(env['ASROOT'])
            rttf = Package('https://github.com/RT-Thread/rt-thread.git')
            MKSymlink(rttf, rtt+'/rt-thread')
            fn = rtt+'/menuconfig/%s/.config'%(env['BOARD'])
            if(os.path.isfile(fn)):
                mtime = os.path.getmtime(fn)
            else:
                mtime = -1
            cmd = 'cd %s/menuconfig/%s && %s ../Kconfig'%(rtt, env['BOARD'], kconfig)
            if(IsPlatformWindows()):
                cmd = '@echo off\n'+cmd.replace(' && ','\n')
                MKFile('rtmenuconfig.bat', cmd)
                cmd = 'rtmenuconfig.bat'
            RunCommand(cmd)
            if(os.path.isfile(fn)):
                mtime2 = os.path.getmtime(fn)
            else:
                mtime2 = -1
            if(mtime != mtime2):
                mk_rtconfig(fn)
        exit(0)
    else:
        raise Exception("can't find out %s"%(kconfig))

def GetCurrentDir():
    conscript = File('SConscript')
    fn = conscript.rfile()
    path = os.path.dirname(fn.abspath)
    return path

def MKDir(p):
    ap = os.path.abspath(p)
    try:
        os.makedirs(ap)
    except:
        if(not os.path.exists(ap)):
            raise Exception('Fatal Error: can\'t create directory <%s>'%(ap))

def RMDir(p):
    if(os.path.exists(p)):
        shutil.rmtree(p)

def RMFile(p):
    if(os.path.exists(p)):
        print('removing %s'%(os.path.abspath(p)))
        os.remove(os.path.abspath(p))

def Download(url, tgt=None):
    # curl is better than wget on msys2
    if(tgt == None):
        tgt = url.split('/')[-1]
    def IsProperType(f):
        tL = {'.zip':'Zip archive data', '.tar.gz':'gzip compressed data',
              '.tar.xz':'XZ compressed data','.tar.bz2':'bzip2 compressed data'}
        if(not os.path.exists(f)):
            return False
        if(0 == os.path.getsize(f)):
            return False
        for t,v in tL.items():
            if(f.endswith(t)):
                err,info = RunSysCmd('file %s'%(tgt))
                if(v not in info):
                    return False
                break
        return True
    if(not os.path.exists(tgt)):
        print('Downloading from %s to %s'%(url, tgt))
        ret = RunCommand('curl %s -o %s'%(url,tgt), False)
        if((ret != 0) or (not IsProperType(tgt))):
            tf = url.split('/')[-1]
            RMFile(tf)
            print('temporarily saving to %s'%(os.path.abspath(tf)))
            RunCommand('wget %s'%(url))
            RunCommand('mv -v %s %s'%(tf, tgt))

def AddPackage(url, ** parameters):
    global Env
    pkg = {}
    pkg['url'] = url
    for key,v in parameters.items():
        pkg[key] = v
    Env['PACKAGES'].append(pkg)

def Package(url, ** parameters):
    if(type(url) == dict):
        parameters = url
        url = url['url']
    cwd = GetCurrentDir()
    bsw = os.path.basename(cwd)
    download = '%s/release/download'%(Env['ASROOT'])
    MKDir(download)
    pkgBaseName = os.path.basename(url)
    if(pkgBaseName.endswith('.zip')):
        tgt = '%s/%s'%(download, pkgBaseName)
        Download(url, tgt)
        pkgName = pkgBaseName[:-4]
        pkg = '%s/%s'%(download, pkgName)
        MKDir(pkg)
        flag = '%s/.unzip.done'%(pkg)
        if(not os.path.exists(flag)):
            try:
                RunCommand('cd %s && unzip ../%s'%(pkg, pkgBaseName))
            except Exception as e:
                print('WARNING:',e)
            MKFile(flag,'url')
    elif(pkgBaseName.endswith('.rar')):
        tgt = '%s/%s'%(download, pkgBaseName)
        Download(url, tgt)
        pkgName = pkgBaseName[:-4]
        pkg = '%s/%s'%(download, pkgName)
        MKDir(pkg)
        flag = '%s/.unrar.done'%(pkg)
        if(not os.path.exists(flag)):
            try:
                RunCommand('cd %s && unrar x ../%s'%(pkg, pkgBaseName))
            except Exception as e:
                print('WARNING:',e)
            MKFile(flag,'url')
    elif(pkgBaseName.endswith('.tar.gz') or pkgBaseName.endswith('.tar.xz')):
        tgt = '%s/%s'%(download, pkgBaseName)
        Download(url, tgt)
        pkgName = pkgBaseName[:-7]
        pkg = '%s/%s'%(download, pkgName)
        MKDir(pkg)
        flag = '%s/.unzip.done'%(pkg)
        if(not os.path.exists(flag)):
            RunCommand('cd %s && tar xf ../%s'%(pkg, pkgBaseName))
            MKFile(flag,'url')
    elif(pkgBaseName.endswith('.tar.bz2')):
        tgt = '%s/%s'%(download, pkgBaseName)
        Download(url, tgt)
        pkgName = pkgBaseName[:-8]
        pkg = '%s/%s'%(download, pkgName)
        MKDir(pkg)
        flag = '%s/.unzip.done'%(pkg)
        if(not os.path.exists(flag)):
            RunCommand('cd %s && tar xf ../%s'%(pkg, pkgBaseName))
            MKFile(flag,'url')
    elif(pkgBaseName.endswith('.git')):
        pkgName = pkgBaseName[:-4]
        pkg = '%s/%s'%(download, pkgName)
        if(not os.path.exists(pkg)):
            RunCommand('cd %s && git clone %s'%(download, url))
        if('version' in parameters):
            flag = '%s/.%s.version.done'%(pkg,bsw)
            if(not os.path.exists(flag)):
                ver = parameters['version']
                RunCommand('cd %s && git checkout %s'%(pkg, ver))
                MKFile(flag,ver)
                # remove all cmd Done flags
                for cmdF in Glob('%s/.*.cmd.done'%(pkg)):
                    RMFile(str(cmdF))
    else:
        pkg = '%s/%s'%(download, url)
        if(not os.path.isdir(pkg)):
            print('ERROR: %s require %s but now it is missing! It maybe downloaded later, so please try build again.'%(bsw, url))
    # cmd is generally a series of 'sed' operatiron to do some simple modifications
    if('cmd' in parameters):
        flag = '%s/.%s.cmd.done'%(pkg, bsw)
        cmd = 'cd %s && '%(pkg)
        cmd += parameters['cmd']
        if(not os.path.exists(flag)):
            RunCommand(cmd)
            MKFile(flag,cmd)
    if('pyfnc' in parameters):
        flag = '%s/.%s.pyfnc.done'%(pkg, bsw)
        if(not os.path.exists(flag)):
            parameters['pyfnc'](pkg)
            MKFile(flag)
    # post check
    verList = Glob('%s/.*.version.done'%(pkg))
    cmdList = Glob('%s/.*.cmd.done'%(pkg))
    if(len(verList) >=2 or len(cmdList) >=2):
        print('WARNING: BSW %s : 2 or more BSWs require package %s, '
              'please make sure version and cmd has no conflicts\n'
              '\t please check %s/SConscript'%(bsw, pkgBaseName, cwd))
    return pkg

def MKObject(src, tgt, cmd, rm=True):
    if(GetOption('clean') and rm):
        RMFile(tgt)
        return
    mtime = 0
    for s in src:
        s = str(s)
        if(os.path.isfile(s)):
            tm = os.path.getmtime(s)
            if(tm > mtime):
                mtime = tm
    if(os.path.isfile(tgt)):
        mtime2 = os.path.getmtime(tgt)
    else:
        mtime2 = -1
    if(mtime2 < mtime):
        RunCommand(cmd)

def MKFile(p,c='',m='w'):
    f = open(p,m)
    f.write(c)
    f.close()

def MKSymlink(src,dst):
    asrc = os.path.abspath(src)
    adst = os.path.abspath(dst)

    if(not os.path.exists(dst)):
        if(IsPlatformWindows()):
            RunSysCmd('del %s'%(adst))
            if((sys.platform == 'msys') and
               (os.getenv('MSYS') == 'winsymlinks:nativestrict')):
                RunCommand('ln -fs %s %s'%(asrc,adst))
            elif(os.path.isdir(asrc)):
                RunCommand('mklink /D %s %s'%(adst,asrc))
            else:
                RunCommand('mklink %s %s'%(adst,asrc))
        else:
            RunSysCmd('rm -f %s'%(adst))
            os.symlink(asrc,adst)

def SrcRemove(src, remove):
    if not src:
        return

    for item in src:
        if type(item) == type('str'):
            if(os.path.basename(item) in remove):
                src.remove(str(item))
        else:
            if(type(item) == list):
                for itt in item:
                    if(os.path.basename(itt.rstr()) in remove):
                        item.remove(itt)
                continue
            if(os.path.basename(item.rstr()) in remove):
                src.remove(item)

def RunCommand(cmd, e=True):
    if(GetOption('verbose')):
        print(' >> RunCommand "%s"'%(cmd))
    if(os.name == 'nt'):
        cmd = cmd.replace('&&', '&')
    ret = os.system(cmd)
    if(0 != ret and e):
        raise Exception('FAIL of RunCommand "%s" = %s'%(cmd, ret))
    return ret

def RunSysCmd(cmd):
    import subprocess
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, shell=True)
    (output, err) = p.communicate()
    p_status = p.wait()
    return err, output.decode('utf-8')

def DefineGroup(name, src, depend, **parameters):
    global Env
    if not GetDepend(depend):
        return []
    if('CCFLAGS' in parameters):
        Env.AppendUnique(CCFLAGS = parameters['CCFLAGS'])
    if('CPPPATH' in parameters):
        Env.AppendUnique(CPPPATH = parameters['CPPPATH'])
    if('CPPDEFINES' in parameters):
        Env.AppendUnique(CPPDEFINES = parameters['CPPDEFINES'])
    if('LINKFLAGS' in parameters):
        Env.AppendUnique(LINKFLAGS = parameters['LINKFLAGS'])
    objs = []    
    for obj in src:
        objs.append(Glob(str(obj)))
    return objs

def AddDepend(option):
    BuildOptions[option] = 1

def GetDepend(depend):
    building = True
    if(type(depend) == type('str')):
        if((not (depend in BuildOptions)) or (BuildOptions[depend] == 0)):
            building = False
        elif(BuildOptions[depend] != ''):
            return BuildOptions[depend]

        return building

    # for list type depend
    for item in depend:
        if(item != ''):
            if((not (item in BuildOptions)) or (BuildOptions[item] == 0)):
                building = False

    return building

def GetConfigValue(name):
    try:
        return BuildOptions[name]
    except:
        return ''

def GetELFEnv(so=True):
    global Env
    mmap = '%s/release/%s/build/%s/%s.map'%(Env['ASROOT'], Env['RELEASE'],
                Env['BOARD'],Env['BOARD'])
    cstr = ''
    if(os.path.exists(mmap) and (so==False)):
        # for executale files, should know all basic API address
        reSym = re.compile(r'^\s+(0x\w+)\s+([a-zA-Z_]\w+)$')
        ignore = ['main']
        fp = open(mmap)
        for el in fp.readlines():
            if(reSym.search(el)):
                grp = reSym.search(el).groups()
                if(grp[1] in ignore):continue
                cstr += '%s = %s;\n'%(grp[1], grp[0])
        fp.close()
    elif(so == False):
        return None
    cwd = os.path.abspath(os.path.curdir)
    if(Env.GetOption('clean')):
        RMFile('aself.lds')
    if(not os.path.exists('aself.lds')):
        MKFile('aself.lds',
               '''SECTIONS
{
  . = 0;
  .text : { *(.text*) }
  . = ALIGN(4);
  .data : { *(.data*) }
  . = ALIGN(4);
  .bss : { *(.bss*) }
  . = ALIGN(4);
  .rodata : { *(.rodata*) }
  . = ALIGN(4);
  .rel.plt : { *(.rel.*) }
}\n\n%s\n'''%(cstr), 'w')
    CPPPATH = []
    for p in Env['CPPPATH']:
        if('rt-thread' not in p):
            CPPPATH.append(p)
    env = Environment(CC=Env['CC'],
                      LINK=Env['LINK'],
                      CPPPATH=CPPPATH,
                      CCFLAGS=Env['CCFLAGS']+['-fPIC'],
                      LINKFLAGS=['-e','main','-fPIC','-s','-nostdlib','-T','%s/aself.lds'%(cwd)],
                      SHLINKFLAGS=['-fPIC','-shared','-s','-nostdlib',
                                   '-T','%s/aself.lds'%(cwd)],
                      TOOLS = Env['TOOLS'])
    for flg in ['-ffunction-sections','-fdata-sections','-g']:
        if(flg in env['CCFLAGS']):
            env['CCFLAGS'].remove(flg)
    if(not GetOption('verbose')):
    # override the default verbose command string
        env.Replace(
          ARCOMSTR = 'AR $SOURCE',
          CCCOMSTR = 'ELFCC $SOURCE',
          CXXCOMSTR = 'ELFCXX $SOURCE',
          LINKCOMSTR = 'ELFLINK $TARGET',
          SHCCCOMSTR = 'SHCC $SOURCE',
          SHCXXCOMSTR = 'SHCXX $SOURCE',
          SHLINKCOMSTR = 'SHLINK $TARGET'
        )
    if(IsPlatformWindows()):
        env['SHLINKCOM'] = '$SHLINK $SHLINKFLAGS $SOURCES -o $TARGET'
    return env

def ForkEnv(father=None, attr={}):
    if(father is None):
        father = Env
    child = Environment()
    for key,v in father.items():
        if(key == 'PACKAGES'):
            continue
        if(type(v) is list):
            child[key] = list(v)
        elif(type(v) is str):
            child[key] = str(v)
        elif(type(v) is dict):
            child[key] = dict(v)
        elif(type(v) is SCons.Util.CLVar):
            child[key] = SCons.Util.CLVar(v)
        else:
            child[key] = v
    for key,v in attr.items():
        child[key] = v
    return child

class Qemu():
    def __init__(self, qemu=None):
        arch_map = {'x86':'i386','cortex-m':'arm', 'arm64':'aarch64'}
        BDIR = Env['BDIR']
        ARCH = Env['ARCH']
        self.arch = Env['arch']
        self.port = self.FindPort()
        self.params = '-serial tcp:127.0.0.1:%s,server'%(self.port)
        self.params += '-net nic -net user,hostfwd=tcp::13400-:1234 '
        if('gdb' in COMMAND_LINE_TARGETS):
            self.params += ' -gdb tcp::1234 -S'
        if(self.arch in arch_map.keys()):
            self.arch = arch_map[self.arch]
        if(qemu is None):
            self.isAsQemu = True
            self.qemu = self.LocateASQemu()
            self.CreateDiskImg('%s/asblk0.img'%(BDIR), 32*1024*1024, 'vfat')
            self.CreateDiskImg('%s/asblk1.img'%(BDIR), 32*1024*1024, 'ext4')
        else:
            self.isAsQemu = False
            self.qemu = qemu

    def FindPort(self):
        import socket
        port = 1103
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        while(port < 2000):
            try:
                sock.bind(("127.0.0.1", port))
                break
            except:
                port += 1
        sock.close()
        return port

    def LocateASQemu(self):
        ASROOT = Env['ASROOT']
        if(IsPlatformWindows()):
            # try default install location of qemu
            qemu = '%s/qemu-system-%s'%(Env['CC'][:-3],self.arch)
            if(not os.path.exists(qemu+'.exe')):
                qemu = '%s/com/as.tool/qemu/src/build-x86_64-w64-mingw32/%s-softmmu/qemu-system-%s'%(ASROOT, self.arch, self.arch)
        else:
            qemu = '%s/release/download/qemu/%s-softmmu/qemu-system-%s'%(ASROOT, self.arch, self.arch)
        if(IsPlatformWindows()):
            qemu += '.exe'
        if(not os.path.exists(qemu)):
            print('%s is not exits, try build it out locally!'%(qemu))
            self.BuildASQemu()
        self.params += ' -device pci-ascan -device pci-asnet -device pci-asblk'
        if(IsPlatformWindows()):
            etc = os.path.abspath('%s/../etc/qemu'%(os.path.dirname(qemu)))
            self.params += ' -L %s'%(etc)
        return qemu

    def Run(self, params, where=None):
        ASROOT = Env['ASROOT']
        MODULES = Env['MODULES']
        build = Env['BDIR']
        if(where is None):
            where = build
        python = Env['python3']
        cmd = 'python main.py'
        if(IsPlatformWindows()):
            cmd = 'start ' + cmd
        else:
            cmd += ' &'
        if('asone' in COMMAND_LINE_TARGETS):
            RunCommand('cd %s/com/as.tool/as.one.py && %s'%(ASROOT,cmd))
        if(IsPlatformWindows()):
            if(self.isAsQemu and ('CAN' in MODULES)):
                RunCommand('start %s/build/%s/socket_can_driver 0'%(ASROOT, os.name))
                RunCommand('start %s/build/%s/socket_can_driver 1'%(ASROOT, os.name))
            RunCommand('cd %s && start cmd /C %s %s %s'%(where, self.qemu, params, self.params))
            RunCommand('sleep 2 && telnet 127.0.0.1 %s'%(self.port))
        else:
            fp = open('%s/telnet.sh'%(build),'w')
            fp.write('sudo modprobe vcan\nsudo ip link add dev can0 type vcan\nsudo ip link set up can0\nsudo ip link set can0 mtu 72\n')
            fp.write('sleep 0.5\ntelnet 127.0.0.1 %s\n'%(self.port))
            fp.close()
            fp = open('%s/qemu.sh'%(build),'w')
            fp.write('%s %s %s & sh %s/telnet.sh\nsleep 3600000\n'%(self.qemu,params,self.params,build))
            fp.close()
            RunCommand('sudo pgrep qemu-system-%s | xargs -i sudo kill -9 {}'%(self.arch))
            RunCommand('cd %s && chmod +x %s/*.sh && sudo gnome-terminal -x %s/qemu.sh'%(build,build,build))
        exit(0)

    def CreateDiskImg(self, file, size, type='raw'):
        ASROOT = Env['ASROOT']
        if(os.path.exists(file)):
            print('DiskImg "%s" already exist!'%(file))
            return
        print('Create a New DiskImg "%s"!'%(file))
        if(IsPlatformWindows()):
            # try default install location of qemu
            qemuimg = '%s/qemu-img'%(Env['CC'][:-3])
            if(not os.path.exists(qemuimg+'.exe')):
                qemuimg = '%s/com/as.tool/qemu/src/build-x86_64-w64-mingw32/qemu-img'%(ASROOT)
        else:
            qemuimg = '%s/release/download/qemu/qemu-img'%(ASROOT)
            if(not os.path.exists(qemuimg)):
                qemuimg = 'qemu-img'

        RunCommand('%s create -f raw %s %s'%(qemuimg, file, size))

        if(type.startswith('ext')):
            if(IsPlatformWindows()):
                lwext4mkfs = '%s/release/download/lwext4/build_generic/fs_test/lwext4-mkfs.exe'%(ASROOT)
                RunCommand('%s -i %s -b 4096 -e %s'%(lwext4mkfs,file,type[3]))
            else:
                RunCommand('sudo mkfs.%s -b 4096 %s'%(type,file))
        elif(type.startswith('vfat')):
            if(IsPlatformWindows()):
                pass # TODO
            else:
                RunCommand('sudo mkfs.fat %s'%(file))

    def BuildASQemu(self):
        ASROOT = Env['ASROOT']
        if(IsPlatformWindows()):
            RunCommand('cd %s && set BOARD=any&& set ANY=aslib&& scons'%(ASROOT))
            mpath = os.getenv('MSYS2')
            RunCommand('%s/msys2_shell.cmd -mingw64 -where %s/com/as.tool/qemu'%(mpath,ASROOT))
            print('please mannuly invoke below comand in the poped up msys2 window:')
            print('\tMINGW_INSTALLS=mingw64 makepkg-mingw -sLf')
            print('\tpacman -U mingw-w64-x86_64-qemu-2.10.0-1-any.pkg.tar.xz')
            print('and then do "scons run" again')
            exit(-1)
        else:
            fp = open('/tmp/asqemu.mk','w')
            fp.write('''download = $(prj-dir)/release/download
pkgver=2.10.0
$(download)/qemu-$(pkgver).tar.xz:
\t@(cd $(download); wget https://download.qemu.org/qemu-$(pkgver).tar.xz)

$(download)/qemu: $(download)/qemu-$(pkgver).tar.xz
\t@(cd $(download); tar xf qemu-$(pkgver).tar.xz; ln -fs qemu-$(pkgver) qemu)
\t@(sed -i "40cint memfd_create(const char *name, unsigned int flags)" $(download)/qemu/util/memfd.c)
\t@(cd $(prj-dir); BOARD=any ANY=aslib scons; cp build/%s/any/aslib/libaslib.a $(download)/qemu/hw/char/libpyas.a)
\t@(cd $(download)/qemu/hw/char; cp $(prj-dir)/com/as.tool/qemu/hw/char/* .; \
        cat Makefile >> Makefile.objs)

asqemu:$(download)/qemu
\t@(cd $(download)/qemu; ./configure --python=/usr/bin/python2; make LDFLAGS="-L/usr/lib/x86_64-linux-gnu")\n'''%(os.name))
            fp.close()
            RunCommand('make asqemu -f /tmp/asqemu.mk prj-dir=%s'%(ASROOT))

# accroding to http://benno.id.au/blog/2006/08/27/filtergensplint
# scons --splint -i  >  splint.log 2>&1
class splint():
    global Env
    def __init__(self, objs, env):
        self.update(env)

    def update(self,env):
        p = self.getit()
        for m in Env['MODULES']:
            env.Append(CPPDEFINES=['USE_%s'%(m)])
        env.Append(CPPDEFINES=['__GNUC__'])
        if(IsPlatformWindows()):
            RunCommand('set LARCH_PATH=%s/lib'%(p))
        os.environ['LARCH_PATH'] ='%s/lib'%(p)
        env['CC'] = '%s/bin/splint'%(p)
        env.Append(CFLAGS=['-badflag','-likelybool','-nestcomment','-preproc'])
        env['CCCOM'] = '$CC $CFLAGS $CCFLAGS $_CCCOMCOM $SOURCES'

    def getit(self):
        ASROOT = Env['ASROOT']
        if(IsPlatformWindows()):
            pkg = 'https://github.com/downloads/maoserr/splint_win32/splint-3.1.2.zip'
            lintdir = 'splint-3.1.2'
        else:
            pkg = 'http://www.splint.org/downloads/splint-3.1.2.src.tgz'
            lintdir = 'splint-3.1.2'
        if(not os.path.exists('%s/release/download/%s'%(ASROOT,lintdir))):
            RunCommand('cd %s/release/download && curl -O %s'%(ASROOT,pkg))
            if(IsPlatformWindows()):
                RunCommand('cd %s/release/download && unzip %s'%(ASROOT,os.path.basename(pkg)))
            else:
                RunCommand('cd %s/release/download && tar xf %s && cd %s && ./configure && make'%(ASROOT,os.path.basename(pkg),lintdir))
        return '%s/release/download/%s'%(ASROOT,lintdir)

def SelectCompilerARMICC(iarPath=None):
    import glob
    iar = iarPath
    if(iar is None):
        # try search path of IAR
        for disk in ['C:/','D:/','E:/','F:/']:
            for prg in ['Program Files (x86)','Program Files','ProgramData']:
                iar = os.path.join(disk, prg, 'IAR Systems')
                if(os.path.exists(iar)):
                    for bench in glob.glob(os.path.join(iar,'*')):
                        iar = os.path.join(bench,'arm')
                        if(os.path.exists(os.path.join(iar,'bin','iccarm.exe'))):
                            print('IAR:: %s'%(iar))
                            iarPath = iar
    if(iarPath is not None):
        print('IAR:: %s <== using this one'%(iarPath))
        Env['CC']=os.path.join(iarPath,'bin','iccarm.exe')
        Env['AS']=os.path.join(iarPath,'bin','iasmarm.exe')
        Env['LINK']=os.path.join(iarPath,'bin','ilinkarm.exe')
        Env['AR']=os.path.join(iarPath,'bin','iarchive.exe')

    return iarPath

def SelectCompilerArmNoneEabi():
    global Env
    ASROOT = Env['ASROOT']
    Env['CC']='arm-none-eabi-gcc -std=gnu99'
    Env['CXX']='arm-none-eabi-g++'
    Env['AS']='arm-none-eabi-as'
    Env['LINK']='arm-none-eabi-ld'
    Env['S19'] = 'arm-none-eabi-objcopy -O srec --srec-forceS3 --srec-len 32'
    if(IsPlatformWindows()):
        gccarm = 'https://launchpad.net/gcc-arm-embedded/5.0/5-2016-q3-update/+download/gcc-arm-none-eabi-5_4-2016q3-20160926-win32.zip'
    else:
        gccarm = 'https://launchpad.net/gcc-arm-embedded/5.0/5-2016-q3-update/+download/gcc-arm-none-eabi-5_4-2016q3-20160926-linux.tar.bz2'
    if(IsPlatformWindows() and ('ASENV' in Env)):
        cpl = '%s/tools/gcc-arm-none-eabi-5_4-2016q3-20160926-win32'%(Env['ASENV'])
    else:
        cpl = Package(gccarm)
        if(not IsPlatformWindows()):
            cpl += '/gcc-arm-none-eabi-5_4-2016q3'
    Env.Append(LIBPATH=['%s/lib/gcc/arm-none-eabi/5.4.1'%(cpl)])
    Env.Append(LIBPATH=['%s/arm-none-eabi/lib'%(cpl)])
    Env['CC']='%s/bin/arm-none-eabi-gcc -std=gnu99'%(cpl)
    Env['CXX']='%s/bin/arm-none-eabi-g++'%(cpl)
    Env['AS']='%s/bin/arm-none-eabi-gcc -c'%(cpl)
    Env['LINK']='%s/bin/arm-none-eabi-ld'%(cpl)
    Env['S19'] = '%s/bin/arm-none-eabi-objcopy -O srec --srec-forceS3 --srec-len 32'%(cpl)

def SelectCompilerArm64():
    global Env
    ASROOT = Env['ASROOT']
    if(IsPlatformWindows()):
        gccarm = 'gcc-linaro-7.2.1-2017.11-i686-mingw32_aarch64-elf.tar.xz'
    else:
        gccarm = 'gcc-linaro-7.2.1-2017.11-x86_64_aarch64-elf.tar.xz'
    pkg = Package('https://releases.linaro.org/components/toolchain/binaries/7.2-2017.11/aarch64-elf/%s'%(gccarm))
    cpl = '%s/%s'%(pkg, gccarm[:-7])
    Env['CC']='%s/bin/aarch64-elf-gcc -std=gnu99 -fno-stack-protector'%(cpl)
    Env['CXX']='%s/bin/aarch64-elf-g++'%(cpl)
    Env['AS']='%s/bin/aarch64-elf-gcc -c'%(cpl)
    Env['LINK']='%s/bin/aarch64-elf-ld'%(cpl)

def SelectCompilerX86():
    if(IsPlatformWindows()):
        gccx86='i686-elf-tools-windows.zip'
    else:
        gccx86='i686-elf-tools-linux.zip'
        print('INFO: if the downloading of toolchain is too slow,'
              'Ctrl+C and then run below command to set up the toolchain\n'
              ' mkdir -p {0}/release/download/i686-elf-tools-linux/bin\n'
              ' cd {0}/release/download/i686-elf-tools-linux/bin\n'
              ' ln -fs /usr/bin/gcc i686-elf-gcc\n'
              ' ln -fs /usr/bin/ld i686-elf-ld\n'
              ' echo native > ../../{1}\n'
              ' cd -'.format(Env['ASROOT'],gccx86))
    if(IsPlatformWindows() and ('ASENV' in Env)):
        cpl = '%s/tools/i686-elf-tools-windows'%(Env['ASENV'])
    else:
        cpl = Package('https://github.com/lordmilko/i686-elf-tools/releases/download/7.1.0/%s'%(gccx86))
    Env['CC']   = '%s/bin/i686-elf-gcc -m32 -std=gnu99 -fno-stack-protector'%(cpl)
    Env['AS']   = '%s/bin/i686-elf-gcc -m32 -c'%(cpl)
    Env['CXX']  = '%s/bin/i686-elf-g++ -m32 -fno-stack-protector'%(cpl)
    Env['LINK'] = '%s/bin/i686-elf-ld -m32 -melf_i386'%(cpl)
    Env.Append(CPPPATH=['%s/lib/gcc/i686-elf/7.1.0/include'%(cpl)])

def SelectCompilerCWCC():
    cw = os.getenv('CWCC_PATH')
    if(cw is None):
        cw = 'C:/Program Files (x86)/Freescale/CW for MPC55xx and MPC56xx 2.10'
    if(not os.path.exists(cw)):
        print('==> Please set environment CWCC_PATH\n\tset CWCC_PATH=/path/to/codewarrior_compiler')
        exit()
    cwppc = cw + '/PowerPC_EABI_Tools/Command_Line_Tools'
    Env['CC']   = cwppc + '/mwcceppc.exe'
    Env['AS']   = cwppc + '/mwaseppc.exe'
    Env['CXX']  = cwppc + '/mwcceppc.exe'
    Env['LINK'] = cwppc + '/mwldeppc.exe'
    Env.Append(ASFLAGS=['-gdwarf-2','-vle'])
    Env.Append(CCFLAGS=['-readonlystrings','-RTTI','on','-dialect','c99','-gdwarf-2',
                          '-gccext','on','-gccinc','-cwd','explicit',
                          '-opt','off','-W','most','-abi','eabi',
                          '-fp','soft','-sdata=8','-sdata2=8',
                          '-bool','on','-ipa','off','-inline','smart',
                          '-ppc_asm_to_vle','-vle'])
    Env.Append(CPPPATH=[cw + '/PA_Support/ewl/EWL_C/include',
                          cw + '/PowerPC_EABI_Support/Runtime/Include'])
    Env.Append(LIBPATH=[cw + '/PowerPC_EABI_Support/Runtime/Lib',
                          cw + '/PowerPC_EABI_Support/MSL/MSL_C/PPC_EABI/Lib'])
    Env.Append(LIBS=['Runtime.PPCEABI.VS.UC.a',
                            'MSL_C.PPCEABI.bare.VS.UC.a'])
    Env.Append(CCFLAGS=['-include','asmconfig.h'])
    Env.Append(CPPDEFINES=['USE_FAST_LEAST_TYPE_DEF','VLE_IS_ON=1','_G_va_list=va_list'])

def SelectCompilerPPCEabi():
    # http://download.ronetix.info/toolchains/powerpc/ppc_cross_development_guide.pdf
    cw = os.getenv('PPCGCC_PATH')
    if(cw is None):
        #ppc = 'C:/SysGCC/powerpc-eabi'
        ppc = 'C:/Program Files (x86)/Ronetix/powerpc-eabi'
    if(not os.path.exists(ppc)):
        print('==> Please set environment PPCGCC_PATH\n\tset PPCGCC_PATH=/path/to/powerpc-eabi')
        exit()
    Env['CC']   = ppc + '/bin/powerpc-eabi-gcc.exe'
    Env['AS']   = ppc + '/bin/powerpc-eabi-as.exe'
    Env['CXX']  = ppc + '/bin/powerpc-eabi-g++.exe'
    Env['LINK'] = ppc + '/bin/powerpc-eabi-link.exe'

def AddPythonDev(env):
    pyp = sys.executable
    if(IsPlatformWindows()):
        pyp = pyp.replace(os.sep, '/')[:-10]
        pylib = 'python'+sys.version[0]+sys.version[2]
        if(pylib in env.get('LIBS',[])): return
        pf = '%s/libs/lib%s.a'%(pyp, pylib)
        if(not os.path.exists(pf)):
            RunCommand('cp {0}/libs/{1}.lib {0}/libs/lib{1}.a'.format(pyp, pylib))
        env.Append(CPPDEFINES=['_hypot=hypot'])
        env.Append(CPPPATH=['%s/include'%(pyp)])
        env.Append(LIBPATH=['%s/libs'%(pyp)])
        istr = 'set'
        pybind11 = '%s/Lib/site-packages/pybind11/include'%(pyp)
    else:
        pyp = os.sep.join(pyp.split(os.sep)[:-2])
        if(sys.version[0:3] == '2.7'):
            _,pyp = RunSysCmd('which python3')
            pyp = os.sep.join(pyp.split(os.sep)[:-2])
            _,version = RunSysCmd('python3 -c "import sys; print(sys.version[0:3])"')
            pylib = 'python'+version+'m'
        else:
            pylib = 'python'+sys.version[0:3]+'m'
        if(pylib in env.get('LIBS',[])): return
        env.Append(CPPPATH=['%s/include/%s'%(pyp,pylib)])
        if(pyp == '/usr'):
            env.Append(LIBPATH=['%s/lib/x86_64-linux-gnu'%(pyp)])
            env.Append(CPPPATH=['%s/local/include/%s'%(pyp,pylib[:9])])
        else:
            env.Append(LIBPATH=['%s/lib'%(pyp)])
        istr = 'export'
        pybind11 = '%s/lib/%s/site-packages/pybind11/include'%(pyp, pylib[:9])
    env.Append(CPPPATH=[pybind11])
    #print('%s PYTHONHOME=%s if see error " Py_Initialize: unable to load the file system codec"'%(istr, pyp))
    env.Append(LIBS=[pylib, 'pthread', 'stdc++', 'm'])

def MemoryUsage(target, objs):
    try:
        from elftools.elf.elffile import ELFFile
    except:
        RunCommand('pip install pyelftools')
        from elftools.elf.elffile import ELFFile
    def Summary(filename):
        with open(filename, 'rb') as f:
            elf = ELFFile(f)
            summary = { 'obj': os.path.basename(filename), 'full_name': filename }
            for sec in elf.iter_sections():
                ss = sec.name.split('.')
                if(len(ss) <= 1 ): continue
                name = ss[1]
                if(name in summary):
                    summary[name] += len(sec.data())
                else:
                    summary[name] = len(sec.data())
        return summary

    secs = ['full_name','obj']
    summ = Summary(target)
    for key,_ in summ.items():
        if(key not in secs):
            secs.append(key)
    objsums = []
    for obj in objs:
        obj = str(obj)
        osumm = Summary(obj)
        objsums.append(osumm)
        for key,_ in osumm.items():
            if(key not in secs):
                secs.append(key)
    fp = open('%s.csv'%(target),'w')
    for sec in secs:
        fp.write(sec+',')
    fp.write('\n')
    for sm in objsums+[summ]:
        for sec in secs:
            if(sec in sm):
                fp.write('%s,'%(sm[sec]))
            else:
                fp.write(',')
        fp.write('\n')
    fp.close()
    care = ['obj','text','rodata','data','bss']
    print('obj                      text     rodata   data     bss')
    for sm in objsums+[summ]:
        v = []
        for sec in care:
            if(sec in sm):
                v.append(sm[sec])
            else:
                v.append(0)
        print('%-24s %-8s %-8s %-8s %-8s'%(v[0],v[1],v[2],v[3],v[4]))
    print('Please check more detailed memory usage information in %s.csv'%(target))

def BuildingSWCS(swcs):
    for swc in swcs:
        swc = str(swc)
        swc = os.path.abspath(swc)
        path = os.path.dirname(swc)
        cmd = 'cd %s && %s %s'%(path, Env['python3'], swc)
        tgt = path+'/Rte_Type.h'
        MKObject([swc], tgt, cmd)

def BuildOFS(ofs):
    for of in ofs:
        src = str(of)
        tgt = src[:-3]+'.h'
        cflags = ''
        for p in Env['CPPPATH']:
            cflags += ' -I%s'%(p)
        for d in Env['CPPDEFINES']:
            cflags += ' -D%s'%(d)
        cmd = 'cp %s .tmp.c && %s -S .tmp.c -o .tmp.S %s'%(src, Env['CC'], cflags)
        cmd += ' && sed -n "/#define/p" .tmp.S > %s'%(tgt)
        MKObject([src], tgt, cmd)

def SHA256(v):
    m = hashlib.sha256()
    if(type(v) == list):
        for vv in v:
            with open(vv) as f:
                m.update(f.read().encode('utf-8'))
    else:
        with open(v) as f:
            m.update(f.read().encode('utf-8'))
    return str(m.digest())

def PreProcess(cfgdir, fil):
    print('  PP %s'%(fil))
    filR = '%s/%s'%(cfgdir,os.path.basename(fil))
    mtime = os.path.getmtime(fil)
    if(os.path.exists(filR)):
        mtimeR = os.path.getmtime(filR)
    else:
        mtimeR = -1
    if(mtime < mtimeR):
        print('WARNING: temporary %s is newer than %s, please save it safely!'%(filR, fil))
        return filR
    filC = filR + '.h'
    MKSymlink(fil,filC)
    cmd = 'gcc -E --include %s/asmconfig.h %s'%(cfgdir, filC)
    err, txt = RunSysCmd(cmd)
    if(0 == err):
        raise Exception('gcc preprocessing %s failed:\n%s'%(fil, txt))
    newTxt = ''
    for line in txt.split('\n'):
        line = line.strip()
        if((not line.startswith('#')) and (line != '')):
            newTxt += line+'\n'
    MKFile(filR, newTxt)
    return filR

def BuildDTS(dts,BDIR):
    if(len(dts) > 0):
        dtc = Package('dtc')+'/dtc'
    for src in dts:
        src=str(src)
        src = os.path.abspath(src)
        bp = os.path.dirname(src)
        tgt = '%s/%s.dtb'%(os.path.abspath(BDIR), os.path.basename(src)[:-4])
        cmd = 'cd %s && %s -I dts -O dtb %s -o %s'%(bp, dtc, src, tgt)
        MKObject([src], tgt, cmd)

def Building(target, sobjs, env=None):
    import xcc
    import argen
    if(env is None):
        env = Env
    if(GetOption('splint')):
        splint(objs, env)
    if('BDIR' in env):
        BDIR = env['BDIR']
    else:
        BDIR = 'build/%s'%(target)
    objs = []
    xmls = []
    ofs = []
    swcs = []
    dts = []
    arxml= None

    cfgdir = '%s/config'%(BDIR)
    MKDir(cfgdir)
    env.Append(CPPPATH=['%s'%(cfgdir)])
    env.Append(ASFLAGS='-I%s'%(cfgdir))

    if('PACKAGES' in env):
        for p in env['PACKAGES']:
            pkg = Package(p)
            pbdir = '%s/packages/%s'%(BDIR,os.path.basename(pkg))
            sobjs += SConscript('%s/SConscript'%(pkg),variant_dir=pbdir, duplicate=0)

    for obj in sobjs:
        if(str(obj)[-6:]=='.arxml'):
            if(arxml is None):
                arxml = obj
            else:
                raise Exception('too much arxml specified! [%s %s]'%(arxml,obj))
        elif(str(obj)[-4:]=='.xml'):
            xmls.append(obj)
        elif(str(obj)[-3:]=='.of'):
            ofs.append(obj)
        elif(str(obj)[-3:]=='.py'):
            swcs.append(obj)
        elif(str(obj)[-4:]=='.dts'):
            dts.append(obj)
        else:
            objs.append(obj)

    AppendPythonPath([cfgdir])
    os.environ['ARXML']=str(arxml)
    cfgdone = '%s/config.done'%(cfgdir)
    forceGen = GetOption('force')
    if(os.path.exists(cfgdone) and not forceGen):
        shaN = SHA256(glob.glob('%s/*xml'%(cfgdir)))
        shaO = open(cfgdone).read()
        if(shaN != shaO):
            forceGen = True
    if( ( (not os.path.exists(cfgdone)) and (not GetOption('clean')) ) 
            or forceGen ):
        MKDir(cfgdir)
        RMFile(cfgdone)
        xcc.XCC(cfgdir, env, True)
        if(arxml != None):
            arxmlR = PreProcess(cfgdir, str(arxml))
            for xml in xmls:
                MKSymlink(str(xml),'%s/%s'%(cfgdir,os.path.basename(str(xml))))
            xcc.XCC(cfgdir)
            argen.ArGen.ArGenMain(arxmlR,cfgdir)
        MKFile(cfgdone, SHA256(glob.glob('%s/*xml'%(cfgdir))))
    if(('studio' in COMMAND_LINE_TARGETS) and (env == Env)):
        studio=os.path.abspath('%s/com/as.tool/config.infrastructure.system/'%(env['ASROOT']))
        assert(arxml)
        pd = os.path.abspath(cfgdir)
        RunCommand('cd %s && %s studio.py %s'%(studio,env['python3'],pd))
        exit(0)

    if(('gcc' in env['CC']) and os.path.exists('%s/asmconfig.h'%(cfgdir))):
        env.Append(CCFLAGS=['--include', '%s/asmconfig.h'%(cfgdir)])

    objs += Glob('%s/*.c'%(cfgdir))

    if(GetOption('clean')):
        if(os.path.exists('%s/autosar.arxml'%(cfgdir))):
            shutil.copy('%s/autosar.arxml'%(cfgdir), '%s/autosar.arxml'%(BDIR))
            shutil.rmtree(cfgdir)
            MKDir(cfgdir)
            shutil.move('%s/autosar.arxml'%(BDIR), '%s/autosar.arxml'%(cfgdir))
        RunCommand('rm -fv *.s19')

    BuildDTS(dts,BDIR)
    BuildOFS(ofs)
    BuildingSWCS(swcs)
    if('Program' in env):
        # special program for some compiler
        env['Program'](target, objs, env)
    else:
        if('BUILD_TYPE' in env):
            BUILD_TYPE = env['BUILD_TYPE']
        else:
            BUILD_TYPE = 'exe'
        if(BUILD_TYPE == 'exe'):
            env.Program(target, objs)
        elif(BUILD_TYPE == 'dll'):
            env.SharedLibrary(target, objs)
        elif(BUILD_TYPE == 'lib'):
            env.Library(target, objs)

    if(IsPlatformWindows()):target += '.exe'
    if(GetOption('memory')):
        MemoryUsage(target, env.Object(objs))
    #env['POSTACTION'].append('readelf -l %s'%(target))
    if('POSTACTION' in env):
        for action in env['POSTACTION']:
            env.AddPostAction(target, action)

if((not IsPlatformWindows()) and bScons):
    AddOption('--prepare',
              dest = 'prepare',
              action = 'store_true',
              default = False,
              help = 'prepare build env for linux')
    if(GetOption('prepare')):
        os.system('sudo apt-get install libgtk-3-dev autoconf libtool-bin curl flex bison gperf nasm '
                  'libncurses-dev libreadline-dev glib2.0 libcurl4-openssl-dev '
                  'libsdl2-dev python3-tk '
                  'python3-pyqt5 python3-sip python3-sip-dev sip-dev python3-pip net-tools')
        os.system('sudo apt-get install lib32stdc++6')    # for Ubuntu before 18.04
        os.system('sudo apt-get install libstdc++6:i386') # for Ubuntu 18.04
        os.system('sudo pip3 install pillow pyserial bitarray ply matplotlib')

