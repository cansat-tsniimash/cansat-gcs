#!/usr/bin/bash

THIS_DIR=$(dirname ${0})

mkdir -p ${THIS_DIR}/ccsds-link-build/
cmake -G "Eclipse CDT4 - Ninja" -S ${THIS_DIR}/ccsds-link -B ${THIS_DIR}/ccsds-link-build/debug -DCMAKE_BUILD_TYPE=Debug
cmake -G "Eclipse CDT4 - Ninja" -S ${THIS_DIR}/ccsds-link -B ${THIS_DIR}/ccsds-link-build/release -DCMAKE_BUILD_TYPE=Release