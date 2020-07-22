#!/bin/bash

. ../../bconf.sh
BUILDDIR=../../${BUILDDIR}

HCC=clang++
HC=clang

# test input
${HC} -std=c11 -static -m64 -g -O0 -no-pie -o ex_tracee ex_tracee.c


mv ex_tracee ${BUILDDIR}
