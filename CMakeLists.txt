cmake_minimum_required(VERSION 3.22)

# ============================================================================
#  Anamorph — Stereo Tools Audio Plugin (VST3)
#
#  Headless, command-line-only build. CMake + JUCE only (no Projucer / IDE).
#  Pulls JUCE via FetchContent (pinned tag). VST3 SDK ships with JUCE.
#
#  Build:
#     cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#     cmake --build build --config Release
# ============================================================================

project(Anamorph VERSION 0.7.2 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ----------------------------------------------------------------------------
#  Options
# ----------------------------------------------------------------------------
option(ANAMORPH_BUILD_TESTS "Build the DSP self-test executable" ON)
option(ANAMORPH_BUILD_STANDALONE "Build the Standalone target for convenience" ON)

# Allow pointing at a local JUCE checkout to avoid network in restricted CI.
#   -DANAMORPH_JUCE_PATH=/path/to/JUCE
set(ANAMORPH_JUCE_PATH "" CACHE STRING "Path to a local JUCE checkout (optional)")
set(ANAMORPH_JUCE_TAG  "8.0.8" CACHE STRING "JUCE git tag to fetch when no local path is given")

# ----------------------------------------------------------------------------
#  Bring in JUCE
# ----------------------------------------------------------------------------
if(ANAMORPH_JUCE_PATH)
    message(STATUS "Anamorph: using local JUCE at ${ANAMORPH_JUCE_PATH}")
    add_subdirectory(${ANAMORPH_JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    message(STATUS "Anamorph: fetching JUCE ${ANAMORPH_JUCE_TAG} from github.com/juce-framework/JUCE")
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${ANAMORPH_JUCE_TAG}
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# ----------------------------------------------------------------------------
#  Format-agnostic DSP core  (decoupled from the plugin format wrapper)
#
#  Implemented as an INTERFACE library so the sources are compiled into each
#  final target (plugin / standalone / tests). This deliberately avoids
#  pre-compiling JUCE modules into a STATIC library, which would otherwise
#  duplicate JUCE module object code and break linking. The DSP core only
#  depends on juce_dsp / juce_audio_basics — never on the plugin wrapper.
# ----------------------------------------------------------------------------
add_library(AnamorphDSP INTERFACE)
target_include_directories(AnamorphDSP INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp)
target_sources(AnamorphDSP INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/AnamorphEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/HaasProcessor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/VelvetNoise.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/ChorusEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/MonoMaker.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/MultibandWidth.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/LoudnessMatch.cpp
)

# ----------------------------------------------------------------------------
#  The plugin
# ----------------------------------------------------------------------------
# VST3 everywhere; AU additionally on macOS (Logic/GarageBand only load AU).
# AU can only be built on a Mac, which is why it ships via the macOS CI job.
set(ANAMORPH_FORMATS VST3)
if(APPLE)
    list(APPEND ANAMORPH_FORMATS AU)
endif()
if(ANAMORPH_BUILD_STANDALONE)
    list(APPEND ANAMORPH_FORMATS Standalone)
endif()

juce_add_plugin(Anamorph
    COMPANY_NAME            "RollyTech"
    BUNDLE_ID               "com.rollytech.anamorph"
    PLUGIN_MANUFACTURER_CODE Anmf
    PLUGIN_CODE             Anmr
    FORMATS                 ${ANAMORPH_FORMATS}
    PRODUCT_NAME            "Anamorph"
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES         "Fx" "Spatial" "Stereo"
    DESCRIPTION             "Stereo Tools / Stereo image toolkit"
)

target_sources(Anamorph PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/PluginParameters.cpp
    src/PresetManager.cpp
    src/gui/LookAndFeel.cpp
    src/gui/Vectorscope.cpp
    src/gui/CorrelationMeter.cpp
    src/gui/LevelMeter.cpp
    src/gui/SpectrumImager.cpp
)

target_include_directories(Anamorph PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Versioning (spec feedback #32): a human main version (0.x.y, < 1.0.0 pre-release)
# plus a build/dev number synced with CI (pass -DANAMORPH_BUILD_NUMBER=${github.run_number}).
set(ANAMORPH_BUILD_NUMBER "0" CACHE STRING "CI build / dev number shown in the About box")

target_compile_definitions(Anamorph PUBLIC
    ANAMORPH_VERSION_STRING="${PROJECT_VERSION}"
    ANAMORPH_BUILD_NUMBER="${ANAMORPH_BUILD_NUMBER}"
    JUCE_WEB_BROWSER=0          # no webview needed -> no hard webkit dependency at runtime
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
)

target_link_libraries(Anamorph
    PRIVATE
        AnamorphDSP
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# ----------------------------------------------------------------------------
#  DSP self-tests (headless gate, see scripts/run-tests.sh)
# ----------------------------------------------------------------------------
if(ANAMORPH_BUILD_TESTS)
    juce_add_console_app(AnamorphTests PRODUCT_NAME "AnamorphTests")
    target_sources(AnamorphTests PRIVATE tests/dsp_tests.cpp)
    target_include_directories(AnamorphTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(AnamorphTests PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
    )
    target_link_libraries(AnamorphTests
        PRIVATE
            AnamorphDSP
            juce::juce_dsp
            juce::juce_audio_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
