// Disposable filesystem for the nonshipping State executable. The production
// preset manager still performs its real save/load/list operations, but its
// State-build directory is installed before any processor is constructed.
#pragma once

#include <filesystem>
#include <cstdio>
#include <system_error>

namespace state_test_filesystem
{
    inline juce::File temporaryDirectory;

    inline std::filesystem::path nativePath (const juce::File& file)
    {
       #if JUCE_WINDOWS
        return std::filesystem::path (file.getFullPathName().toWideCharPointer());
       #else
        return std::filesystem::path (file.getFullPathName().toRawUTF8());
       #endif
    }

    inline juce::File temporaryFile (juce::StringRef suffix)
    {
        // Like File::createTempFile, this returns an unused name, not an open file.
        return temporaryDirectory.getNonexistentChildFile ("temp-" + juce::Uuid().toString(), suffix);
    }

    class Profile
    {
    public:
        Profile() = default;
        Profile (const Profile&) = delete;
        Profile& operator= (const Profile&) = delete;

        bool initialise (const juce::File& parent)
        {
            namespace fs = std::filesystem;
            std::error_code error;
            if (! parent.isDirectory())
                return false;

            const auto canonicalParent = fs::canonical (nativePath (parent), error);
            if (error) return false;
            const auto productDirectory = anamorph::PresetManager::presetDirectory();
            if (productDirectory == juce::File()) return false;
            const auto canonicalProduct = fs::weakly_canonical (nativePath (productDirectory), error);
            if (error) return false;
            const juce::File product (juce::String (canonicalProduct.c_str()));

            for (int attempt = 0; attempt < 16; ++attempt)
            {
                const juce::File candidate (juce::String ((canonicalParent / ("anamorph-state-" +
                                                    juce::Uuid().toString().toStdString())).c_str()));
                // Even a caller-selected temporary parent must not put the test
                // root inside the actual preset tree, or encompass that tree.
                if (candidate == product || candidate.isAChildOf (product) || product.isAChildOf (candidate))
                    return false;

                error.clear();
                if (! fs::create_directory (nativePath (candidate), error))
                {
                    if (error) return false;
                    continue; // another process already owns that name
                }
                root = candidate;
                rootIdentifier = root.getFileIdentifier();
                if (rootIdentifier == 0) { root.deleteFile(); root = juce::File(); return false; }
               #if ! JUCE_WINDOWS
                fs::permissions (nativePath (root), fs::perms::owner_all, fs::perm_options::replace, error);
                if (error) { root.deleteFile(); root = juce::File(); return false; }
               #endif
                token = juce::Uuid().toString();
                marker = root.getChildFile (".anamorph-state-owner");
                if (! marker.replaceWithText (token))
                {
                    root.deleteFile(); // only succeeds if the new directory is empty
                    root = juce::File();
                    return false;
                }
                const auto presets = root.getChildFile ("presets");
                const auto temporary = root.getChildFile ("temp");
                if (! presets.createDirectory() || ! temporary.createDirectory())
                    return false;

                temporaryDirectory = temporary;
                anamorph::PresetManager::setTestPresetDirectory (presets);
                return anamorph::PresetManager::presetDirectory() == presets;
            }
            return false;
        }

        ~Profile()
        {
            if (root == juce::File()) return;
            // Never follow a replaced root/symlink or remove an unmarked tree.
            // This is an accidental-collision safeguard, not a hostile-user sandbox.
            if (root.isSymbolicLink() || root.getFileIdentifier() != rootIdentifier
                || marker.isSymbolicLink() || ! marker.existsAsFile()
                || marker.loadFileAsString() != token || ! root.deleteRecursively (false))
                std::fprintf (stderr, "State test profile preserved; cleanup refused or failed: %s\n",
                              root.getFullPathName().toRawUTF8());
        }

        const juce::File& directory() const { return root; }

    private:
        juce::File root, marker;
        juce::String token;
        juce::uint64 rootIdentifier = 0;
    };
}
