#pragma once

#include <juce_data_structures/juce_data_structures.h>
#include <cmath>

namespace anamorph
{
    // XML attributes are text already. On a PRIVATE serialization copy, spell a NaN using
    // JUCE's floating-point String constructor, bypassing var::toString/serialiseDouble's
    // undefined NaN-to-int conversion (still present in JUCE 9.0.4). This is the same spelling
    // that formatter eventually emits on the validated toolchains, including the NaN sign.
    // No live parameter, cached A/B tree, numeric resolver or finite value is changed.
    inline std::unique_ptr<juce::XmlElement> createXmlWithSafeNumbers (const juce::ValueTree& source)
    {
        auto copy = source.createCopy();
        const auto spellNaNs = [] (auto&& self, juce::ValueTree tree) -> void
        {
            for (int i = 0; i < tree.getNumProperties(); ++i)
            {
                const auto name = tree.getPropertyName (i);
                const auto& value = tree.getProperty (name);
                if (value.isDouble() && std::isnan ((double) value))
                    tree.setProperty (name, juce::String ((double) value, 15, true), nullptr);
            }
            for (auto child : tree) self (self, child);
        };
        spellNaNs (spellNaNs, copy);
        return copy.createXml();
    }

    inline juce::String toXmlStringWithSafeNumbers (const juce::ValueTree& source)
    {
        if (auto xml = createXmlWithSafeNumbers (source)) return xml->toString();
        return {};
    }
}
