// Read-only path witness, linked against the same JUCE and PresetManager as the State suite.
#include "PresetManager.h"
#include <cstdio>

int main()
{
    using File = juce::File;
    for (const auto& path : { File::getSpecialLocation (File::userHomeDirectory),
                              File::getSpecialLocation (File::userApplicationDataDirectory),
                              anamorph::PresetManager::presetDirectory(),
                              File::getSpecialLocation (File::tempDirectory) })
        std::printf ("%s\n", path.getFullPathName().toRawUTF8());
}
