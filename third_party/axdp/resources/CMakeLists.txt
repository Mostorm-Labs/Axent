cmake_minimum_required(VERSION 3.15)

project(axdp_examples)
############################################################
# Create an executable
############################################################

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# Add an executable with the above sources
add_executable(axdp_sdk_example
        ${PROJECT_SOURCE_DIR}/demo/axdp_example_common.h
        ${PROJECT_SOURCE_DIR}/demo/axdp_example_common.cpp
        ${PROJECT_SOURCE_DIR}/demo/axdp_example.cpp
        )

if (WIN32)
        set_target_properties(axdp_sdk_example PROPERTIES
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()

target_include_directories(axdp_sdk_example 
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

if(WIN32)
        target_link_directories(axdp_sdk_example
                PRIVATE
                ${PROJECT_SOURCE_DIR}/lib/Windows/Debug/Win32
        )
        # Reference
        # https://cloud.tencent.com/developer/ask/sof/117375

        add_custom_command(
            TARGET axdp_sdk_example POST_BUILD        # Adds a post-build event to MyTest
            COMMAND ${CMAKE_COMMAND} -E copy_if_different   # which executes "cmake - E copy_if_different..."
            "${PROJECT_SOURCE_DIR}/lib/${CMAKE_SYSTEM_NAME}/$<CONFIGURATION>/${CMAKE_GENERATOR_PLATFORM}/axdp.dll"   # <--this is in-file
            $<TARGET_FILE_DIR:axdp_sdk_example>     # <--this is out-file path
        )
elseif(APPLE)
        target_link_directories(axdp_sdk_example
                PRIVATE
                ${PROJECT_SOURCE_DIR}/lib/Darwin/Debug
        )
else()
        message(STATUS "Error platform may not be supported")
endif(WIN32)

#link the new hello_library target with the hello_binary target
target_link_libraries(axdp_sdk_example
        PRIVATE
        axdp
        # ${PROJECT_SOURCE_DIR}/lib/Darwin/Debug/libaxdp.dylib
)

