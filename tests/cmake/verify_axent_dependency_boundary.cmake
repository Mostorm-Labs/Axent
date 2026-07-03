if(NOT DEFINED AXENT_REPO_ROOT)
    message(FATAL_ERROR "AXENT_REPO_ROOT is required")
endif()

set(axent_deps_file "${AXENT_REPO_ROOT}/cmake/AxentDependencies.cmake")
set(gitmodules_file "${AXENT_REPO_ROOT}/.gitmodules")

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

assert_text_contains(
    "${axent_effective_deps}"
    "add_subdirectory\\(third_party/IXWebSocket EXCLUDE_FROM_ALL\\)"
    "Axent must add IXWebSocket before axtp-cpp-runtime"
)
assert_text_contains(
    "${axent_effective_deps}"
    "add_subdirectory\\(third_party/hidapi EXCLUDE_FROM_ALL\\)"
    "Axent must add hidapi before axtp-cpp-runtime"
)
assert_text_contains(
    "${axent_effective_deps}"
    "add_subdirectory\\(third_party/axtp-cpp-runtime\\)"
    "Axent must add axtp-cpp-runtime"
)
assert_text_appears_before(
    "${axent_effective_deps}"
    "add_subdirectory(third_party/IXWebSocket EXCLUDE_FROM_ALL)"
    "add_subdirectory(third_party/axtp-cpp-runtime)"
    "Axent must add IXWebSocket before axtp-cpp-runtime"
)
assert_text_appears_before(
    "${axent_effective_deps}"
    "add_subdirectory(third_party/hidapi EXCLUDE_FROM_ALL)"
    "add_subdirectory(third_party/axtp-cpp-runtime)"
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

message(STATUS "Axent dependency boundary check passed")
