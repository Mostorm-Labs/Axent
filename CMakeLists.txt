cmake_minimum_required(VERSION 3.21)
project(Axent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
include(cmake/CompilerWarnings.cmake)
include(cmake/AxentDependencies.cmake)

add_library(libaxent
    src/version.cpp
    src/control/protocol_codecs.cpp
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
    src/logging/logger.cpp
    src/adapters/mock_adapter.cpp
)
add_library(axent::libaxent ALIAS libaxent)
target_include_directories(libaxent PUBLIC include)
target_link_libraries(libaxent PUBLIC axtp::core axtp::json_rpc)
axent_enable_warnings(libaxent)

if(BUILD_TESTING)
    add_executable(version_smoke_test tests/version_smoke_test.cpp)
    target_link_libraries(version_smoke_test PRIVATE axent::libaxent)
    axent_enable_warnings(version_smoke_test)
    add_test(NAME version_smoke_test COMMAND version_smoke_test)

    add_executable(core_types_test tests/core_types_test.cpp)
    target_link_libraries(core_types_test PRIVATE axent::libaxent)
    axent_enable_warnings(core_types_test)
    add_test(NAME core_types_test COMMAND core_types_test)

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

    add_executable(broker_flow_test tests/broker_flow_test.cpp)
    target_link_libraries(broker_flow_test PRIVATE axent::libaxent)
    axent_enable_warnings(broker_flow_test)
    add_test(NAME broker_flow_test COMMAND broker_flow_test)

    add_executable(protocol_codecs_test tests/protocol_codecs_test.cpp)
    target_link_libraries(protocol_codecs_test PRIVATE axent::libaxent)
    axent_enable_warnings(protocol_codecs_test)
    add_test(NAME protocol_codecs_test COMMAND protocol_codecs_test)
endif()
