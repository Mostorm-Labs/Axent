#!/usr/bin/env zsh

BUILD_PATH=$1

#build for android multi architechture
TARGETS="armeabi-v7a armeabi x86 mips arm64-v8a mips64"

for TARGET in ${TARGETS}
do
    # create one build dir per target architecture
    mkdir -p ${BUILD_PATH}/${TARGET}

    cd ${BUILD_PATH}/${TARGET}

    cmake -DANDROID_NATIVE_API_LEVEL=${ANDROID_NATIVE_API_LEVEL} -DCMAKE_TOOLCHAIN_FILE=${ANDROID_TOOLCHAIN} -DANDROID_NDK=${ANDROID_NDK} -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE} -DANDROID_ABI=${TARGET} -DPROJ_HOME=${PROJ_HOME} ../../src

    make -j32

    cd -
done