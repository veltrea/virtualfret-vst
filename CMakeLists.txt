cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal binary")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
endif()

project(VirtualFret VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(external/JUCE EXCLUDE_FROM_ALL)

juce_add_binary_data(VirtualFretAssets
    HEADER_NAME "VirtualFretAssets.h"
    NAMESPACE VirtualFretAssets
    SOURCES
        assets/tunings.json
        assets/chord-shapes.json)

# Core logic (tuning model, chord shape resolution, strum scheduling,
# state serialization, i18n) depends only on juce_core +
# juce_data_structures so it can run inside the console test target
# without any GUI or plugin machinery.
set(VIRTUALFRET_CORE_SOURCES
    src/core/FretModel.h
    src/core/FretModel.cpp
    src/core/TuningLibrary.h
    src/core/TuningLibrary.cpp
    src/core/ChordShapeLibrary.h
    src/core/ChordShapeLibrary.cpp
    src/core/StrumScheduler.h
    src/core/Localization.h
    src/core/Localization.cpp)

juce_add_plugin(VirtualFret
    COMPANY_NAME "veltrea"
    BUNDLE_ID "com.veltrea.virtualfret"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT TRUE
    IS_MIDI_EFFECT TRUE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    # Never auto-install: a rebuild once swapped the .vst3 underneath a
    # running Ableton Live and new device instances lost their MIDI-output
    # tap in the routing choosers. Use scripts/install-plugins.sh, which
    # refuses to run while Live is open.
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Vltr
    PLUGIN_CODE Vfrt
    # AU is deliberately not built: Ableton Live cannot route MIDI out of
    # AU plugins, so an AU build only creates a misleading browser entry.
    FORMATS VST3 Standalone
    # Live treats "Fx" VST3s as audio effects and refuses to load one
    # without audio inputs; register as an instrument instead.
    VST3_CATEGORIES Instrument Tools
    PRODUCT_NAME "VirtualFret")

target_sources(VirtualFret PRIVATE
    ${VIRTUALFRET_CORE_SOURCES}
    src/PluginProcessor.h
    src/PluginProcessor.cpp
    src/PluginEditor.h
    src/PluginEditor.cpp
    src/ui/Theme.h
    src/ui/FretboardComponent.h
    src/ui/FretboardComponent.cpp
    src/ui/HeadstockComponent.h
    src/ui/HeadstockComponent.cpp
    src/ui/StrumZoneComponent.h
    src/ui/StrumZoneComponent.cpp
    src/ui/ToolbarComponent.h
    src/ui/ToolbarComponent.cpp)

target_compile_definitions(VirtualFret PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

# Keep debug info in release builds so crash reports from hosts (Live etc.)
# can be symbolicated against the dSYM that package-release.sh produces.
# LTO is deliberately not used: it breaks dSYM generation.
target_compile_options(VirtualFret PRIVATE $<$<CONFIG:Release>:-g>)

target_link_libraries(VirtualFret
    PRIVATE
        VirtualFretAssets
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# Console unit tests for the core logic (run via ctest).
enable_testing()

add_executable(CoreTests
    tests/CoreTests.cpp
    ${VIRTUALFRET_CORE_SOURCES})

target_include_directories(CoreTests PRIVATE src)

target_compile_definitions(CoreTests PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0)

target_link_libraries(CoreTests PRIVATE
    VirtualFretAssets
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events)

add_test(NAME CoreTests COMMAND CoreTests)
