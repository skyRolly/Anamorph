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

project(Anamorph VERSION 0.8.11 LANGUAGES C CXX)

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
set(ANAMORPH_JUCE_TAG  "8.0.14" CACHE STRING "JUCE git tag to fetch when no local path is given")

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
#  Build hardening (RH-PR-2, ADR-0021) — behaviour-neutral binary hygiene.
#
#  Carried by an INTERFACE target so the flags propagate as usage requirements:
#  linked PUBLIC into the shared-code target they reach every plugin format
#  target's compiles AND its final link; AnamorphTests links them too, so the
#  self-tests always validate the shipped flag configuration.
#
#  Deliberately absent (see ADR-0021):
#    * -O3 / LTO / -ffast-math — numerics-affecting flags are frozen
#      (DSP_POLICY; LTO already ships via juce::juce_recommended_lto_flags).
#    * Symbol visibility — JUCE's plugin helpers already set
#      CXX_VISIBILITY_PRESET hidden + VISIBILITY_INLINES_HIDDEN on the shared
#      code and every format target (JUCEUtils.cmake); restating it here would
#      only mask where the setting really comes from.
#    * strip — stripping happens in CI packaging AFTER the debug info is
#      captured (dSYM / PDB / .debug artifacts, see build.yml); local builds
#      stay fully debuggable.
# ----------------------------------------------------------------------------
add_library(AnamorphHardening INTERFACE)
if(MSVC)
    # /Zi (Release): generate debug info so the linker can emit a PDB — CMake's
    #   MSVC Release otherwise compiles without /Zi and links without /DEBUG,
    #   i.e. no PDB exists at all and field crashes are unsymbolizable.
    # /guard:cf: Control Flow Guard — the one hardening flag MSVC does not
    #   enable by default. /DYNAMICBASE /NXCOMPAT are defaults, pinned
    #   explicitly so a toolchain change cannot silently drop them.
    # /DEBUG alone would flip the linker to /OPT:NOREF,NOICF; /OPT:REF,ICF
    #   restores the exact non-/DEBUG Release link semantics, so the shipped
    #   image matches the previous one and only the PDB is new.
    target_compile_options(AnamorphHardening INTERFACE
        /guard:cf $<$<CONFIG:Release>:/Zi>)
    target_link_options(AnamorphHardening INTERFACE
        /guard:cf /DYNAMICBASE /NXCOMPAT
        $<$<CONFIG:Release>:/DEBUG /OPT:REF /OPT:ICF>)
else()
    # -g (Release): adds debug sections only — GCC/Clang codegen is unchanged;
    #   CI extracts them (objcopy --only-keep-debug / dsymutil) and strips the
    #   shipped binary. Behaviour-neutrality is proven by the twin engine dump
    #   (worklogs/release-hardening/RH_PR2_INVESTIGATION.md).
    # -ffunction-sections/-fdata-sections + --gc-sections / -dead_strip:
    #   per-symbol sections so the linker drops unreferenced code/data.
    # -fstack-protector-strong: made explicit (was distro-default-dependent).
    target_compile_options(AnamorphHardening INTERFACE
        -fstack-protector-strong
        -ffunction-sections -fdata-sections
        $<$<CONFIG:Release>:-g>)
    if(APPLE)
        target_link_options(AnamorphHardening INTERFACE
            -Wl,-dead_strip)
    else()
        target_link_options(AnamorphHardening INTERFACE
            -Wl,--gc-sections
            -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack)
    endif()
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
    ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp/SoloMonitor.cpp
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
        AnamorphHardening
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
            juce::juce_data_structures   # ValueTree for the state-restoration clamp test
        PUBLIC
            AnamorphHardening
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()

