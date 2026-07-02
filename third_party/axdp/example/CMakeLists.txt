cmake_minimum_required(VERSION 3.15)

project(axdp_examples)
############################################################
# Create an executable
############################################################

# Add an executable with the above sources
add_executable(axdp_sdk_example
        axdp_example_common.h
        axdp_example_common.cpp
        axdp_vm33_test.cpp
        axdp_example.cpp
        )

# Add multi device upgrade tool
add_executable(multi_device_upgrade
        axdp_example_common.h
        axdp_example_common.cpp
        multi_device_upgrade.cpp
        )

if (WIN32)
        set_target_properties(axdp_sdk_example PROPERTIES
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif ()

#link the new hello_library target with the hello_binary target
target_link_libraries(axdp_sdk_example
        PRIVATE
        axdp
        )

target_link_libraries(multi_device_upgrade
        PRIVATE
        axdp
        )

#add_executable(axdp_demo
#        axdp_demo.cpp
#)
#
#target_link_libraries(axdp_demo
#        PRIVATE
#        axdp
#        )
#
#add_executable(hamedal_ctrl
#        ../third_party/json/single_include/nlohmann/json.hpp
#        ./hamedal_ctrl.cpp
#        )
#target_link_libraries(hamedal_ctrl
#        PRIVATE
#        axdp)