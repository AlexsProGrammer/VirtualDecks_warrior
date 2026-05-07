# src/core/CMakeLists.txt

add_subdirectory(audio)
add_subdirectory(data)
add_subdirectory(analysis)
add_subdirectory(effects)

set(CORE_SOURCES "${AUDIO_SOURCES};${DATA_SOURCES};${ANALYSIS_SOURCES};${EFFECTS_SOURCES}" PARENT_SCOPE)
