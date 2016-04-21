#!/bin/bash

if [ ! -d build ]; then
    mkdir build
fi
cd build
if [ ! -f Makefile ]; then
    cmake -DZ3_ROOT="${Z3_ROOT:?Must be set to the Z3 installation directory}" $@ ..
fi
make
