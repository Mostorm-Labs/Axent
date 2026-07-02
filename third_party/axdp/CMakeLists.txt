if(POLICY CMP0091)
        cmake_policy(SET CMP0091 NEW)
endif(POLICY CMP0091)

#if (POLICY CMP0126)
#        cmake_policy(SET CMP0126 NEW)
#endif (POLICY CMP0126)

## Enable selection between [arm64] and [x86_64] [$(ARCHS_STANDARD)] targets
if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES
                # x86_64
                # arm64
                ${ARCHS_STANDARD}
                CACHE STRING "Build architectures for macOS" FORCE)
else ()
        message(STATUS "Current CMAKE_OSX_ARCHITECTURE string value is " ${CMAKE_OSX_ARCHITECTURES})
endif()
# set_property(CACHE CMAKE_OSX_ARCHITECTURES PROPERTY STRINGS arm64 x86_64)
set_property(CACHE CMAKE_OSX_ARCHITECTURES PROPERTY STRINGS ${ARCHS_STANDARD})

# Set default deployment target to 11.0 if not set and enable selection in GUI up to 13.0
if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET
                10.13
                CACHE STRING "Minimum macOS version to target for deployment (at runtime). Newer APIs will be weak-linked." FORCE)
endif()
set_property(CACHE CMAKE_OSX_DEPLOYMENT_TARGET PROPERTY STRINGS 13.0 12.0 11.0)


# # Allow selection of common build types via UI
# if(NOT CMAKE_BUILD_TYPE)
#         set(CMAKE_BUILD_TYPE
#                 "RelWithDebInfo"
#                 CACHE STRING "Build type [Release, RelWithDebInfo, Debug, MinSizeRel]" FORCE)
#         set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Release RelWithDebInfo Debug MinSizeRel)
# endif()

# Prohibit in-source builds
if("${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_SOURCE_DIR}")
        message(FATAL_ERROR "AXDP: You cannot build in a source directory (or any directory with CMakeLists.txt file)."
                "Please make a build subdirectory. Feel free to remove CMakeCache.txt and CMakeFiles.")
endif()

# Enable find_package targets to become globally available targets
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL TRUE)
# Enable RPATH support for generated binaries
set(CMAKE_MACOSX_RPATH TRUE)
# Use RPATHs from build tree _in_ the build tree
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
# Do not add default linker search paths to RPATH
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)
# Use common bundle-relative RPATH for installed targets
set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks")

cmake_minimum_required(VERSION 3.15)

project(axdp_library)

# Allow selection of common build types via UI
if(NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE
                "RelWithDebInfo"
                CACHE STRING "Build type [Release, RelWithDebInfo, Debug, MinSizeRel]" FORCE)
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Release RelWithDebInfo Debug MinSizeRel)
endif()

set(PROJECT_VERSION 1)
set(PROJECT_VERSION_MAJOR 4)
set(PROJECT_VERSION_MINOR 1)
set(PROJECT_VERSION_PATCH 1)
string(TIMESTAMP BUILD_DATE "%Y%m%d")
set(AXDP_LIB_PREFIX libaxdp)
set(AXDP_VERSION
        V${PROJECT_VERSION}.${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}.${BUILD_DATE}_${CMAKE_SYSTEM_NAME})
set(CMAKE_INSTALL_PREFIX ${PROJECT_SOURCE_DIR}/dist/${AXDP_LIB_PREFIX}_${AXDP_VERSION})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
# target_compile_options(axdp PUBLIC -Wall -Wno-c++11-extensions)

message(STATUS "[axdp]AXDP PROJECT_VERSION : " ${AXDP_VERSION})
message(STATUS "[axdp]CMake System: " ${CMAKE_SYSTEM})
message(STATUS "[axdp]CMake Build Type: " ${CMAKE_BUILD_TYPE})
message(STATUS "[axdp]CMake Project Name: " ${PROJECT_NAME})
message(STATUS "[axdp]CMake Project Source Dir: " ${PROJECT_SOURCE_DIR})
message(STATUS "[axdp]CMake Source Dir: " ${CMAKE_SOURCE_DIR})
message(STATUS "[axdp]CMake C Compiler: " ${CMAKE_C_COMPILER})
message(STATUS "[axdp]CMake C++ Compiler: " ${CMAKE_CXX_COMPILER})
message(STATUS "[axdp]CMake C Flags: " ${CMAKE_C_FLAGS})
message(STATUS "[axdp]CMake CXX Flags: " ${CMAKE_CXX_FLAGS})
message(STATUS "[axdp]CMake CXX Standard: " ${CMAKE_CXX_STANDARD})
message(STATUS "[axdp]CMake OSX target architecture: " ${CMAKE_OSX_ARCHITECTURES})
message(STATUS "[axdp]CMAKE OSX deployment target: " ${CMAKE_OSX_DEPLOYMENT_TARGET})
message(STATUS "[axdp]CMake install path prefix: " ${CMAKE_INSTALL_PREFIX})

############################################################
# Create a library
############################################################
#Generate the static library from the library sources
#add_library(axdp STATIC)
add_library(axdp SHARED)

target_sources(axdp
        PRIVATE
        ${PROJECT_SOURCE_DIR}/include/axdp_api.h
        ${PROJECT_SOURCE_DIR}/include/axdp_api_ex.h
        ${PROJECT_SOURCE_DIR}/include/axdp_defines.h
        ${PROJECT_SOURCE_DIR}/src/log_utils.h
        ${PROJECT_SOURCE_DIR}/src/axdp_api.cpp
        ${PROJECT_SOURCE_DIR}/src/axdp_utils.h
        ${PROJECT_SOURCE_DIR}/src/axdp_utils.cpp
        ${PROJECT_SOURCE_DIR}/src/bin_parser.h
        ${PROJECT_SOURCE_DIR}/src/bin_parser.cpp
        ${PROJECT_SOURCE_DIR}/src/device_accessor_impl.h
        ${PROJECT_SOURCE_DIR}/src/device_accessor_impl.cpp
        ${PROJECT_SOURCE_DIR}/src/dfu_helper.h
        ${PROJECT_SOURCE_DIR}/src/dfu_helper.cpp
        ${PROJECT_SOURCE_DIR}/src/fbf_helper.h
        ${PROJECT_SOURCE_DIR}/src/fbf_helper.cpp
        ${PROJECT_SOURCE_DIR}/src/io_device.h
        ${PROJECT_SOURCE_DIR}/src/hid_device.h
        ${PROJECT_SOURCE_DIR}/src/hid_device.cpp
        ${PROJECT_SOURCE_DIR}/src/message_engine.h
        ${PROJECT_SOURCE_DIR}/src/message_engine.cpp
        ${PROJECT_SOURCE_DIR}/src/message_receiver.h
        ${PROJECT_SOURCE_DIR}/src/message_receiver.cpp
        ${PROJECT_SOURCE_DIR}/src/message_sender.h
        ${PROJECT_SOURCE_DIR}/src/message_sender.cpp
        ${PROJECT_SOURCE_DIR}/src/message_recv_handler.cpp
        ${PROJECT_SOURCE_DIR}/src/message_send_handler.cpp
        ${PROJECT_SOURCE_DIR}/src/message_send_handler_ex.cpp
        ${PROJECT_SOURCE_DIR}/src/human_interface_handler.h
        ${PROJECT_SOURCE_DIR}/src/human_interface_handler.cpp
        ${PROJECT_SOURCE_DIR}/src/protocol_message.h
        ${PROJECT_SOURCE_DIR}/src/protocol_message.cpp
        ${PROJECT_SOURCE_DIR}/src/protocol_defines.h
        ${PROJECT_SOURCE_DIR}/third_party/crc/src/crc8.c
        ${PROJECT_SOURCE_DIR}/third_party/crc/src/crc16.c
        ${PROJECT_SOURCE_DIR}/third_party/crc/src/crcccitt.c
        ${PROJECT_SOURCE_DIR}/third_party/crc/src/crcdnp.c
        ${PROJECT_SOURCE_DIR}/third_party/crc/src/crckrmit.c
        ${PROJECT_SOURCE_DIR}/third_party/crc/src/crcsick.c
        ${PROJECT_SOURCE_DIR}/third_party/crc/src/nmea-chk.c
        ${PROJECT_SOURCE_DIR}/third_party/md5/src/md5.c
        ${PROJECT_SOURCE_DIR}/third_party/md5/src/md5++.cpp

        $<$<BOOL:${WIN32}>:
        # for WINDOWS
        ${PROJECT_SOURCE_DIR}/third_party/hidapi/hidapi/hidapi.h
        ${PROJECT_SOURCE_DIR}/third_party/hidapi/windows/hid.c
        ${PROJECT_SOURCE_DIR}/third_party/hidapi/windows/hidapi_cfgmgr32.h
        ${PROJECT_SOURCE_DIR}/third_party/hidapi/windows/hidapi_hidclass.h
        ${PROJECT_SOURCE_DIR}/third_party/hidapi/windows/hidapi_hidpi.h
        ${PROJECT_SOURCE_DIR}/third_party/hidapi/windows/hidapi_hidsdi.h
        ${PROJECT_SOURCE_DIR}/third_party/hidapi/windows/hidapi_winapi.h
        >

        $<$<BOOL:${APPLE}>:
        # for MAC OS
        ${PROJECT_SOURCE_DIR}/third_party/hidapi/hidapi/hidapi.h
        ${PROJECT_SOURCE_DIR}/third_party/hidapi/mac/hid.c
        ${PROJECT_SOURCE_DIR}/third_party/hidapi/mac/hidapi_darwin.h
        >

        $<$<BOOL:${ANDROID}>:
        # for ANDROID
        ${PROJECT_SOURCE_DIR}/third_party/hidapi/hidapi/hidapi.h
        ${PROJECT_SOURCE_DIR}/third_party/hidapi/libusb/hid.c
        ${PROJECT_SOURCE_DIR}/third_party/hidapi/libusb/hidapi_libusb.h
        ${PROJECT_SOURCE_DIR}/third_party/libusb/libusb/core.c
        ${PROJECT_SOURCE_DIR}/third_party/libusb/libusb/descriptor.c
        ${PROJECT_SOURCE_DIR}/third_party/libusb/libusb/hotplug.c
        ${PROJECT_SOURCE_DIR}/third_party/libusb/libusb/io.c
        ${PROJECT_SOURCE_DIR}/third_party/libusb/libusb/sync.c
        ${PROJECT_SOURCE_DIR}/third_party/libusb/libusb/strerror.c
        ${PROJECT_SOURCE_DIR}/third_party/libusb/libusb/os/linux_usbfs.c
        ${PROJECT_SOURCE_DIR}/third_party/libusb/libusb/os/events_posix.c
        ${PROJECT_SOURCE_DIR}/third_party/libusb/libusb/os/threads_posix.c
        ${PROJECT_SOURCE_DIR}/third_party/libusb/libusb/os/linux_netlink.c
        >
        )

target_include_directories(axdp
        PUBLIC
        ${PROJECT_SOURCE_DIR}
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/src
        ${PROJECT_SOURCE_DIR}/common
        ${PROJECT_SOURCE_DIR}/third_party
        ${PROJECT_SOURCE_DIR}/third_party/crc/include
        ${PROJECT_SOURCE_DIR}/third_party/md5/include
        ${PROJECT_SOURCE_DIR}/third_party/hidapi/hidapi
        ${PROJECT_SOURCE_DIR}/third_party/libusb/libusb
        )

if (APPLE)
        # set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15")
        # message(STATUS "[awx]CMake OSX Deployment Target: " ${CMAKE_OSX_DEPLOYMENT_TARGET})
        # Use script : cmake .. -G "Xcode" -DCMAKE_OSX_ARCHITECTURES='$(ARCHS_STANDARD)'
        # set(CMAKE_OSX_ARCHITECTURES ${ARCHS_STANDARD})
        # message(STATUS "cmake osx architectures = ${CMAKE_OSX_ARCHITECTURES}")
        find_package(Threads REQUIRED)
        target_link_libraries(axdp
                #PUBLIC hidapi_include
                PRIVATE Threads::Threads
                PRIVATE "-framework IOKit" "-framework CoreFoundation" "-framework AppKit"
                )
        # set_target_properties(axdp
        #         PROPERTIES
        #         EXPORT_NAME "darwin"
        #         OUTPUT_NAME "axdp"
        #         VERSION ${PROJECT_VERSION}
        #         SOVERSION ${PROJECT_VERSION_MAJOR}
        #         MACHO_COMPATIBILITY_VERSION ${PROJECT_VERSION_MAJOR}
        #         FRAMEWORK_VERSION ${PROJECT_VERSION_MAJOR}
        #         #PUBLIC_HEADER "${HIDAPI_PUBLIC_HEADERS}"
        #         )

        # compatibility with find_package()
        add_library(axdp::darwin ALIAS axdp)
        # compatibility with raw library link
        add_library(axdp_darwin ALIAS axdp)
elseif(WIN32)
        set_target_properties(axdp PROPERTIES
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
elseif(ANDROID)
        message(STATUS "[awx]Link log library for axdp when in android platform")
        target_link_libraries(axdp -llog)
endif ()

add_subdirectory(example)
add_subdirectory(tools)

############################################################
# Install
############################################################
# Library
install (TARGETS axdp axdp_sdk_example
    ARCHIVE DESTINATION lib/$<CONFIG>
    LIBRARY DESTINATION lib/$<CONFIG>
    RUNTIME DESTINATION bin/$<CONFIG>
)

# Header files
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/ DESTINATION include)

# Demo files
install(FILES 
        ${PROJECT_SOURCE_DIR}/example/axdp_example_common.h
        ${PROJECT_SOURCE_DIR}/example/axdp_example_common.cpp
        ${PROJECT_SOURCE_DIR}/example/axdp_example.cpp
        DESTINATION demo
)

# Config
# install (FILES cmake.conf DESTINATION etc)
