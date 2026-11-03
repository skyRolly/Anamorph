# ADR-0071: report the checkout actually consumed, and refuse a stale immutable
# FetchContent source. The separate ANAMORPH_JUCE_PATH branch permits old/new
# local comparisons; named custom revisions retain their existing behavior.
find_package(Git REQUIRED)
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
unset(_anamorph_juce_git_result)
unset(_anamorph_juce_git_error)
unset(_anamorph_juce_head)
unset(_anamorph_juce_pin_length)
unset(_anamorph_juce_expected)
