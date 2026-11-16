# Run only on demand: cmake -DAION_FAUST_CACHE=<absolute-cache> -P cmake/faust/Prepare.cmake
cmake_minimum_required(VERSION 3.22)
if(NOT IS_ABSOLUTE "${AION_FAUST_CACHE}")
    message(FATAL_ERROR "AION_FAUST_CACHE must be an absolute dependency-cache path")
endif()
file(MAKE_DIRECTORY "${AION_FAUST_CACHE}")
set(archive "${AION_FAUST_CACHE}/faust-2.88.0.tar.gz")
set(expected e4e175cf236924b5b7d4784cbb8c50cc01e211159e169655dd9e6d8f92b871d9)
if(NOT EXISTS "${archive}")
    file(DOWNLOAD "https://github.com/grame-cncm/faust/releases/download/2.88.0/faust-2.88.0.tar.gz"
        "${archive}" EXPECTED_HASH SHA256=${expected} TLS_VERIFY ON SHOW_PROGRESS)
endif()
file(SHA256 "${archive}" actual)
if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "Faust archive hash mismatch; refusing extraction")
endif()
set(root "${AION_FAUST_CACHE}/faust-2.88.0")
if(EXISTS "${root}")
    message(STATUS "Verified archive; preserving existing source directory ${root}")
else()
    # Skip unrelated examples/formalisation symlinks unsupported on Windows.
    file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "${AION_FAUST_CACHE}" PATTERNS
        "faust-2.88.0/compiler/*" "faust-2.88.0/architecture/*"
        "faust-2.88.0/build/*" "faust-2.88.0/libraries/*.lib"
        "faust-2.88.0/COPYING*" "faust-2.88.0/README*")
    if(NOT EXISTS "${root}/compiler/generator/interpreter/interpreter_dsp_aux.hh")
        message(FATAL_ERROR "Faust source extraction incomplete")
    endif()
endif()
message(STATUS "Faust 2.88.0 source: ${root}")
