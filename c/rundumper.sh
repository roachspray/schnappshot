#!/bin/bash

SNAPDIR=./pause
#CONT=no
CONT=

callqaddr=`objdump --disassemble=main ./ex_tracee | egrep '<wonk>' | head -n 1 | cut -d ':' -f 1 | sed -e 's/ //g'`
printf "callq wonk addr= 0x%s\n" ${callqaddr}
#xoraddr=`objdump --disassemble=main ex_tracee | egrep 'xor    %r8d,%r8d' | cut -d ':' -f 1 | sed -e 's/ //g'`
#printf "xor addr= 0x%s\n" ${xoraddr}

LD_LIBRARY_PATH=/home/arr/xed/kits/xed-install-base-2020-07-03-lin-x86-64/lib  \
  ./dumper ${SNAPDIR} 0x${callqaddr} ${CONT}cont ./ex_tracee JABBERJAW

exit 0
