#########################################################################
# File Name: autorun.sh
# Author: XXX
# mail: xxx@126.com
# Created Time: 2022年02月10日 星期四 16时59分26秒
#########################################################################
#!/bin/bash

export BOARD=x86
export RELEASE=ascore
//rm build -rf
//scons -c
scons
scons run
