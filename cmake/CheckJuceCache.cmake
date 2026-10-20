# A pre-upgrade build/ retains both CACHE entries despite the new defaults.
# Refuse either known stale value (including a partial reset) before fetching.
# Local checkouts still permit intentional 9.0.2 comparisons; never rewrite
# cache overrides whose intent cannot be inferred from their values.
if(NOT ANAMORPH_JUCE_PATH AND
   (ANAMORPH_JUCE_TAG STREQUAL "72782788ce18c2d4d760b28e0921d6ffc6431102" OR
    ANAMORPH_JUCE_VERSION STREQUAL "9.0.2"))
    message(FATAL_ERROR
        "Anamorph: stale JUCE 9.0.2 cache; this tree pins JUCE 9.0.3.\n"
        "Reset both cached values, then rebuild:\n"
        "  cmake -S \"${CMAKE_SOURCE_DIR}\" -B \"${CMAKE_BINARY_DIR}\""
        " -UANAMORPH_JUCE_TAG -UANAMORPH_JUCE_VERSION\n"
        "For an intentional JUCE 9.0.2 comparison, use -DANAMORPH_JUCE_PATH=/path/to/JUCE.")
endif()
