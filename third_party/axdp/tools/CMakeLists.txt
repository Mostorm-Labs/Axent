cmake_minimum_required(VERSION 3.15)

project(axdp_examples)
############################################################
# Create an executable
############################################################

# Add an executable with the above sources
add_executable(axdptool
        axdp_cli.h
        axdp_cli.cpp
        request.h
        request_handler.h
        request_handler.cpp
        dev_inst.h
        dev_inst.cpp
        request_result.h
        request_result.cpp
        ../third_party/easyloggingpp/src/easylogging++.cc
        )

if (WIN32)
        set_target_properties(axdptool PROPERTIES
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()

#link the new hello_library target with the hello_binary target
target_link_libraries(axdptool
        PRIVATE
        axdp
        )
