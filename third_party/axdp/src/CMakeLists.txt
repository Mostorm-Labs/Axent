include_directories(${PROJECT_SOURCE_DIR}/third_party/hidapi/hidapi)
include_directories(${PROJECT_SOURCE_DIR}/third_party/crc/include)
include_directories(${PROJECT_SOURCE_DIR}/third_party/json/)
include_directories(${PROJECT_SOURCE_DIR}/third_party/md5/include)

set(COMMON_SRCS ${PROJECT_SOURCE_DIR}/common/thread_pool.cpp)

set(AXDP_SRCS	${PROJECT_SOURCE_DIR}/src/axdp.cpp
				${PROJECT_SOURCE_DIR}/src/axdp_impl_alpha.cpp
				${PROJECT_SOURCE_DIR}/src/axdp_impl_beta.cpp
				${PROJECT_SOURCE_DIR}/src/axdp_bin_parser.cpp
				${PROJECT_SOURCE_DIR}/src/axdp_device_model.cpp
				${PROJECT_SOURCE_DIR}/src/axdp_impl.cpp
				${PROJECT_SOURCE_DIR}/src/axdp_io_hid_device.cpp
				${PROJECT_SOURCE_DIR}/src/axdp_message.cpp
				${PROJECT_SOURCE_DIR}/src/axdp_upgrade.cpp
				${PROJECT_SOURCE_DIR}/src/axdp_utils.cpp)

set(CRC_SRCS ${PROJECT_SOURCE_DIR}/third_party/crc/src/crc8.c
			 ${PROJECT_SOURCE_DIR}/third_party/crc/src/crc16.c
			 ${PROJECT_SOURCE_DIR}/third_party/crc/src/crcccitt.c
			 ${PROJECT_SOURCE_DIR}/third_party/crc/src/crcdnp.c
			 ${PROJECT_SOURCE_DIR}/third_party/crc/src/crckrmit.c
			 ${PROJECT_SOURCE_DIR}/third_party/crc/src/crcsick.c
			 ${PROJECT_SOURCE_DIR}/third_party/crc/src/nmea-chk.c)

set(MD5_SRCS ${PROJECT_SOURCE_DIR}/third_party/md5/src/md5.c
			 ${PROJECT_SOURCE_DIR}/third_party/md5/src/md5++.cpp)

if (WIN32)
	set(HIDAPI_SRCS	${PROJECT_SOURCE_DIR}/third_party/hidapi/windows/hid.c)
elseif(APPLE)
	macro(ADD_FRAMEWORK fwname appname)
		find_library(FRAMEWORK_${fwname}
				NAMES ${fwname}
				PATHS ${CMAKE_OSX_SYSROOT}/System/Library
				PATH_SUFFIXES Frameworks
				NO_DEFAULT_PATH)
		if( ${FRAMEWORK_${fwname}} STREQUAL FRAMEWORK_${fwname}-NOTFOUND)
			MESSAGE(ERROR ": Framework ${fwname} not found")
		else()
			TARGET_LINK_LIBRARIES(${appname} "${FRAMEWORK_${fwname}}/${fwname}")
			MESSAGE(STATUS "Framework ${fwname} found at ${FRAMEWORK_${fwname}}")
		endif()
	endmacro(ADD_FRAMEWORK)
	set(HIDAPI_SRCS	${PROJECT_SOURCE_DIR}/third_party/hidapi/mac/hid.c)
	add_library(axdp STATIC ${AXDP_SRCS} ${COMMON_SRCS} ${CRC_SRCS} ${MD5_SRCS} ${HIDAPI_SRCS})
	target_link_libraries(axdp
			"-framework AppKit"
			"-framework CoreFoundation"
			"-framework IOKit")
elseif(UNIX)
	set(HIDAPI_SRCS ${PROJECT_SOURCE_DIR}/third_party/hidapi/libusb/hid.c)
	set(LIBUSB_SRCS ${PROJECT_SOURCE_DIR}/third_party/libusb/core.c
			${PROJECT_SOURCE_DIR}/third_party/libusb/descriptor.c
			${PROJECT_SOURCE_DIR}/third_party/libusb/hotplug.c
			${PROJECT_SOURCE_DIR}/third_party/libusb/io.c
			${PROJECT_SOURCE_DIR}/third_party/libusb/sync.c
			${PROJECT_SOURCE_DIR}/third_party/libusb/strerror.c
			${PROJECT_SOURCE_DIR}/third_party/libusb/os/linux_usbfs.c
			${PROJECT_SOURCE_DIR}/third_party/libusb/os/events_posix.c
			${PROJECT_SOURCE_DIR}/third_party/libusb/os/threads_posix.c
			${PROJECT_SOURCE_DIR}/third_party/libusb/os/linux_netlink.c)
endif ()