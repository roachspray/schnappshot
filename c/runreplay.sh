#!/bin/bash

SNAPDIR=./pause

#callqaddr=`objdump --disassemble=main ex_tracee | egrep '<wonk>' | head -n 1 | cut -d ':' -f 1 | sed -e 's/ //g'`
#printf "callq wonk addr= 0x%s\n" ${callqaddr}


xoraddr=`objdump --disassemble=main ex_tracee | egrep 'xor    %r8d,%r8d' | cut -d ':' -f 1 | sed -e 's/ //g'`
if [ "${xoraddr}" == "" ]
then
	xoraddr=`objdump --disassemble=main ex_tracee | egrep 'xor    %edx,%edx' | cut -d ':' -f 1 | sed -e 's/ //g'`
fi
printf "xor addr=0x%s\n" ${xoraddr}

LD_LIBRARY_PATH=/home/arr/xed/kits/xed-install-base-2020-07-03-lin-x86-64/lib  \
  ./replay ${SNAPDIR} 0x${xoraddr} ./ex_tracee JABBERJAW
exit 0
