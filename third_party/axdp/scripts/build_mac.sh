#!/usr/bin/env zsh

BUILD_PATH=$1
TARGET=xcode
mkdir -p ${BUILD_PATH}/${TARGET}
cd ${BUILD_PATH}/${TARGET}
cmake ../.. -G "Xcode" -DCMAKE_OSX_ARCHITECTURES='$(ARCHS_STANDARD)'