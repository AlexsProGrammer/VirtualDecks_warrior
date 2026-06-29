# src/CMakeLists.txt

# Attach sources to the top-level `juceDjApp` target if it exists, otherwise
# create a local `JUCELoopStation` JUCE app target and configure it.

# Add subdirectories for modular organization
add_subdirectory(core)
add_subdirectory(ui)
add_subdirectory(utilities)

# Collect all sources from subdomains
set(SOURCE_FILES
    Main.cpp
    MainComponent.cpp
    ${CORE_SOURCES}
    ${UI_SOURCES}
    ${UTILITIES_SOURCES}
)

if(TARGET juceDjApp)
    target_sources(juceDjApp PRIVATE ${SOURCE_FILES})
    target_include_directories(juceDjApp PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_SOURCE_DIR}/core/audio
        ${CMAKE_CURRENT_SOURCE_DIR}/core/data
        ${CMAKE_CURRENT_SOURCE_DIR}/core/analysis
        ${CMAKE_CURRENT_SOURCE_DIR}/core/effects
        ${CMAKE_CURRENT_SOURCE_DIR}/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/decks
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/waveforms
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/library
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/settings
        ${CMAKE_CURRENT_SOURCE_DIR}/utilities
    )
    # (JUCE header generation moved after binary-data setup below)
    # Ensure binary data used by the sources is bundled and linked into the top-level app
    set(LoopStationBinaryData_SOURCES)
    foreach(_asset IN ITEMS
        logo.png
        fonts/default_font.ttf
        effects.json
    )
        if(EXISTS "${CMAKE_SOURCE_DIR}/assets/${_asset}")
            list(APPEND LoopStationBinaryData_SOURCES "${CMAKE_SOURCE_DIR}/assets/${_asset}")
        endif()
    endforeach()
    foreach(_svg IN ITEMS
        horizontalKnob.svg
        verticalKnob.svg
        playButton.svg
        playButtonHover.svg
        pauseButton.svg
        pauseButtonHover.svg
        loadButton.svg
        loadButtonHover.svg
        iconSettings.svg
        iconSettingsHover.svg
        iconCues.svg
        iconGrid.svg
        iconJump.svg
        iconLoop.svg
        iconQuantize.svg
        iconSync.svg
        iconPadFx.svg
        iconBeatFx.svg
        iconReleaseFx.svg
        iconHeadphone.svg
        iconSyncBolt.svg
        iconBpmTap.svg
        iconNudgeLeft.svg
        iconNudgeRight.svg
        iconClose.svg
        iconChainOpen.svg
        iconChainClosed.svg
    )
        if(EXISTS "${CMAKE_SOURCE_DIR}/assets/${_svg}")
            list(APPEND LoopStationBinaryData_SOURCES "${CMAKE_SOURCE_DIR}/assets/${_svg}")
        endif()
    endforeach()

    message(STATUS "LoopStationBinaryData_SOURCES = ${LoopStationBinaryData_SOURCES}")
    if(LoopStationBinaryData_SOURCES)
        juce_add_binary_data(LoopStationBinaryData SOURCES ${LoopStationBinaryData_SOURCES})
        target_link_libraries(juceDjApp PRIVATE LoopStationBinaryData)
        # Ensure the generated BinaryData header is on the include path for the app
        target_include_directories(juceDjApp PRIVATE "${CMAKE_BINARY_DIR}/LoopStationBinaryData_artefacts/JuceLibraryCode")
    endif()

    # Now generate the JUCE aggregated header so it can include BinaryData if present
    juce_generate_juce_header(juceDjApp)

else()
    juce_add_gui_app(JUCELoopStation
        PRODUCT_NAME "VirtualLooper"
        COMPANY_NAME "Pro-Grammer"
        COMPANY_COPYRIGHT "Copyright (c) 2025 Pro-Grammer"
        COMPANY_WEBSITE "https://pro-grammer.de"
        COMPANY_EMAIL "info@pro-grammer.de"
        VERSION "0.9.5"
        ICON_BIG "${CMAKE_SOURCE_DIR}/assets/logo.png"
        MICROPHONE_PERMISSION_ENABLED TRUE
        MICROPHONE_PERMISSION_TEXT "VirtualLooper requires microphone access for audio recording."
    )

    target_compile_features(JUCELoopStation PUBLIC cxx_std_17)

    target_compile_definitions(JUCELoopStation
        PUBLIC
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_DISPLAY_SPLASH_SCREEN=0
            JUCE_REPORT_APP_USAGE=0
            JUCE_STRICT_REFCOUNTEDPOINTER=1
            JUCE_JACK=1
            JUCE_WASAPI=1
            JUCE_ASIO=1
            $<$<CONFIG:Release>:NDEBUG=1>
            $<$<CONFIG:Release>:JUCE_CHECK_MEMORY_LEAKS=0>
    )

    # Binary data
    set(LoopStationBinaryData_SOURCES)
    foreach(_asset IN ITEMS
        logo.png
        fonts/default_font.ttf
        effects.json
    )
        if(EXISTS "${CMAKE_SOURCE_DIR}/assets/${_asset}")
            list(APPEND LoopStationBinaryData_SOURCES "${CMAKE_SOURCE_DIR}/assets/${_asset}")
        endif()
    endforeach()
    foreach(_svg IN ITEMS
        horizontalKnob.svg
        verticalKnob.svg
        playButton.svg
        playButtonHover.svg
        pauseButton.svg
        pauseButtonHover.svg
        loadButton.svg
        loadButtonHover.svg
        iconSettings.svg
        iconSettingsHover.svg
        iconCues.svg
        iconGrid.svg
        iconJump.svg
        iconLoop.svg
        iconQuantize.svg
        iconSync.svg
        iconPadFx.svg
        iconBeatFx.svg
        iconReleaseFx.svg
        iconHeadphone.svg
        iconSyncBolt.svg
        iconBpmTap.svg
        iconNudgeLeft.svg
        iconNudgeRight.svg
        iconClose.svg
    )
        if(EXISTS "${CMAKE_SOURCE_DIR}/assets/${_svg}")
            list(APPEND LoopStationBinaryData_SOURCES "${CMAKE_SOURCE_DIR}/assets/${_svg}")
        endif()
    endforeach()

    message(STATUS "(fallback) LoopStationBinaryData_SOURCES = ${LoopStationBinaryData_SOURCES}")
    if(LoopStationBinaryData_SOURCES)
           juce_add_binary_data(LoopStationBinaryData SOURCES ${LoopStationBinaryData_SOURCES})
           target_link_libraries(JUCELoopStation PRIVATE LoopStationBinaryData)
           # Ensure the generated BinaryData header is on the include path for the fallback app
           target_include_directories(JUCELoopStation PRIVATE "${CMAKE_BINARY_DIR}/LoopStationBinaryData_artefacts/JuceLibraryCode")
    endif()

    juce_generate_juce_header(JUCELoopStation)

    target_sources(JUCELoopStation PRIVATE ${SOURCE_FILES})
    target_include_directories(JUCELoopStation PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_SOURCE_DIR}/core/audio
        ${CMAKE_CURRENT_SOURCE_DIR}/core/data
        ${CMAKE_CURRENT_SOURCE_DIR}/core/analysis
        ${CMAKE_CURRENT_SOURCE_DIR}/core/effects
        ${CMAKE_CURRENT_SOURCE_DIR}/ui
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/decks
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/waveforms
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/library
        ${CMAKE_CURRENT_SOURCE_DIR}/ui/settings
        ${CMAKE_CURRENT_SOURCE_DIR}/utilities
    )

    target_link_libraries(JUCELoopStation
        PRIVATE
            juce::juce_audio_basics
            juce::juce_audio_devices
            juce::juce_audio_formats
            juce::juce_audio_processors
            juce::juce_audio_utils
            juce::juce_core
            juce::juce_data_structures
            juce::juce_dsp
            juce::juce_events
            juce::juce_graphics
            juce::juce_gui_basics
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags
    )

    set_target_properties(JUCELoopStation PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        $<$<CONFIG:Release>:INTERPROCEDURAL_OPTIMIZATION ON>
        $<$<CONFIG:Release>:POSITION_INDEPENDENT_CODE ON>
    )

    if(MSVC)
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT JUCELoopStation)
    endif()
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
