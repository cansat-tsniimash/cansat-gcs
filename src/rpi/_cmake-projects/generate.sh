#!/usr/bin/env bash

THIS_DIR=$(dirname ${0})

GENERATOR=${GENERATOR:-"Unix Makefiles" }
BUILD_NAME=${BUILD_NAME:-"default-makefiles" }

echo "using GENERATOR=$GENERATOR"
echo "using BUILD_NAME=$BUILD_NAME"

mkdir -p ${THIS_DIR}/_build/

BASE_CALL="cmake -G \"${GENERATOR}\" -S \"${THIS_DIR}/make_all\""
echo "${BASE_CALL}"

eval ${BASE_CALL} -B ${THIS_DIR}/_build/${BUILD_NAME}-debug -DCMAKE_BUILD_TYPE=Debug
eval ${BASE_CALL} -B ${THIS_DIR}/_build/${BUILD_NAME}-release -DCMAKE_BUILD_TYPE=Release
