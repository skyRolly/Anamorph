cmake_minimum_required(VERSION 3.22)

# ============================================================================
#  Anamorph — Stereo Tools Audio Plugin (VST3)
#
#  Headless, command-line-only build. CMake + JUCE only (no Projucer / IDE).
#  Pulls JUCE via FetchContent (pinned immutable commit). VST3 SDK ships with JUCE.
#
#  Build:
#     cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
#     cmake --build build --config Release
# ============================================================================

project(Anamorph VERSION 0.9.6 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ----------------------------------------------------------------------------
#  Options
# ----------------------------------------------------------------------------
option(ANAMORPH_BUILD_TESTS "Build the self-test executables (DSP + state compatibility)" ON)
option(ANAMORPH_BUILD_STANDALONE "Build the Standalone target for convenience" ON)
# OFF by default, deliberately. A benchmark that always builds gets run casually
# and its numbers quoted without the machine they came from, which is exactly the
# C2 failure `PERFORMANCE_BUDGET.md` is written against. It is also the only
# target here whose *purpose* is timing, so it must never be an incidental part
# of a correctness build.
option(ANAMORPH_BUILD_BENCH "Build AnamorphBench, the PERFORMANCE_BUDGET.md measurement harness (OFF by default)" OFF)
# OFF by default and Clang-only in practice: libFuzzer ships with Clang, so a
# GCC or MSVC configure with this ON would fail at link with a message about
# `-fsanitize=fuzzer` rather than about this option. The `fuzz` CI job is the
# supported way to build it; locally it needs a Clang toolchain and the caller
# supplies `-fsanitize=fuzzer` through the usual flag variables.
option(ANAMORPH_BUILD_FUZZ "Build AnamorphFuzzState, the setStateInformation fuzz target (OFF by default; needs Clang/libFuzzer)" OFF)
# The x86-64 ISA baseline (ADR-0031 for GCC/Clang, ADR-0032 for MSVC), ON by
# default: the shipped configuration on every x86-64 toolchain.
#
# IT EXISTS TO BE TURNED OFF FOR A COMPARISON, not as a supported way to ship.
# Both ADRs are Class A, and a Class-A claim is only worth what re-verifying it
# costs -- the twin dump has to be built BOTH ways, and the flags live on a
# target, so a `-march` in CMAKE_CXX_FLAGS is placed BEFORE them on the command
# line and loses. Without this switch, reproducing the frozen baseline means
# editing this file, which is exactly how a verification stops being run.
# The DEPENDENCY_POLICY rule-2 workflow needs it at every JUCE bump, and the
# `windows-avx2-ab` BLOCKING gate builds its baseline side with it on every
# push -- OFF is no longer inert on MSVC (it was, before ADR-0032).
#
# A build with this OFF runs on pre-Haswell x86-64 and is measurably slower
# (~17 % engine Ir on GCC/Clang; mechanism-shared on MSVC, unmeasured); it is
# NOT the shipped configuration and nothing in CI turns it off outside that
# gate's baseline half.
option(ANAMORPH_X86_ISA_BASELINE "x86-64: compile -march=haswell -ffp-contract=off (GCC/Clang, ADR-0031) / /arch:AVX2 (MSVC, ADR-0032); ON = the shipped configuration" ON)
# The DEPENDENCY_POLICY rule-2 instrument: build it against two JUCE checkouts
# and diff the two outputs. OFF by default -- it is used at a dependency bump,
# not on every build.
option(ANAMORPH_BUILD_DSPDUMP "Build AnamorphDspDump, the DSP bit-identity harness for dependency bumps (OFF by default)" OFF)

# Allow pointing at a local JUCE checkout to avoid network in restricted CI.
#   -DANAMORPH_JUCE_PATH=/path/to/JUCE
set(ANAMORPH_JUCE_PATH "" CACHE STRING "Path to a local JUCE checkout (optional)")
# JUCE 9.0.1 — pinned by the tag's IMMUTABLE commit SHA, not the (mutable) tag
# name, so the dependency cannot silently change under a re-pointed tag
# (dependency hardening; the human-readable version lives in ANAMORPH_JUCE_VERSION).
set(ANAMORPH_JUCE_VERSION "9.0.1" CACHE STRING "Human-readable JUCE version of the pinned commit")
set(ANAMORPH_JUCE_TAG  "e18f7f506c0b96f2c738a0bcd7fe6467a5005ad8" CACHE STRING
    "JUCE git rev to fetch when no local path is given (commit SHA of tag ${ANAMORPH_JUCE_VERSION})")

# ----------------------------------------------------------------------------
#  Bring in JUCE
# ----------------------------------------------------------------------------
if(ANAMORPH_JUCE_PATH)
    message(STATUS "Anamorph: using local JUCE at ${ANAMORPH_JUCE_PATH}")
    add_subdirectory(${ANAMORPH_JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    message(STATUS "Anamorph: fetching JUCE ${ANAMORPH_JUCE_VERSION} (${ANAMORPH_JUCE_TAG}) from github.com/juce-framework/JUCE")
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
#      AMENDED BY ADR-0031, narrowly: the GCC/Clang x86-64 builds now carry
#      -march=haswell -ffp-contract=off (the block below). -ffast-math and the
#      optimization level are NOT amended and stay frozen.
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

    # ------------------------------------------------------------------------
    #  x86-64 ISA baseline, the Windows half (ADR-0032). /arch:AVX2 at the
    #  DEFAULT /fp:precise -- deliberately NO /fp flag: VS2022+ (toolset 14.30+)
    #  documents /fp:precise as non-contracting by default, which is what makes
    #  this Class A. Measured on toolset 14.51.36231, two runs: 0/32 twin-dump
    #  scenarios moved by this flag alone; 32/32 moved by /fp:contract, which
    #  is why /fp:contract is NOT enabled and enabling it is a new ADR.
    #
    #  A default is not a pin, so the claim is ASSERTED rather than trusted:
    #  the `windows` job fails below toolset 14.30 (and on an unreadable
    #  version), and the `windows-avx2-ab` gate blocks on baseline == this
    #  configuration, every push. The same ISA floor as the GCC/Clang side
    #  (Haswell/Excavator; STATUS_ILLEGAL_INSTRUCTION inside the host below
    #  it) -- COMPATIBILITY_POLICY "Runtime compatibility" is the authority.
    #
    #  clang-cl would inherit this flag through if(MSVC) without any twin dump
    #  ever having run on it -- one more reason it stays outside the validated
    #  set (COMPATIBILITY_MATRIX, toolchain section).
    # ------------------------------------------------------------------------
    #  ARCHITECTURE GUARD. /arch:AVX2 is an x86 flag; ADR-0032 scopes it to
    #  Windows x86-64 and nothing else. CMAKE_CXX_COMPILER_ARCHITECTURE_ID is
    #  the right question to ask because it is the TARGET's identity, taken
    #  from the compiler's own predefines (_M_X64/_M_AMD64 -> "x64",
    #  _M_ARM64 -> "ARM64", _M_ARM64EC -> "ARM64EC", _M_IX86 -> "X86" --
    #  CMakePlatformId.h.in), so it is right for BOTH generators: an ARM64
    #  cross-build under the VS generator (-A ARM64) and an arm64 vcvarsall
    #  shell under Ninja both report ARM64, where CMAKE_SYSTEM_PROCESSOR
    #  reports the HOST and would hand an ARM64 target an x86 flag.
    #
    #  A POSITIVE match on x64, deliberately, rather than "not ARM64": an
    #  architecture nobody has thought about yet gets NO flag, which is the
    #  safe direction. ARM64EC and 32-bit X86 are excluded by the same
    #  property -- ADR-0032 measured x86-64 and says nothing about either, and
    #  the plug-in is 64-bit only regardless.
    #
    #  MISDETECTION FAILS CLOSED AND LOUD: if this ever stopped matching a
    #  real x64 build, the flag would silently vanish -- so `windows-avx2-ab`
    #  asserts /arch:AVX2 IS present in its shipped-configuration build files
    #  before comparing, and fails INFRASTRUCTURE if it is not. The gate
    #  cannot pass vacuously on a guard that stopped firing.
    if(ANAMORPH_X86_ISA_BASELINE AND CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
        target_compile_options(AnamorphHardening INTERFACE /arch:AVX2)
        message(STATUS "Anamorph: x86-64 ISA baseline (ADR-0032) -- /arch:AVX2 (MSVC target ${CMAKE_CXX_COMPILER_ARCHITECTURE_ID})")
    elseif(NOT ANAMORPH_X86_ISA_BASELINE AND CMAKE_CXX_COMPILER_ARCHITECTURE_ID STREQUAL "x64")
        message(WARNING
            "Anamorph: ANAMORPH_X86_ISA_BASELINE=OFF -- building WITHOUT the ADR-0032 /arch:AVX2 "
            "baseline. Comparison builds only; NOT the shipped configuration.")
    else()
        # Not an x86-64 MSVC target: ADR-0032 does not apply and no ISA flag is
        # added. Windows ARM64 is Not Supported (COMPATIBILITY_MATRIX); this
        # branch exists so that if such a target is ever introduced it starts
        # from base ISA rather than silently inheriting an x86 baseline.
        message(STATUS "Anamorph: no x86-64 ISA baseline -- MSVC target is "
                       "'${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}', outside ADR-0032's scope")
    endif()
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

    # ------------------------------------------------------------------------
    #  x86-64 ISA baseline (ADR-0031). GCC/Clang x86-64 ONLY.
    #
    #  THE TWO FLAGS ARE ONE DECISION AND MUST NEVER BE SEPARATED.
    #  `-march=haswell` is what makes the win: AVX2 gives the per-sample loops
    #  256-bit registers instead of 128. It ALSO introduces an FMA instruction
    #  that the frozen SysV baseline did not have at all -- which is why the
    #  permissive `-ffp-contract=fast` default has been inert on this target for
    #  the project's whole life. `-ffp-contract=off` keeps it inert. Drop it and
    #  the change stops being Class A: measured, 88.9 % of samples move (max
    #  delta 2.384e-07) and GCC and Clang stop agreeing bit-for-bit with each
    #  other, which costs the two-toolchain cross-check the `linux` (Clang) and
    #  `linux-lto-tests` (GCC) jobs currently provide for free.
    #
    #  Measured Class A as adopted: 0 of 32 twin-dump scenarios differ from the
    #  baseline, 0 mismatches across 180 configurations, 0 FMA instructions
    #  emitted (707 with contraction left at its default), -17.2 % engine-wide.
    #
    #  THE COST IS AN ISA FLOOR: Haswell (2013) / Excavator (2015), with SIGILL
    #  inside the host below it. That is a user-visible compatibility contract,
    #  so it lives in COMPATIBILITY_POLICY.md ("Runtime compatibility: the
    #  x86-64 ISA floor") -- read it before touching the `-march` value.
    #
    #  SCOPE, and why each exclusion is deliberate:
    #    * arm64 -- NOTHING is added. AArch64 has FMLA in its base ISA and the
    #      shipped arm64 slice contracts today; adding -ffp-contract=off there
    #      would be a Class-B change to shipped arm64 numerics for no
    #      user-visible gain (it would close one half of the arm64/x86-64
    #      difference and not the other -- ADR-0031 option 4).
    #    * MSVC -- handled in the if(MSVC) branch above, under ADR-0032:
    #      /arch:AVX2 at the default non-contracting /fp:precise, guarded by
    #      the same ANAMORPH_X86_ISA_BASELINE option and asserted per push by
    #      the windows job's toolset gate and the blocking windows-avx2-ab
    #      A/B. The -march/-ffp-contract pair below remains GCC/Clang-only.
    #    * clang-cl -- would take the if(MSVC) branch (CMake sets MSVC=1 for
    #      it) and so now structurally INHERITS /arch:AVX2 despite no twin dump
    #      ever having run on it. That is a STRUCTURAL outcome, not a validated
    #      decision: clang-cl is used by no build, and if one ever appears it
    #      is a THIRD case (MSVC branch, GCC-style flags via /clang:) needing
    #      its own investigation and ADR -- see COMPATIBILITY_MATRIX
    #      "Toolchains the ISA baseline is (and is not) validated for".
    #
    #  -Xarch_x86_64 ON APPLE, ALWAYS -- including a thin x86_64 build, so the
    #  two cases stay one code path. A universal build runs the driver once per
    #  architecture from ONE compile command; an unqualified -march=haswell is
    #  handed to the arm64 invocation too and fails the build. SHELL: keeps each
    #  -Xarch_ token glued to the flag it qualifies and stops CMake de-duplicating
    #  the two identical -Xarch_x86_64 tokens.
    #
    #  COMPILE options, not link options: under LTO the codegen happens at link
    #  time, but both toolchains stream the target CPU/feature set per function
    #  into the IR (LLVM's target-cpu/target-features attributes; GCC's
    #  per-function target options), so the compile-time -march is what governs
    #  the code that is finally emitted.
    # ------------------------------------------------------------------------
    if(NOT ANAMORPH_X86_ISA_BASELINE)
        message(WARNING
            "Anamorph: ANAMORPH_X86_ISA_BASELINE=OFF -- building WITHOUT the ADR-0031 x86-64 "
            "baseline. This is the frozen pre-0.9.5 ISA, for comparison builds only: it is ~17% "
            "slower and it is NOT the shipped configuration.")
    elseif(APPLE)
        if(CMAKE_OSX_ARCHITECTURES MATCHES "x86_64"
           OR (NOT CMAKE_OSX_ARCHITECTURES AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$"))
            target_compile_options(AnamorphHardening INTERFACE
                "SHELL:-Xarch_x86_64 -march=haswell"
                "SHELL:-Xarch_x86_64 -ffp-contract=off")
            message(STATUS "Anamorph: x86-64 ISA baseline (ADR-0031) applied to the x86_64 slice")
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
        target_compile_options(AnamorphHardening INTERFACE
            -march=haswell -ffp-contract=off)
        message(STATUS "Anamorph: x86-64 ISA baseline (ADR-0031) -- -march=haswell -ffp-contract=off")
    endif()
    if(APPLE)
        # ------------------------------------------------------------------
        #  -object_path_lto: the one flag that makes macOS crash symbolication
        #  EXIST. Not an optimisation, not a hardening flag -- a fix for a
        #  contract this project claimed to meet and did not.
        #
        #  WHAT WAS BROKEN. `dsymutil` does not read DWARF out of the linked
        #  binary; it reads the binary's DEBUG MAP (the `N_OSO` stabs), walks
        #  back to each object file the linker consumed, and pulls the DWARF
        #  from there. Under `-flto` (which `juce_recommended_lto_flags` puts
        #  on the shipped plugin) ld64 does codegen itself and writes the
        #  merged result to a TEMPORARY object, which it deletes when the link
        #  finishes. The OSO entry then names a path that no longer exists, so
        #  `dsymutil` warns "unable to open object file", exits 0, and emits a
        #  dSYM with no usable DWARF -- which the packaging step correctly
        #  discards. The net effect was that `Anamorph-macOS-debug` was never
        #  produced, on the one platform whose users routinely hand a developer
        #  an OS crash log. Linux and Windows were unaffected: `objcopy
        #  --only-keep-debug` and MSVC's `/DEBUG` both read the LINKED image.
        #
        #  WHAT THIS CHANGES, precisely: where ld64 writes that temporary. The
        #  bitcode, the codegen and the shipped bytes are untouched -- LTO is
        #  still LTO, and this is emphatically NOT a workaround that turns it
        #  off. The macOS jobs assert both halves after the build: that the
        #  directory below is non-empty (so LTO really ran) and that a
        #  UUID-matched dSYM with compile units came out (so the fix worked).
        #
        #  IT MUST BE A DIRECTORY, not a file. A universal build runs the
        #  linker once per architecture; a single file path would have the
        #  second slice overwrite the first, leaving one slice's OSO pointing
        #  at the other slice's object. Given a directory, ld64 generates a
        #  unique name per link invocation, so the two slices -- and the three
        #  format targets that link separately -- coexist.
        #
        #  The directory is created at CONFIGURE time because ld64 does not
        #  create it: a missing path makes the link fail rather than silently
        #  fall back, which is the failure direction we want if this is ever
        #  edited carelessly.
        set(ANAMORPH_LTO_OBJECT_DIR "${CMAKE_BINARY_DIR}/lto-objects")
        file(MAKE_DIRECTORY "${ANAMORPH_LTO_OBJECT_DIR}")
        target_link_options(AnamorphHardening INTERFACE
            -Wl,-dead_strip
            "-Wl,-object_path_lto,${ANAMORPH_LTO_OBJECT_DIR}")
    else()
        target_link_options(AnamorphHardening INTERFACE
            -Wl,--gc-sections
            -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack)

        # CLANG ON LINUX LINKS WITH LLD, and this is a correctness requirement
        # rather than a preference. It is scoped to Clang deliberately: the
        # SHIPPED Linux binary is built by GCC (the `linux` job), which never
        # reaches this branch, so the released artifact's link is unchanged by
        # this block. Added for the `linux-clang` job, which builds the LTO'd
        # plugin target with Clang.
        #
        # GNU ld scans a static archive ONCE, in command-line order, selecting
        # the members that resolve the symbols undefined at that moment. Under
        # `-flto` (which juce_recommended_lto_flags applies to the plugin) the
        # archive members are BITCODE: codegen runs only AFTER that scan, and the
        # merged object it emits needs symbols the scan already passed over --
        # out-of-line copies, vtables and typeinfo that only became referenced
        # once the optimiser had inlined and devirtualised across translation
        # units. ld.bfd does not rescan, so those become undefined references.
        # The failure is spectacular and misleading: the plugin link reports
        # hundreds of missing JUCE symbols that are demonstrably present in
        # `libAnamorph_SharedCode.a`.
        #
        # Whether it fires at all depends on the binutils/plugin pairing -- the
        # same source, same compiler and same flags link cleanly against one
        # `/usr/lib/bfd-plugins` and fail against another. That is precisely why
        # it is pinned here rather than worked around inside the one CI job.
        #
        # lld resolves LTO symbol requests against archives correctly, which is
        # the property being bought. `-fuse-ld=lld` names the linker GENERICALLY
        # on purpose: which `ld.lld` the driver selects is the environment's
        # business, not this file's, and it varies with how clang was PACKAGED --
        # measured: Ubuntu's `clang++-18` takes `/usr/bin/ld.lld` (the distro
        # alternative) while apt.llvm.org's `clang++-22` takes its own
        # `/usr/lib/llvm-22/bin/ld.lld`. Clang searches its own program path
        # before PATH, so a caller cannot redirect it by prepending a directory
        # (tried; it does not work). This file therefore guarantees no version
        # match; CI installs `lld-<major>` beside `clang-<major>` so the pinned
        # toolchain matches by construction (ADR-0028). Where they DO diverge,
        # bitcode is forward-compatible and the reverse fails loudly, not silently.
        #
        # PROBED rather than assumed: `check_linker_flag` links a real TU with the
        # flag in the configuration actually being built (sanitizer flags
        # included), so the answer is about this environment, not about a version
        # number. Where lld is absent the build falls back to the default linker
        # with a warning that names the hazard, because refusing to configure
        # would be a worse failure than a link that may well succeed.
        # `scripts/setup-linux.sh` installs lld, which is what makes the probe
        # succeed on a machine set up by the documented route.
        #
        # The probe result is CACHED in ANAMORPH_HAVE_LLD, so a build directory
        # configured before lld was installed keeps the fallback until that cache
        # entry is cleared. CI always configures a fresh tree; locally, delete
        # `build-clang/CMakeCache.txt` if the warning outlives the install.
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            include(CheckLinkerFlag)
            check_linker_flag(CXX "-fuse-ld=lld" ANAMORPH_HAVE_LLD)
            if(ANAMORPH_HAVE_LLD)
                target_link_options(AnamorphHardening INTERFACE -fuse-ld=lld)
            else()
                message(WARNING
                    "Anamorph: Clang on Linux without lld. LTO links through GNU ld can fail with "
                    "undefined references to symbols that ARE in libAnamorph_SharedCode.a (one-pass "
                    "archive scan vs post-scan LTO codegen). Install lld (scripts/setup-linux.sh "
                    "does) if a link fails that way.")
            endif()
        endif()
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
    PLUGIN_MANUFACTURER_CODE RTec   # vendor-wide, was Anmf; host IDENTITY - frozen, ADR-0023
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

# Wrapper + GUI sources. Single source of truth: AnamorphStateTests below
# compiles the SAME list into its own console target, so a new wrapper/GUI file
# must never be added to only one of them.
set(ANAMORPH_PLUGIN_SOURCES
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/PluginParameters.cpp
    src/PresetManager.cpp
    src/gui/LookAndFeel.cpp
    src/gui/Vectorscope.cpp
    src/gui/CorrelationMeter.cpp
    src/gui/LevelMeter.cpp
    src/gui/SpectrumImager.cpp
    src/gui/PhysicalMouseButtons.cpp
)

# The macOS half of the physical-mouse-button query is Objective-C++ (it calls
# +[NSEvent pressedMouseButtons], the one API that reports buttons this process
# never received events for -- see src/gui/PhysicalMouseButtons.h). Added only on
# Apple; the .cpp beside it compiles to nothing there and carries the portable
# forward everywhere else, so exactly one definition exists on every platform.
if(APPLE)
    list(APPEND ANAMORPH_PLUGIN_SOURCES src/gui/PhysicalMouseButtons_mac.mm)
endif()

target_sources(Anamorph PRIVATE ${ANAMORPH_PLUGIN_SOURCES})

target_include_directories(Anamorph PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Versioning (spec feedback #32): a human main version (0.x.y, < 1.0.0 pre-release)
# plus a build/dev number synced with CI (pass -DANAMORPH_BUILD_NUMBER=${github.run_number}).
set(ANAMORPH_BUILD_NUMBER "0" CACHE STRING "CI build / dev number shown in the About box")

# The build number is attached to the ONE translation unit that reads it, not to
# the targets. It is the only definition in this file whose value changes on
# every build -- CI passes ${{ github.run_number }} -- and a target-wide
# definition puts that changing value on the command line of every TU in the
# target, including all of JUCE.
#
# That is what made compiler caching almost worthless here: measured on the
# Linux Release build, the two targets carrying this definition are 84.4% of
# total compile time (AnamorphStateTests 57.7% + Anamorph 26.7%), so 84.4% of
# the work was guaranteed to miss the cache on every single run purely because
# an About-box string had incremented. Scoped to PluginEditor.cpp the same runs
# hit for everything else and recompile two objects.
#
# src/PluginEditor.cpp already carries an `#ifndef ANAMORPH_BUILD_NUMBER ->
# "0"` fallback (:36-37), so this is not a new contract; it is the same
# definition delivered to the same reader through a narrower door. Source-file
# properties are directory-scoped, and BOTH consumers -- the shared-code target
# and AnamorphStateTests -- compile this file from this directory, so both still
# see the real number. Nothing else ever read it: `grep ANAMORPH_BUILD_NUMBER
# src/` matches PluginEditor.cpp alone.
set_source_files_properties(src/PluginEditor.cpp PROPERTIES
    COMPILE_DEFINITIONS ANAMORPH_BUILD_NUMBER="${ANAMORPH_BUILD_NUMBER}")

target_compile_definitions(Anamorph PUBLIC
    ANAMORPH_VERSION_STRING="${PROJECT_VERSION}"
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
#  Headless self-tests (both gates, see scripts/run-tests.sh):
#    AnamorphTests      -- DSP acceptance suite
#    AnamorphStateTests -- state / parameter-compatibility suite (below)
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

    # ------------------------------------------------------------------------
    #  State-compatibility self-tests (headless gate, scripts/run-tests.sh)
    #
    #  Exercises the REAL AnamorphAudioProcessor (state round-trip, parameter
    #  registry snapshot, legacy migrations, presets), so it compiles the
    #  plugin sources into its own console target — the same each-target-
    #  compiles-the-sources pattern as the AnamorphDSP INTERFACE lib; JUCE's
    #  shared-code target links its modules PRIVATE, so it cannot be reused
    #  here without restructuring the shipped build. The editor/GUI sources
    #  are compiled (createEditor() references them), and since 2026-08-21 the
    #  suite also constructs and destroys the editor -- headlessly, opening no
    #  window. No target or module change was needed for that: this target
    #  already compiled PluginEditor.cpp and already linked juce_audio_utils,
    #  juce_dsp and juce_opengl, which is why the editor lifetime test lives in
    #  the existing suite rather than in a new (review-gated) target.
    # ------------------------------------------------------------------------
    juce_add_console_app(AnamorphStateTests PRODUCT_NAME "AnamorphStateTests")
    target_sources(AnamorphStateTests PRIVATE
        tests/state_tests.cpp
        ${ANAMORPH_PLUGIN_SOURCES})
    target_include_directories(AnamorphStateTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(AnamorphStateTests PUBLIC
        ANAMORPH_VERSION_STRING="${PROJECT_VERSION}"
        # ANAMORPH_BUILD_NUMBER is attached to PluginEditor.cpp instead -- see
        # the set_source_files_properties() call and its reasoning above. This
        # target compiles ${ANAMORPH_PLUGIN_SOURCES}, so it still gets it there.
        ANAMORPH_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(AnamorphStateTests
        PRIVATE
            AnamorphDSP
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_opengl
        PUBLIC
            AnamorphHardening
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()

# ----------------------------------------------------------------------------
#  AnamorphBench -- the measurement procedure `PERFORMANCE_BUDGET.md` prescribes.
#
#  OUTSIDE the ANAMORPH_BUILD_TESTS block on purpose: the option is documented as
#  independent, and nesting it would make `-DANAMORPH_BUILD_BENCH=ON
#  -DANAMORPH_BUILD_TESTS=OFF` build nothing at all -- silently, which is the
#  worst way for an option to be a lie. It needs only AnamorphDSP, which is
#  defined above either way.
#
#  IT LINKS THE SHIPPED FLAG SET, including `juce_recommended_lto_flags`, because
#  the budget document is explicit that a bench which does not is "measuring a
#  different binary": LTO and the hardening flags are part of what users install.
#  That makes this the ONLY console target here carrying the LTO flags, which is
#  intended -- the correctness suites deliberately do not (see the sanitizers and
#  linux-lto-tests jobs for why).
# ----------------------------------------------------------------------------
if(ANAMORPH_BUILD_BENCH)
    juce_add_console_app(AnamorphBench PRODUCT_NAME "AnamorphBench")
    target_sources(AnamorphBench PRIVATE tests/bench.cpp)
    target_include_directories(AnamorphBench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(AnamorphBench PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(AnamorphBench
        PRIVATE
            AnamorphDSP
            juce::juce_dsp
            juce::juce_audio_basics
        PUBLIC
            AnamorphHardening
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

# ----------------------------------------------------------------------------
#  AnamorphDspDump -- the DSP bit-identity harness DEPENDENCY_POLICY rule 2
#  needs, committed.
#
#  The rule has always been "a JUCE bump must prove bit-identical engine
#  output". Two bumps have proved it. Neither left the instrument behind: both
#  used a scratchpad tool "per the xbench.cpp precedent". So the gate was
#  permanent and the tool was disposable, and each bump rebuilt it -- which is
#  how the 8.0.14 -> 9.0.0 run shipped a scenario set that left `algoAmount` at
#  its identity default and hashed all four algorithms the same.
#
#  IT DOES NOT LINK juce_recommended_lto_flags, and that is the one deliberate
#  difference from AnamorphBench next door. The bench must measure the shipped
#  binary, so it carries the shipped flags. This tool must isolate ONE variable
#  -- the dependency -- and LTO is a second one: it lets the linker's inlining
#  decisions differ between two runs for reasons that have nothing to do with
#  the JUCE version under test, turning a clean question into a noisy one.
#
#  Outside the ANAMORPH_BUILD_TESTS block, same reasoning as the bench above.
# ----------------------------------------------------------------------------
if(ANAMORPH_BUILD_DSPDUMP)
    juce_add_console_app(AnamorphDspDump PRODUCT_NAME "AnamorphDspDump")
    target_sources(AnamorphDspDump PRIVATE tests/dsp_dump.cpp)
    target_include_directories(AnamorphDspDump PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(AnamorphDspDump PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1)
    target_link_libraries(AnamorphDspDump
        PRIVATE
            AnamorphDSP
            juce::juce_dsp
            juce::juce_audio_basics
        PUBLIC
            AnamorphHardening
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()

# ----------------------------------------------------------------------------
#  AnamorphFuzzState -- libFuzzer over the host state-restore path.
#
#  It compiles the PLUGIN SOURCES, exactly as AnamorphStateTests does, because
#  the target under test is the real `AnamorphAudioProcessor::setStateInformation`
#  rather than a re-implementation of it. That is also why the editor sources are
#  in the list: `createEditor()` references them, and they are linked but never
#  instantiated (the same arrangement, and the same reason, as the state suite).
#
#  NO `main`: libFuzzer supplies one, and `-fsanitize=fuzzer` brings it in.
#
#  THAT FLAG IS ON THE TARGET, NOT IN THE CACHE VARIABLES, and it has to be:
#  libFuzzer's `main` collides with the one in CMake's own compiler-probe
#  program, so `-DCMAKE_CXX_FLAGS=-fsanitize=fuzzer` fails at `project()` with a
#  duplicate-`main` link error before any of this is reached. The caller still
#  supplies the *detector* sanitizers (`address,undefined` in the `fuzz` job)
#  through the usual variables; only the fuzzer driver itself is target-scoped.
# ----------------------------------------------------------------------------
if(ANAMORPH_BUILD_FUZZ)
    add_executable(AnamorphFuzzState
        tests/fuzz_state.cpp
        ${ANAMORPH_PLUGIN_SOURCES})
    target_include_directories(AnamorphFuzzState PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
    target_compile_definitions(AnamorphFuzzState PUBLIC
        ANAMORPH_VERSION_STRING="${PROJECT_VERSION}"
        ANAMORPH_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_STANDALONE_APPLICATION=1)
    target_compile_options(AnamorphFuzzState PRIVATE -fsanitize=fuzzer)
    target_link_options(AnamorphFuzzState PRIVATE -fsanitize=fuzzer)
    target_link_libraries(AnamorphFuzzState
        PRIVATE
            AnamorphDSP
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_opengl
        PUBLIC
            AnamorphHardening
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()

