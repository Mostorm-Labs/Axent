cmake_minimum_required(VERSION 3.21)
project(Axent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
option(AXENT_BUILD_TESTING "Build Axent test executables" ${BUILD_TESTING})
include(cmake/CompilerWarnings.cmake)
include(cmake/AxentDependencies.cmake)

add_library(libaxent
    src/version.cpp
    src/control/protocol_codecs.cpp
    src/control/control_plane.cpp
    src/core/json.cpp
    src/core/device_manager.cpp
    src/core/session_manager.cpp
    src/core/capability_registry.cpp
    src/core/adapter_registry.cpp
    src/core/route_manager.cpp
    src/core/middleware.cpp
    src/core/flow_control.cpp
    src/core/broker.cpp
    src/config/config.cpp
    src/diagnostics/diagnostics.cpp
    src/firmware/firmware_task.cpp
    src/cli/options.cpp
    src/logging/logger.cpp
    src/adapters/axdp_adapter.cpp
    src/adapters/axtp_adapter.cpp
    src/adapters/mock_adapter.cpp
    src/adapters/tea_adapter.cpp
    src/host/axent_host.cpp
    src/media/media_relay.cpp
    src/media/media_subscription.cpp
)
add_library(axent::libaxent ALIAS libaxent)
target_include_directories(libaxent PUBLIC include)
target_link_libraries(libaxent
    PUBLIC axtp::core axtp::json_rpc axtp::runtime axtp::sdk
)
if(TARGET axtp::transport_hidapi)
    target_link_libraries(libaxent PRIVATE axtp::transport_hidapi)
    target_compile_definitions(libaxent PRIVATE AXENT_HAS_AXTP_HID_TRANSPORT=1)
endif()
if(AXENT_HAS_AXDP_HEADERS)
    target_include_directories(libaxent PRIVATE third_party/axdp/include)
endif()

if(AXENT_HAS_TEA_MACOS_SDK)
    target_include_directories(libaxent PRIVATE third_party/tea/macos/TeaSdkMacOS_1.0.0.36/include)
endif()
axent_enable_warnings(libaxent)

set(AXENT_HAS_WEBSOCKET_TRANSPORT OFF)
if(TARGET axtp::transport_websocket_ix)
    set(AXENT_HAS_WEBSOCKET_TRANSPORT ON)
    add_library(axent_websocket_control
        src/control/websocket_server.cpp
    )
    target_include_directories(axent_websocket_control PUBLIC include)
    target_link_libraries(axent_websocket_control
        PUBLIC axent::libaxent
        PRIVATE axtp::transport_websocket_ix
    )
    axent_enable_warnings(axent_websocket_control)

    add_executable(axentd src/daemon/main.cpp)
    target_link_libraries(axentd PRIVATE axent_websocket_control)
    axent_enable_warnings(axentd)
endif()

add_executable(axent src/cli/main.cpp)
target_link_libraries(axent PRIVATE axent::libaxent)
axent_enable_warnings(axent)

if(AXENT_BUILD_TESTING)
    add_test(
        NAME axent_dependency_boundary
        COMMAND ${CMAKE_COMMAND}
            -DAXENT_REPO_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cmake/verify_axent_dependency_boundary.cmake
    )

    add_executable(version_smoke_test tests/version_smoke_test.cpp)
    target_link_libraries(version_smoke_test PRIVATE axent::libaxent)
    axent_enable_warnings(version_smoke_test)
    add_test(NAME version_smoke_test COMMAND version_smoke_test)

    add_executable(core_types_test tests/core_types_test.cpp)
    target_link_libraries(core_types_test PRIVATE axent::libaxent)
    axent_enable_warnings(core_types_test)
    add_test(NAME core_types_test COMMAND core_types_test)

    add_executable(transport_media_types_test tests/transport_media_types_test.cpp)
    target_link_libraries(transport_media_types_test PRIVATE axent::libaxent)
    axent_enable_warnings(transport_media_types_test)
    add_test(NAME transport_media_types_test COMMAND transport_media_types_test)

    add_executable(media_relay_test tests/media_relay_test.cpp)
    target_link_libraries(media_relay_test PRIVATE axent::libaxent)
    axent_enable_warnings(media_relay_test)
    add_test(NAME media_relay_test COMMAND media_relay_test)

    add_executable(axent_host_test tests/axent_host_test.cpp)
    target_link_libraries(axent_host_test PRIVATE axent::libaxent)
    axent_enable_warnings(axent_host_test)
    add_test(NAME axent_host_test COMMAND axent_host_test)

    add_executable(manager_registry_test tests/manager_registry_test.cpp)
    target_link_libraries(manager_registry_test PRIVATE axent::libaxent)
    axent_enable_warnings(manager_registry_test)
    add_test(NAME manager_registry_test COMMAND manager_registry_test)

    add_executable(mock_adapter_test tests/mock_adapter_test.cpp)
    target_link_libraries(mock_adapter_test PRIVATE axent::libaxent)
    axent_enable_warnings(mock_adapter_test)
    add_test(NAME mock_adapter_test COMMAND mock_adapter_test)

    add_executable(diagnostics_config_test tests/diagnostics_config_test.cpp)
    target_link_libraries(diagnostics_config_test PRIVATE axent::libaxent)
    axent_enable_warnings(diagnostics_config_test)
    add_test(NAME diagnostics_config_test COMMAND diagnostics_config_test)

    add_executable(logging_foundation_test tests/logging_foundation_test.cpp)
    target_link_libraries(logging_foundation_test PRIVATE axent::libaxent)
    axent_enable_warnings(logging_foundation_test)
    add_test(NAME logging_foundation_test COMMAND logging_foundation_test)

    add_executable(cli_options_test tests/cli_options_test.cpp)
    target_link_libraries(cli_options_test PRIVATE axent::libaxent)
    axent_enable_warnings(cli_options_test)
    add_test(NAME cli_options_test COMMAND cli_options_test)

    add_executable(broker_flow_test tests/broker_flow_test.cpp)
    target_link_libraries(broker_flow_test PRIVATE axent::libaxent)
    axent_enable_warnings(broker_flow_test)
    add_test(NAME broker_flow_test COMMAND broker_flow_test)

    add_executable(protocol_codecs_test tests/protocol_codecs_test.cpp)
    target_link_libraries(protocol_codecs_test PRIVATE axent::libaxent)
    axent_enable_warnings(protocol_codecs_test)
    add_test(NAME protocol_codecs_test COMMAND protocol_codecs_test)

    if(AXENT_HAS_WEBSOCKET_TRANSPORT)
        add_executable(websocket_server_test tests/websocket_server_test.cpp)
        target_link_libraries(websocket_server_test PRIVATE axent_websocket_control axtp::transport_websocket_ix)
        axent_enable_warnings(websocket_server_test)
        add_test(NAME websocket_server_test COMMAND websocket_server_test)
    endif()

    add_executable(cli_smoke_test tests/cli_smoke_test.cpp)
    axent_enable_warnings(cli_smoke_test)
    add_test(NAME cli_smoke_test COMMAND cli_smoke_test $<TARGET_FILE:axent>)

    add_executable(adapter_skeleton_test tests/adapter_skeleton_test.cpp)
    target_link_libraries(adapter_skeleton_test PRIVATE axent::libaxent)
    axent_enable_warnings(adapter_skeleton_test)
    add_test(NAME adapter_skeleton_test COMMAND adapter_skeleton_test)

    add_executable(axtp_real_adapter_test tests/axtp_real_adapter_test.cpp)
    target_link_libraries(axtp_real_adapter_test PRIVATE axent::libaxent)
    axent_enable_warnings(axtp_real_adapter_test)
    add_test(NAME axtp_real_adapter_test COMMAND axtp_real_adapter_test)
endif()
