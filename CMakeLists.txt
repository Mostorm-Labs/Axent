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
endif()
