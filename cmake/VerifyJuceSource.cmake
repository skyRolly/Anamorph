# ADR-0071: post-configuration provenance, before product targets are generated.
# FetchContent_MakeAvailable has already configured JUCE. This is not a gate on
# configuration-time execution; configure only trusted sources.
# ANAMORPH_JUCE_PATH keeps its existing independent local-checkout branch.
set(_anamorph_juce_override FALSE)
if(DEFINED FETCHCONTENT_SOURCE_DIR_JUCE AND NOT "${FETCHCONTENT_SOURCE_DIR_JUCE}" STREQUAL "")
    set(_anamorph_juce_override TRUE)
endif()

find_package(Git QUIET)
set(_anamorph_juce_git_root FALSE)
if(Git_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${juce_SOURCE_DIR}" rev-parse --show-toplevel
        RESULT_VARIABLE _anamorph_juce_root_result
        OUTPUT_VARIABLE _anamorph_juce_root OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_VARIABLE _anamorph_juce_root_error TIMEOUT 10)
    if(_anamorph_juce_root_result EQUAL 0)
        file(REAL_PATH "${juce_SOURCE_DIR}" _anamorph_juce_source_real)
        file(REAL_PATH "${_anamorph_juce_root}" _anamorph_juce_root_real)
        if(WIN32)
            string(TOLOWER "${_anamorph_juce_source_real}" _anamorph_juce_source_real)
            string(TOLOWER "${_anamorph_juce_root_real}" _anamorph_juce_root_real)
        endif()
        # An archive extracted inside another repository must not inherit that
        # unrelated repository's HEAD as its supposed JUCE identity.
        if(_anamorph_juce_source_real STREQUAL _anamorph_juce_root_real)
            set(_anamorph_juce_git_root TRUE)
        endif()
    endif()
endif()

# EXISTS follows links and can also be false for unreadable entries. Only a
# source with no .git directory entry qualifies for the archive exception.
# Quote glob metacharacters in the parent path so it remains a literal path.
string(REPLACE "[" "[[]" _anamorph_juce_metadata_pattern "${juce_SOURCE_DIR}")
string(REPLACE "*" "[*]" _anamorph_juce_metadata_pattern "${_anamorph_juce_metadata_pattern}")
string(REPLACE "?" "[?]" _anamorph_juce_metadata_pattern "${_anamorph_juce_metadata_pattern}")
file(GLOB _anamorph_juce_git_metadata LIST_DIRECTORIES TRUE "${_anamorph_juce_metadata_pattern}/.git")

if(_anamorph_juce_git_root)
    execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${juce_SOURCE_DIR}" rev-parse HEAD
        RESULT_VARIABLE _anamorph_juce_git_result
        OUTPUT_VARIABLE _anamorph_juce_head OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_VARIABLE _anamorph_juce_git_error TIMEOUT 10)
    if(NOT _anamorph_juce_git_result EQUAL 0)
        message(FATAL_ERROR "Anamorph: cannot verify JUCE source HEAD: ${_anamorph_juce_git_error}")
    endif()
    string(LENGTH "${ANAMORPH_JUCE_TAG}" _anamorph_juce_pin_length)
    string(TOLOWER "${ANAMORPH_JUCE_TAG}" _anamorph_juce_expected)
    if(_anamorph_juce_pin_length EQUAL 40 AND ANAMORPH_JUCE_TAG MATCHES "^[0-9a-fA-F]+$"
       AND NOT _anamorph_juce_head STREQUAL _anamorph_juce_expected)
        message(FATAL_ERROR "Anamorph: JUCE source HEAD ${_anamorph_juce_head} does not match immutable pin ${ANAMORPH_JUCE_TAG}; reset the fetched checkout or use ANAMORPH_JUCE_PATH for an intentional comparison.")
    endif()
    message(STATUS "Anamorph: verified JUCE source HEAD ${_anamorph_juce_head} at ${juce_SOURCE_DIR}")
elseif(_anamorph_juce_override AND NOT _anamorph_juce_git_metadata)
    message(WARNING "Anamorph: unverified JUCE source override at ${juce_SOURCE_DIR}: no Git metadata rooted at this source directory. Trusted offline/development archives remain supported, but no immutable commit verification is claimed. Release/CI pin evidence requires a verifiable Git checkout.")
else()
    # A normal fetched checkout, or an override with broken Git metadata, may
    # not silently fall back to the archive policy.
    message(FATAL_ERROR "Anamorph: cannot verify JUCE source Git identity at ${juce_SOURCE_DIR}. A normal FetchContent build requires a verifiable JUCE Git checkout. Trusted source archives must be selected explicitly with FETCHCONTENT_SOURCE_DIR_JUCE or ANAMORPH_JUCE_PATH.")
endif()

unset(_anamorph_juce_metadata_pattern)
unset(_anamorph_juce_git_metadata)
unset(_anamorph_juce_override)
unset(_anamorph_juce_git_root)
unset(_anamorph_juce_root_result)
unset(_anamorph_juce_root)
unset(_anamorph_juce_root_error)
unset(_anamorph_juce_source_real)
unset(_anamorph_juce_root_real)
unset(_anamorph_juce_git_result)
unset(_anamorph_juce_git_error)
unset(_anamorph_juce_head)
unset(_anamorph_juce_pin_length)
unset(_anamorph_juce_expected)
