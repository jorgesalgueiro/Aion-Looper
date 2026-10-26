# Scoped additions to a build-local copy of the pinned JUCE module only.
set(aion_devices_module "${juce_SOURCE_DIR}/modules/juce_audio_devices")
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/patch_device_delivery.py"
    "${aion_devices_module}" "${aion_standalone_generated}" COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${aion_devices_module}/audio_io/juce_AudioDeviceManager.cpp"
    "${aion_devices_module}/juce_audio_devices.cpp" "${aion_devices_module}/juce_audio_devices.mm"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/patch_device_delivery.py"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/extract_device_splitter.py")
get_target_property(aion_devices_sources juce_audio_devices INTERFACE_SOURCES)
set(aion_devices_replaced 0)
foreach(extension cpp mm)
    set(aion_devices_original "${aion_devices_module}/juce_audio_devices.${extension}")
    list(FIND aion_devices_sources "${aion_devices_original}" aion_devices_index)
    if(NOT aion_devices_index LESS 0)
        list(REMOVE_ITEM aion_devices_sources "${aion_devices_original}")
        list(APPEND aion_devices_sources "${aion_standalone_generated}/aion_audio_devices.${extension}")
        math(EXPR aion_devices_replaced "${aion_devices_replaced}+1")
    endif()
    # Source-local paths keep SDK-relative includes working without changing
    # flags for every JUCE translation unit or introducing another holder type.
    set_source_files_properties("${aion_standalone_generated}/aion_audio_devices.${extension}" PROPERTIES
        INCLUDE_DIRECTORIES "${aion_devices_module};${CMAKE_CURRENT_SOURCE_DIR}/source")
endforeach()
if(NOT aion_devices_replaced EQUAL 1)
    message(FATAL_ERROR "JUCE audio_devices module source layout changed")
endif()
set_property(TARGET juce_audio_devices PROPERTY INTERFACE_SOURCES "${aion_devices_sources}")
