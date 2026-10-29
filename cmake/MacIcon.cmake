# ADR-0062 macOS fallback: JUCE finishes configure-time icon/plist generation
# inside juce_add_plugin. Replace its sparse ICNS now, then give every bundle's
# resource copy a build dependency on the native generator. Windows keeps ICO.
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    get_target_property(ANAMORPH_MAC_ICON Anamorph JUCE_ICON_FILE)
    set(ANAMORPH_MAC_ICON_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenerateMacIcon.cmake")
    set(ANAMORPH_MAC_ICON_ARGS
        "-DBIG=${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/anamorph-1024.png"
        "-DSMALL=${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/anamorph-32.png"
        "-DOUTPUT=${ANAMORPH_MAC_ICON}"
        -P "${ANAMORPH_MAC_ICON_SCRIPT}")
    execute_process(COMMAND "${CMAKE_COMMAND}" ${ANAMORPH_MAC_ICON_ARGS}
        COMMAND_ERROR_IS_FATAL ANY)
    add_custom_command(OUTPUT "${ANAMORPH_MAC_ICON}"
        COMMAND "${CMAKE_COMMAND}" ${ANAMORPH_MAC_ICON_ARGS}
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/anamorph-1024.png"
                "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/anamorph-32.png"
                "${ANAMORPH_MAC_ICON_SCRIPT}"
        COMMENT "Compiling the approved macOS iconset with iconutil"
        VERBATIM)
    add_custom_target(AnamorphMacIcon DEPENDS "${ANAMORPH_MAC_ICON}")
    foreach(ANAMORPH_MAC_FORMAT IN LISTS ANAMORPH_FORMATS)
        add_dependencies(Anamorph_${ANAMORPH_MAC_FORMAT} AnamorphMacIcon)
    endforeach()
endif()
