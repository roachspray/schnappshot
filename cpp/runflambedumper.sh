#!/bin/bash

#GDB="gdb --args"
GDB=
SNAPDIR=./pause
#CONT=no
CONT=

callqaddr=`objdump --disassemble=main ./ex_tracee | egrep '<wonk>' | head -n 1 | cut -d ':' -f 1 | sed -e 's/ //g'`
printf "callq wonk addr= 0x%s\n" ${callqaddr}

LD_LIBRARY_PATH=/home/arr/xed/kits/xed-install-base-2020-07-03-lin-x86-64/lib  \
  ./flambe -d ${SNAPDIR} 0x${callqaddr} ./ex_tracee JABBERJAW

exit 0
