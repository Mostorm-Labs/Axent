if(NOT DEFINED AXENT_REPO_ROOT)
    message(FATAL_ERROR "AXENT_REPO_ROOT is required")
endif()

set(axent_deps_file "${AXENT_REPO_ROOT}/cmake/AxentDependencies.cmake")
set(axent_cmake_file "${AXENT_REPO_ROOT}/CMakeLists.txt")
set(axent_readme "${AXENT_REPO_ROOT}/README.md")
set(gitmodules_file "${AXENT_REPO_ROOT}/.gitmodules")
set(axent_roadmap_doc "${AXENT_REPO_ROOT}/docs/ROADMAP.md")
set(axent_architecture_doc "${AXENT_REPO_ROOT}/docs/architecture/AXENT_ARCHITECTURE.md")
set(axent_host_model_doc "${AXENT_REPO_ROOT}/docs/architecture/AXENT_HOST_MODEL.md")
set(axent_extension_model_doc "${AXENT_REPO_ROOT}/docs/architecture/AXENT_EXTENSION_MODEL.md")
set(axent_codex_guardrails_doc "${AXENT_REPO_ROOT}/docs/dev/CODEX_GUARDRAILS.md")

if(NOT EXISTS "${axent_deps_file}")
    message(FATAL_ERROR "Missing ${axent_deps_file}")
endif()
if(NOT EXISTS "${gitmodules_file}")
    message(FATAL_ERROR "Missing ${gitmodules_file}")
endif()

file(READ "${axent_deps_file}" axent_deps)
file(READ "${gitmodules_file}" axent_gitmodules)

function(strip_cmake_comments input output_variable)
    string(REPLACE "\r\n" "\n" normalized "${input}")
    string(REPLACE "\r" "\n" normalized "${normalized}")
    string(LENGTH "${normalized}" input_length)

    set(output "")
    set(index 0)
    while(index LESS input_length)
        string(SUBSTRING "${normalized}" "${index}" 1 current_char)

        set(next_index "${index}")
        math(EXPR next_index "${next_index} + 1")
        if(current_char STREQUAL "#" AND next_index LESS input_length)
            string(SUBSTRING "${normalized}" "${next_index}" 1 next_char)
            if(next_char STREQUAL "[")
                set(marker_index "${next_index}")
                math(EXPR marker_index "${marker_index} + 1")
                set(marker_equals "")
                while(marker_index LESS input_length)
                    string(SUBSTRING "${normalized}" "${marker_index}" 1 marker_char)
                    if(marker_char STREQUAL "=")
                        string(APPEND marker_equals "=")
                        math(EXPR marker_index "${marker_index} + 1")
                    else()
                        break()
                    endif()
                endwhile()

                if(marker_index LESS input_length)
                    string(SUBSTRING "${normalized}" "${marker_index}" 1 marker_char)
                    if(marker_char STREQUAL "[")
                        set(close_marker "]${marker_equals}]")
                        math(EXPR content_index "${marker_index} + 1")
                        string(SUBSTRING "${normalized}" "${content_index}" -1 remaining_text)
                        string(FIND "${remaining_text}" "${close_marker}" close_offset)
                        if(close_offset EQUAL -1)
                            string(APPEND output "\n")
                            break()
                        endif()

                        string(APPEND output "\n")
                        string(LENGTH "${close_marker}" close_marker_length)
                        math(EXPR close_index "${content_index} + ${close_offset}")
                        math(EXPR index "${close_index} + ${close_marker_length}")
                        continue()
                    endif()
                endif()
            endif()
        endif()

        string(APPEND output "${current_char}")
        math(EXPR index "${index} + 1")
    endwhile()

    string(REGEX REPLACE "#[^\n]*" "" output "${output}")
    set("${output_variable}" "${output}" PARENT_SCOPE)
endfunction()

strip_cmake_comments("${axent_deps}" axent_effective_deps)

function(assert_text_contains text pattern message)
    if(NOT "${text}" MATCHES "${pattern}")
        message(FATAL_ERROR "${message}")
    endif()
endfunction()

function(assert_text_does_not_contain text pattern message)
    if("${text}" MATCHES "${pattern}")
        message(FATAL_ERROR "${message}")
    endif()
endfunction()

function(assert_text_appears_before text before_text after_text message)
    string(FIND "${text}" "${before_text}" before_index)
    string(FIND "${text}" "${after_text}" after_index)

    if(before_index EQUAL -1)
        message(FATAL_ERROR "${message}")
    endif()
    if(after_index EQUAL -1)
        message(FATAL_ERROR "Axent must add axtp-cpp-runtime")
    endif()
    if(NOT before_index LESS after_index)
        message(FATAL_ERROR "${message}")
    endif()
endfunction()

function(assert_file_contains file_path pattern message)
    if(NOT EXISTS "${file_path}")
        message(FATAL_ERROR "Missing ${file_path}")
    endif()

    file(READ "${file_path}" file_contents)
    if(NOT "${file_contents}" MATCHES "${pattern}")
        message(FATAL_ERROR "${message}")
    endif()
endfunction()

function(assert_file_does_not_contain file_path pattern message)
    if(NOT EXISTS "${file_path}")
        message(FATAL_ERROR "Missing ${file_path}")
    endif()

    file(READ "${file_path}" file_contents)
    if("${file_contents}" MATCHES "${pattern}")
        message(FATAL_ERROR "${message}: ${file_path}")
    endif()
endfunction()

function(assert_files_do_not_contain files_variable pattern message)
    foreach(file_path IN LISTS ${files_variable})
        assert_file_does_not_contain("${file_path}" "${pattern}" "${message}")
    endforeach()
endfunction()

assert_file_contains(
    "${axent_roadmap_doc}"
    "Axent 演进 Roadmap"
    "Axent roadmap documentation must exist"
)

assert_file_contains(
    "${axent_roadmap_doc}"
    "Phase 0：架构冻结与文档约束"
    "Axent roadmap must start with architecture freeze and guardrails"
)

assert_file_contains(
    "${axent_roadmap_doc}"
    "Phase 7：axentd Device Host"
    "Axent roadmap must include axentd Device Host"
)

assert_file_contains(
    "${axent_roadmap_doc}"
    "Phase 8：Nearcast 接入 Axent"
    "Axent roadmap must include Nearcast integration through Axent"
)

assert_file_contains(
    "${axent_architecture_doc}"
    "Axent is a device-control runtime core with a stable extension model"
    "Axent architecture documentation must define the runtime core model"
)

assert_file_contains(
    "${axent_architecture_doc}"
    "Product differences go through extensions or hosts"
    "Axent architecture documentation must define the product boundary rule"
)

assert_file_contains(
    "${axent_architecture_doc}"
    "Axent owns the canonical `axtpctl` executable"
    "Axent architecture documentation must define axtpctl ownership"
)

assert_file_contains(
    "${axent_readme}"
    "`axtpctl` is the canonical AXTP control and diagnostic CLI maintained by Axent"
    "Axent README must make axtpctl the primary AXTP CLI"
)

assert_file_contains(
    "${axent_host_model_doc}"
    "axent daemon"
    "Axent host model must document the daemon host command"
)

assert_file_contains(
    "${axent_host_model_doc}"
    "axent run nearcast"
    "Axent host model must document product host launch semantics"
)

assert_file_contains(
    "${axent_host_model_doc}"
    "axent up --with daemon,nearcast"
    "Axent host model must document supervisor startup semantics"
)

assert_file_contains(
    "${axent_extension_model_doc}"
    "Device Adapter"
    "Axent extension model must define Device Adapters"
)

assert_file_contains(
    "${axent_extension_model_doc}"
    "Product Extension"
    "Axent extension model must define Product Extensions"
)

assert_file_contains(
    "${axent_extension_model_doc}"
    "Stream Provider"
    "Axent extension model must define Stream Providers"
)

assert_file_contains(
    "${axent_codex_guardrails_doc}"
    "Axent Core must not contain product-specific names"
    "Codex guardrails must prohibit product-specific core logic"
)

assert_file_contains(
    "${axent_codex_guardrails_doc}"
    "axent --daemon --nearcast"
    "Codex guardrails must reject ambiguous mixed host flags"
)

assert_file_contains(
    "${axent_codex_guardrails_doc}"
    "Axent owns the canonical `axtpctl`"
    "Codex guardrails must protect canonical axtpctl ownership"
)

file(GLOB_RECURSE axent_core_boundary_files LIST_DIRECTORIES false
    "${AXENT_REPO_ROOT}/include/axent/core/*.hpp"
    "${AXENT_REPO_ROOT}/src/core/*.cpp"
)

file(GLOB_RECURSE axent_code_boundary_files LIST_DIRECTORIES false
    "${AXENT_REPO_ROOT}/include/axent/*.hpp"
    "${AXENT_REPO_ROOT}/src/*.cpp"
    "${AXENT_REPO_ROOT}/tests/*.cpp"
    "${AXENT_REPO_ROOT}/tests/*.hpp"
)

file(GLOB_RECURSE axent_cli_boundary_files LIST_DIRECTORIES false
    "${AXENT_REPO_ROOT}/include/axent/cli/*.hpp"
    "${AXENT_REPO_ROOT}/include/axent/tooling/*.hpp"
    "${AXENT_REPO_ROOT}/src/cli/*.cpp"
    "${AXENT_REPO_ROOT}/src/tooling/*.cpp"
    "${AXENT_REPO_ROOT}/src/tooling/*.hpp"
)

set(axent_forbidden_product_terms "[Nn]ear[Cc]ast|Launcher|Signage|CastSession|Preview UI|PreviewUI|Renderer")
set(axent_forbidden_core_include_pattern "#[ \t]*include[ \t]*[<\"]axent/(adapters|cli|control|daemon|host|tooling)/")
set(axent_forbidden_mixed_host_flags "--daemon|--nearcast")

assert_files_do_not_contain(
    axent_core_boundary_files
    "${axent_forbidden_product_terms}"
    "Axent Core must not contain product-specific terms"
)

assert_files_do_not_contain(
    axent_core_boundary_files
    "${axent_forbidden_core_include_pattern}"
    "Axent Core must not include upper-layer module headers"
)

assert_files_do_not_contain(
    axent_code_boundary_files
    "axent[ \t]+--daemon[ \t]+--nearcast|axent[ \t]+--nearcast[ \t]+--daemon"
    "Axent code must not document or implement ambiguous mixed host commands"
)

assert_files_do_not_contain(
    axent_cli_boundary_files
    "${axent_forbidden_mixed_host_flags}"
    "Axent CLI must not expose ambiguous mixed host flags"
)

assert_file_contains(
    "${AXENT_REPO_ROOT}/cmake/AxentDependencies.cmake"
    "AXENT_USE_EXTERNAL_AXTP_RUNTIME"
    "Axent must expose a parent-provided AXTP runtime option"
)

assert_file_contains(
    "${AXENT_REPO_ROOT}/cmake/AxentDependencies.cmake"
    "axtp::runtime"
    "Axent external runtime mode must validate axtp::runtime"
)

assert_file_contains(
    "${axent_cmake_file}"
    "AXENT_BUILD_TESTING"
    "Axent must allow parent projects to disable Axent tests"
)

assert_file_contains(
    "${axent_cmake_file}"
    "add_library\\(axent_axtp_tooling STATIC"
    "Axent must provide a shared AXTP tooling target"
)

assert_file_contains(
    "${axent_cmake_file}"
    "add_library\\(axent::axtp_tooling ALIAS axent_axtp_tooling\\)"
    "Axent must expose the axent::axtp_tooling alias"
)

assert_file_contains(
    "${axent_cmake_file}"
    "add_executable\\(axtpctl src/cli/axtpctl_main.cpp\\)"
    "Axent must own the axtpctl executable target"
)

assert_file_does_not_contain(
    "${axent_cmake_file}"
    "axtp_toolkit"
    "Axent targets must not depend on the retired cpp-runtime toolkit"
)

if(EXISTS "${AXENT_REPO_ROOT}/src/cli/axtp_cli.cpp")
    message(FATAL_ERROR "AXTP runner must live under src/tooling, not src/cli")
endif()
foreach(required_tooling_file
        "include/axent/tooling/axtp_cli.hpp"
        "include/axent/cli/axtp_cli.hpp"
        "src/tooling/axtp_cli.cpp"
        "src/cli/axtpctl_main.cpp")
    if(NOT EXISTS "${AXENT_REPO_ROOT}/${required_tooling_file}")
        message(FATAL_ERROR "Required AXTP tooling file is missing: ${required_tooling_file}")
    endif()
endforeach()

assert_text_contains(
    "${axent_effective_deps}"
    "add_subdirectory\\(\"\\$\\{AXENT_THIRD_PARTY_DIR\\}/IXWebSocket\" \"\\$\\{CMAKE_CURRENT_BINARY_DIR\\}/third_party/IXWebSocket\" EXCLUDE_FROM_ALL\\)"
    "Axent must add IXWebSocket before axtp-cpp-runtime"
)
assert_text_contains(
    "${axent_effective_deps}"
    "add_subdirectory\\(\"\\$\\{AXENT_THIRD_PARTY_DIR\\}/hidapi\" \"\\$\\{CMAKE_CURRENT_BINARY_DIR\\}/third_party/hidapi\" EXCLUDE_FROM_ALL\\)"
    "Axent must add hidapi before axtp-cpp-runtime"
)
assert_text_contains(
    "${axent_effective_deps}"
    "add_subdirectory\\(\"\\$\\{AXENT_THIRD_PARTY_DIR\\}/axtp-cpp-runtime\" \"\\$\\{CMAKE_CURRENT_BINARY_DIR\\}/third_party/axtp-cpp-runtime\"\\)"
    "Axent must add axtp-cpp-runtime"
)
assert_text_appears_before(
    "${axent_effective_deps}"
    "add_subdirectory(\"\${AXENT_THIRD_PARTY_DIR}/IXWebSocket\" \"\${CMAKE_CURRENT_BINARY_DIR}/third_party/IXWebSocket\" EXCLUDE_FROM_ALL)"
    "add_subdirectory(\"\${AXENT_THIRD_PARTY_DIR}/axtp-cpp-runtime\" \"\${CMAKE_CURRENT_BINARY_DIR}/third_party/axtp-cpp-runtime\")"
    "Axent must add IXWebSocket before axtp-cpp-runtime"
)
assert_text_appears_before(
    "${axent_effective_deps}"
    "add_subdirectory(\"\${AXENT_THIRD_PARTY_DIR}/hidapi\" \"\${CMAKE_CURRENT_BINARY_DIR}/third_party/hidapi\" EXCLUDE_FROM_ALL)"
    "add_subdirectory(\"\${AXENT_THIRD_PARTY_DIR}/axtp-cpp-runtime\" \"\${CMAKE_CURRENT_BINARY_DIR}/third_party/axtp-cpp-runtime\")"
    "Axent must add hidapi before axtp-cpp-runtime"
)
assert_text_contains(
    "${axent_effective_deps}"
    "function\\(axent_select_hidapi_target output_variable\\)"
    "Axent must normalize hidapi target names for cross-platform builds"
)
assert_text_contains(
    "${axent_effective_deps}"
    "set\\(AXTP_CPP_RUNTIME_BUILD_TOOLS OFF CACHE BOOL \"\" FORCE\\)"
    "Axent product builds must not build cpp-runtime tools"
)
assert_text_contains(
    "${axent_effective_deps}"
    "set\\(AXTP_CPP_RUNTIME_BUILD_MEDIAHOST OFF CACHE BOOL \"\" FORCE\\)"
    "Axent product builds must not build the retired cpp-runtime mediahost demo"
)
assert_text_contains(
    "${axent_effective_deps}"
    "set\\(AXTP_CPP_RUNTIME_TOOLS_FETCH_DEPS OFF CACHE BOOL \"\" FORCE\\)"
    "Axent product builds must keep cpp-runtime tool dependency fetching off"
)
assert_text_does_not_contain(
    "${axent_effective_deps}"
    "set\\(AXTP_CPP_RUNTIME_TOOLS_FETCH_DEPS ON"
    "Axent must not enable cpp-runtime tool dependency fetching"
)
assert_text_contains(
    "${axent_gitmodules}"
    "path = third_party/hidapi"
    "Axent must track hidapi as a top-level submodule"
)
assert_text_contains(
    "${axent_gitmodules}"
    "path = third_party/IXWebSocket"
    "Axent must track IXWebSocket as a top-level submodule"
)

foreach(required_file
        "third_party/hidapi/CMakeLists.txt"
        "third_party/IXWebSocket/CMakeLists.txt"
        "third_party/axtp-cpp-runtime/CMakeLists.txt")
    if(NOT EXISTS "${AXENT_REPO_ROOT}/${required_file}")
        message(FATAL_ERROR "Required dependency path is missing: ${required_file}")
    endif()
endforeach()

set(parent_fixture_dir "${CMAKE_CURRENT_BINARY_DIR}/axent-parent-runtime-fixture")
file(REMOVE_RECURSE "${parent_fixture_dir}")
file(MAKE_DIRECTORY "${parent_fixture_dir}")
file(WRITE "${parent_fixture_dir}/CMakeLists.txt" [=[
cmake_minimum_required(VERSION 3.21)
project(axent_parent_runtime_fixture LANGUAGES CXX)

add_library(axtp_core INTERFACE)
add_library(axtp::core ALIAS axtp_core)
add_library(axtp_json_rpc INTERFACE)
add_library(axtp::json_rpc ALIAS axtp_json_rpc)
add_library(axtp_runtime INTERFACE)
add_library(axtp::runtime ALIAS axtp_runtime)
add_library(axtp_sdk INTERFACE)
add_library(axtp::sdk ALIAS axtp_sdk)
add_library(axtp_firmware_profile INTERFACE)
add_library(axtp::firmware_profile ALIAS axtp_firmware_profile)
add_library(axtp_transport_hidapi INTERFACE)
add_library(axtp::transport_hidapi ALIAS axtp_transport_hidapi)
add_library(axtp_transport_tcp_native INTERFACE)
add_library(axtp::transport_tcp_native ALIAS axtp_transport_tcp_native)
add_library(axtp_transport_websocket_ix INTERFACE)
add_library(axtp::transport_websocket_ix ALIAS axtp_transport_websocket_ix)

set(AXENT_USE_EXTERNAL_AXTP_RUNTIME ON CACHE BOOL "" FORCE)
set(AXENT_BUILD_CONCRETE_TRANSPORT_DEPS OFF CACHE BOOL "" FORCE)
set(AXENT_BUILD_TESTING OFF CACHE BOOL "" FORCE)
]=])
file(APPEND "${parent_fixture_dir}/CMakeLists.txt" "add_subdirectory(\"${AXENT_REPO_ROOT}\" axent)\n")
file(APPEND "${parent_fixture_dir}/CMakeLists.txt" [=[

if(NOT TARGET axent::libaxent)
    message(FATAL_ERROR "Expected axent::libaxent target")
endif()
if(NOT TARGET axent::axtp_tooling)
    message(FATAL_ERROR "Expected axent::axtp_tooling target")
endif()
if(NOT TARGET axtpctl)
    message(FATAL_ERROR "Expected Axent-owned axtpctl target")
endif()

get_target_property(libaxent_sources libaxent SOURCES)
if("${libaxent_sources}" MATCHES "axtp_cli\\.cpp")
    message(FATAL_ERROR "libaxent must not compile the AXTP tooling runner")
endif()

get_target_property(tooling_links axent_axtp_tooling LINK_LIBRARIES)
list(FIND tooling_links "libaxent" tooling_libaxent_index)
list(FIND tooling_links "axent::libaxent" tooling_alias_index)
if(NOT tooling_libaxent_index EQUAL -1 OR NOT tooling_alias_index EQUAL -1)
    message(FATAL_ERROR "axent_axtp_tooling must not link Axent Core")
endif()

get_target_property(axent_links axent LINK_LIBRARIES)
list(FIND axent_links "axent::libaxent" axent_core_index)
list(FIND axent_links "axent::axtp_tooling" axent_tooling_index)
if(axent_core_index EQUAL -1 OR axent_tooling_index EQUAL -1)
    message(FATAL_ERROR "axent must link both libaxent and shared AXTP tooling")
endif()

get_target_property(axtpctl_links axtpctl LINK_LIBRARIES)
list(FIND axtpctl_links "axent::axtp_tooling" axtpctl_tooling_index)
list(FIND axtpctl_links "axent::libaxent" axtpctl_core_index)
if(axtpctl_tooling_index EQUAL -1 OR NOT axtpctl_core_index EQUAL -1)
    message(FATAL_ERROR "axtpctl must link tooling without linking Axent Core")
endif()
]=])

execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${parent_fixture_dir}" -B "${parent_fixture_dir}/build"
    RESULT_VARIABLE parent_fixture_result
    OUTPUT_VARIABLE parent_fixture_output
    ERROR_VARIABLE parent_fixture_error
)
if(NOT parent_fixture_result EQUAL 0)
    message(FATAL_ERROR
        "Axent parent-provided runtime fixture failed.\n"
        "stdout:\n${parent_fixture_output}\n"
        "stderr:\n${parent_fixture_error}"
    )
endif()

execute_process(
    COMMAND "${CMAKE_CTEST_COMMAND}" --test-dir "${parent_fixture_dir}/build" -N
    RESULT_VARIABLE parent_fixture_ctest_result
    OUTPUT_VARIABLE parent_fixture_ctest_output
    ERROR_VARIABLE parent_fixture_ctest_error
)
if(NOT parent_fixture_ctest_result EQUAL 0)
    message(FATAL_ERROR
        "Axent parent-provided runtime fixture test listing failed.\n"
        "stdout:\n${parent_fixture_ctest_output}\n"
        "stderr:\n${parent_fixture_ctest_error}"
    )
endif()
if(NOT "${parent_fixture_ctest_output}" MATCHES "Total Tests: 0")
    message(FATAL_ERROR
        "AXENT_BUILD_TESTING=OFF must not import Axent tests.\n"
        "ctest -N output:\n${parent_fixture_ctest_output}"
    )
endif()

message(STATUS "Axent dependency boundary check passed")
