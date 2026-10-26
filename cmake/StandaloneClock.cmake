# All TUs share the same generated holder definition. Never edit the JUCE checkout.
set(aion_standalone_module "${juce_SOURCE_DIR}/modules/juce_audio_plugin_client")
set(aion_standalone_generated "${CMAKE_CURRENT_BINARY_DIR}/generated/standalone-clock")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/patch_standalone_clock.py"
    "${aion_standalone_module}" "${aion_standalone_generated}" COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${aion_standalone_module}/Standalone/juce_StandaloneFilterWindow.h"
    "${aion_standalone_module}/juce_audio_plugin_client_Standalone.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/patch_standalone_clock.py")
target_include_directories(AionLooper PUBLIC "${aion_standalone_generated}")
if(TARGET AionLooper_Standalone)
    target_include_directories(AionLooper_Standalone PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
    set(aion_standalone_upstream "${aion_standalone_module}/juce_audio_plugin_client_Standalone.cpp")
    get_target_property(aion_standalone_sources juce_audio_plugin_client_Standalone INTERFACE_SOURCES)
    list(FIND aion_standalone_sources "${aion_standalone_upstream}" aion_standalone_index)
    if(aion_standalone_index LESS 0)
        message(FATAL_ERROR "JUCE Standalone wrapper source layout changed")
    endif()
    list(REMOVE_ITEM aion_standalone_sources "${aion_standalone_upstream}")
    list(APPEND aion_standalone_sources "${aion_standalone_generated}/aion_standalone_clock.cpp")
    set_property(TARGET juce_audio_plugin_client_Standalone PROPERTY INTERFACE_SOURCES "${aion_standalone_sources}")
endif()
if(BUILD_TESTING)
    add_test(NAME standalone_clock_sdk_generation COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/StandaloneClockPatchTests.py"
        "${aion_standalone_module}" "${aion_standalone_generated}")
endif()
