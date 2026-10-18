include(FetchContent)
# Pinned stable version + verified commit. Never follow a moving branch in a release.
set(AION_JUCE_TAG "9.0.4")
set(AION_JUCE_COMMIT "3fb5cbc0340758596dac39f7c8de47d562b4896e")
set(AION_CLAP_COMMIT "7adee3a1bd4684d4caa5601100e364abccff4b4d")
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG ${AION_JUCE_TAG} GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)
# Build linked dependency targets, but omit their SDK/install rules from our package.
set_property(DIRECTORY "${juce_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)
find_package(Git REQUIRED)
execute_process(COMMAND "${GIT_EXECUTABLE}" -c safe.directory=${juce_SOURCE_DIR}
    rev-parse HEAD WORKING_DIRECTORY "${juce_SOURCE_DIR}"
    OUTPUT_VARIABLE juce_revision OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE git_result)
if(NOT git_result EQUAL 0 OR NOT juce_revision STREQUAL AION_JUCE_COMMIT)
    message(FATAL_ERROR "JUCE source must be the pinned ${AION_JUCE_TAG} commit ${AION_JUCE_COMMIT}")
endif()
if(AION_BUILD_CLAP)
    set(CLAP_JUCE_EXTENSIONS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(clap_juce_extensions
        GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
        GIT_TAG ${AION_CLAP_COMMIT} GIT_SUBMODULES_RECURSE TRUE)
    FetchContent_MakeAvailable(clap_juce_extensions)
    set_property(DIRECTORY "${clap_juce_extensions_SOURCE_DIR}" PROPERTY EXCLUDE_FROM_ALL TRUE)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -c safe.directory=${clap_juce_extensions_SOURCE_DIR}
        rev-parse HEAD WORKING_DIRECTORY "${clap_juce_extensions_SOURCE_DIR}"
        OUTPUT_VARIABLE clap_revision OUTPUT_STRIP_TRAILING_WHITESPACE RESULT_VARIABLE clap_git_result)
    if(NOT clap_git_result EQUAL 0 OR NOT clap_revision STREQUAL AION_CLAP_COMMIT)
        message(FATAL_ERROR "CLAP extension source must be pinned commit ${AION_CLAP_COMMIT}")
    endif()
endif()
