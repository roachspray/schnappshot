#!/bin/bash

. ../bconf.sh
BUILDDIR=../${BUILDDIR}

HCC=clang++
HC=clang

XED_ROOT=/home/arr/xed/kits/xed-install-base-2020-07-03-lin-x86-64
XED_INCDIR=-I${XED_ROOT}/include
XED_LIBDIR=-L${XED_ROOT}/lib

HCPPFLAGS="-std=c++14 -m64 -g -no-pie -Werror-unused-variable"
HCPPFLAGS="${HCPPFLAGS} -D_SEE_CHILD_OUTPUT"

# toggle on off
XED="-D_WITH_XED -DXED_ENCODER ${XED_INCDIR} ${XED_LIBDIR} -lxed"
#XED="-I."

# flambe
${HCC} ${HCPPFLAGS} ${XED} -o flambe flambe.cc SnapStuff.cc SnapFork.cc  \
  SnapUtil.cc
${HCC} ${HCPPFLAGS} ${XED} -o test test.cc SnapStuff.cc SnapFork.cc  \
  SnapUtil.cc

mv flambe ${BUILDDIR}
cp runflambedumper.sh ${BUILDDIR}
cp runflambereplay.sh ${BUILDDIR}
