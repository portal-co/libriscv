#!/usr/bin/env bash
set -e

mkdir -p build
pushd build
cmake .. -DCMAKE_BUILD_TYPE=MinSizeRel -DRISCV_EXT_C=OFF -DRISCV_MEMORY_TRAPS=OFF
make -j6
popd

VERBOSE=1 ./build/rvdoom
