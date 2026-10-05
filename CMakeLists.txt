cmake_minimum_required(VERSION 3.22)

# Universal by default for releases; pass -DCMAKE_OSX_ARCHITECTURES=arm64 for faster dev builds.
if(APPLE)
    if(NOT CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS build architectures" FORCE)
    endif()
    if(NOT CMAKE_OSX_DEPLOYMENT_TARGET)
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version" FORCE)
    endif()
endif()

project(Spenningur VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Single-config generators: an unoptimised plug-in is not a plug-in. Default to Release.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SPENNINGUR_BUILD_PLUGIN "Build the JUCE plug-in (needs JUCE)" ON)
option(SPENNINGUR_BUILD_TESTS  "Build the core unit tests" ON)
option(SPENNINGUR_DEV_TOOLS    "Build panel_shot and dsp_check (needs JUCE)" OFF)
option(SPENNINGUR_INSTALL_AFTER_BUILD "Copy the plug-in into ~/Library/Audio/Plug-Ins after building" OFF)

# ── Core: detector, harmony, resonator. No JUCE, no allocation on the audio path. ──
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS core/src/*.cpp)
add_library(spn_core STATIC ${CORE_SOURCES})
target_include_directories(spn_core PUBLIC core/include)
if(NOT MSVC)
    target_compile_options(spn_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

if(SPENNINGUR_BUILD_TESTS)
    enable_testing()
    file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
    add_executable(spn_tests ${TEST_SOURCES})
    target_link_libraries(spn_tests PRIVATE spn_core)
    if(NOT MSVC)
        target_compile_options(spn_tests PRIVATE -Wall -Wextra -Wno-unused-parameter)
    endif()
    add_test(NAME spn_tests COMMAND spn_tests)
endif()

# ── Plug-in ──
if(SPENNINGUR_BUILD_PLUGIN)
    set(JUCE_DIR "/Applications/JUCE" CACHE PATH "Path to a JUCE source checkout")
    if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "JUCE not found at ${JUCE_DIR} - pass -DJUCE_DIR=/path/to/JUCE")
    endif()
    add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)

    file(GLOB PLUGIN_SOURCES CONFIGURE_DEPENDS Source/*.cpp)

    juce_add_plugin(Spenningur
        COMPANY_NAME            "Sungam"
        BUNDLE_ID               "com.sungam.spenningur"
        PLUGIN_MANUFACTURER_CODE Sngm
        PLUGIN_CODE             Spnn
        FORMATS                 VST3 AU Standalone
        PRODUCT_NAME            "Spenningur"
        DESCRIPTION             "Resonating chord harmoniser"
        IS_SYNTH                FALSE
        NEEDS_MIDI_INPUT        FALSE
        NEEDS_MIDI_OUTPUT       FALSE
        IS_MIDI_EFFECT          FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD ${SPENNINGUR_INSTALL_AFTER_BUILD}
        VST3_CATEGORIES         Fx
        AU_MAIN_TYPE            kAudioUnitType_Effect)

    target_sources(Spenningur PRIVATE ${PLUGIN_SOURCES})
    target_compile_definitions(Spenningur PUBLIC
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0)
    target_include_directories(Spenningur PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
    target_link_libraries(Spenningur
        PRIVATE spn_core juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags
               juce::juce_recommended_warning_flags)

    if(SPENNINGUR_DEV_TOOLS)
        # Offscreen render of the real editor: panel.png, no host needed.
        juce_add_console_app(panel_shot PRODUCT_NAME "panel_shot" NEEDS_CURL FALSE)
        target_sources(panel_shot PRIVATE tools/panel_shot.cpp ${PLUGIN_SOURCES})
        target_compile_definitions(panel_shot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
            JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_include_directories(panel_shot PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
        target_link_libraries(panel_shot PRIVATE spn_core juce::juce_audio_utils juce::juce_dsp)

        # Runs the real processor over synthetic melodies and prints what comes out.
        juce_add_console_app(dsp_check PRODUCT_NAME "dsp_check" NEEDS_CURL FALSE)
        target_sources(dsp_check PRIVATE tools/dsp_check.cpp ${PLUGIN_SOURCES})
        target_compile_definitions(dsp_check PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
            JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
        target_include_directories(dsp_check PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
        target_link_libraries(dsp_check PRIVATE spn_core juce::juce_audio_utils juce::juce_dsp)
    endif()
endif()
