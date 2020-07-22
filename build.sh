#!/bin/bash

. bconf.sh

# yes, this is kinda ridiculous

mkdir ${BUILDDIR} || (printf "Can't make ${BUILDDIR}" && exit 1)
pushd c
./build.sh || (printf "C failed to build\n" && exit 1)
popd
pushd cpp
./build.sh || (printf "CPP failed to build\n" && exit 1)
popd
pushd common 
./build.sh || (printf "Common failed to build\n" && exit 1)
popd
