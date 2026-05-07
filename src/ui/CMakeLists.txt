# src/ui/CMakeLists.txt

add_subdirectory(decks)
add_subdirectory(waveforms)
add_subdirectory(library)
add_subdirectory(settings)

set(UI_SOURCES "${DECKS_SOURCES};${WAVEFORMS_SOURCES};${LIBRARY_SOURCES};${SETTINGS_SOURCES}" PARENT_SCOPE)
