#!/usr/bin/env bash

THIS_DIR=$(dirname ${0})

GENERATOR=${GENERATOR:-"Unix Makefiles"}
PROJECT_NAME=${1:-"make_all"}
BUILD_NAME=${BUILD_NAME:-"${PROJECT_NAME}-default-makefiles"}

echo "using GENERATOR=$GENERATOR"
echo "using BUILD_NAME=$BUILD_NAME"

mkdir -p ${THIS_DIR}/_build/

BASE_CALL="cmake -G \"${GENERATOR}\" -S \"${THIS_DIR}/${PROJECT_NAME}\""
echo "${BASE_CALL}"

eval ${BASE_CALL} -B ${THIS_DIR}/_build/${BUILD_NAME}-debug -DCMAKE_BUILD_TYPE=Debug
eval ${BASE_CALL} -B ${THIS_DIR}/_build/${BUILD_NAME}-release -DCMAKE_BUILD_TYPE=Release
