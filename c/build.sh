#!/bin/bash

. ../bconf.sh
BUILDDIR=../${BUILDDIR}

HCC=clang

XED_ROOT=/home/arr/xed/kits/xed-install-base-2020-07-03-lin-x86-64
XED_INCDIR=-I${XED_ROOT}/include
XED_LIBDIR=-L${XED_ROOT}/lib

# toggle on off
#XED="-Werror-unused-variable -D_WITH_XED -DXED_ENCODER ${XED_INCDIR} ${XED_LIBDIR} -lxed"
XED=


# dumper
${HCC} -std=c11 -m64 -g -no-pie -D_SEE_CHILD_OUTPUT ${XED} -o dumper  \
  dumper.c helper.c

# replay 
${HCC} -std=c11 -m64 -g -no-pie -D_SEE_CHILD_OUTPUT ${XED} -o replay  \
  replay.c helper.c

# dumper/reloader :/
${HCC} -std=c11 -m64 -g -no-pie -D_SEE_CHILD_OUTPUT ${XED} -o dumload  \
  dumload.c helper.c

mv dumper replay dumload ${BUILDDIR}
cp rundumper.sh ${BUILDDIR}
cp runreplay.sh ${BUILDDIR} 
