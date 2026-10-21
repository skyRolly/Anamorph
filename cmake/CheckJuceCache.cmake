# A pre-upgrade build/ retains both CACHE entries despite the new defaults.
# Refuse either known stale value (including a partial reset) before fetching.
# Local checkouts still permit intentional comparisons; never rewrite
# cache overrides whose intent cannot be inferred from their values.
set(_anamorph_stale_juce_tags
    "72782788ce18c2d4d760b28e0921d6ffc6431102"
    "be29c81492b6151c8ea8d14c840e1311963b3a83")
set(_anamorph_stale_juce_versions "9.0.2" "9.0.3")
if(NOT ANAMORPH_JUCE_PATH AND
   (ANAMORPH_JUCE_TAG IN_LIST _anamorph_stale_juce_tags OR
    ANAMORPH_JUCE_VERSION IN_LIST _anamorph_stale_juce_versions))
    message(FATAL_ERROR
        "Anamorph: stale JUCE cache (${ANAMORPH_JUCE_VERSION}, ${ANAMORPH_JUCE_TAG}); this tree pins JUCE 9.0.4.\n"
        "Reset both cached values, then rebuild:\n"
        "  cmake -S \"${CMAKE_SOURCE_DIR}\" -B \"${CMAKE_BINARY_DIR}\""
        " -UANAMORPH_JUCE_TAG -UANAMORPH_JUCE_VERSION\n"
        "For an intentional comparison, use -DANAMORPH_JUCE_PATH=/path/to/JUCE.")
endif()
unset(_anamorph_stale_juce_tags)
unset(_anamorph_stale_juce_versions)
