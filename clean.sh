#!/bin/bash
. bconf.sh

rm -rf ${BUILDDIR}
pushd c
./clean.sh
popd
pushd cpp
./clean.sh
popd

