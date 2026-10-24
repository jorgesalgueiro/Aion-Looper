# Scoped to this plugin's VST3 wrapper. Keep the pinned JUCE checkout intact.
if(TARGET AionLooper_VST3)
    target_include_directories(AionLooper_VST3 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
    set(aion_vst3_upstream "${juce_SOURCE_DIR}/modules/juce_audio_plugin_client/juce_audio_plugin_client_VST3.cpp")
    set(aion_vst3_generated "${CMAKE_CURRENT_BINARY_DIR}/generated/aion_vst3_programs.cpp")
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/patch_vst3_programs.py"
        "${aion_vst3_upstream}" "${aion_vst3_generated}" COMMAND_ERROR_IS_FATAL ANY)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
        "${aion_vst3_upstream}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/patch_vst3_programs.py")
    get_target_property(aion_vst3_sources juce_audio_plugin_client_VST3 INTERFACE_SOURCES)
    list(FIND aion_vst3_sources "${aion_vst3_upstream}" aion_vst3_index)
    if(aion_vst3_index LESS 0)
        message(FATAL_ERROR "JUCE VST3 wrapper source layout changed")
    endif()
    list(REMOVE_ITEM aion_vst3_sources "${aion_vst3_upstream}")
    list(APPEND aion_vst3_sources "${aion_vst3_generated}")
    set_property(TARGET juce_audio_plugin_client_VST3 PROPERTY INTERFACE_SOURCES "${aion_vst3_sources}")
    if(BUILD_TESTING)
        add_test(NAME vst3_wrapper_generation COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/Vst3WrapperPatchTests.py"
            "${aion_vst3_upstream}" "${aion_vst3_generated}")
    endif()
endif()
