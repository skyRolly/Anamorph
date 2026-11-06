// Disposable filesystem for the nonshipping State executable. The production
// preset manager still performs its real save/load/list operations, but its
// State-build directory is installed before any processor is constructed.
#pragma once

#include <cstdio>
#include <cstdlib>
#include <memory>
#include <vector>
#if JUCE_WINDOWS
 #include <filesystem>
 #include <system_error>
#else
 #include <cerrno>
 #include <stdlib.h> // realpath
 #include <sys/stat.h>
#endif

namespace state_test_filesystem
{
    inline juce::File temporaryDirectory;

    // Resolve existing symlinks without requiring the leaf to exist. macOS's
    // deployment target predates libc++ std::filesystem (10.15), so POSIX
    // platforms use realpath on the nearest existing ancestor instead.
    inline juce::File canonicalPath (const juce::File& file)
    {
       #if JUCE_WINDOWS
        std::error_code error;
        const auto path = std::filesystem::weakly_canonical (
            std::filesystem::path (file.getFullPathName().toWideCharPointer()), error);
        return error ? juce::File() : juce::File (juce::String (path.c_str()));
       #else
        auto existing = file;
        std::vector<juce::String> suffix;
        while (! existing.exists())
        {
            if (existing.isSymbolicLink()) return {};
            const auto parent = existing.getParentDirectory();
            if (parent == existing || existing.getFileName().isEmpty()) return {};
            suffix.push_back (existing.getFileName());
            existing = parent;
        }
        std::unique_ptr<char, decltype (&std::free)> path (
            ::realpath (existing.getFullPathName().toRawUTF8(), nullptr), &std::free);
        if (path == nullptr) return {};
        juce::File result (juce::String::fromUTF8 (path.get()));
        for (auto it = suffix.rbegin(); it != suffix.rend(); ++it)
            result = result.getChildFile (*it);
        return result;
       #endif
    }

    inline bool createExclusiveDirectory (const juce::File& directory, bool& collision)
    {
       #if JUCE_WINDOWS
        std::error_code error;
        const bool created = std::filesystem::create_directory (
            std::filesystem::path (directory.getFullPathName().toWideCharPointer()), error);
        collision = ! created && ! error;
        return created;
       #else
        if (::mkdir (directory.getFullPathName().toRawUTF8(), 0700) == 0)
        {
            collision = false;
            return true;
        }
        collision = errno == EEXIST;
        return false;
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
            if (! parent.isDirectory())
                return false;

            const auto canonicalParent = canonicalPath (parent);
            if (canonicalParent == juce::File()) return false;
            const auto productDirectory = anamorph::PresetManager::presetDirectory();
            if (productDirectory == juce::File()) return false;
            const auto product = canonicalPath (productDirectory);
            if (product == juce::File()) return false;
            if (canonicalParent == product || canonicalParent.isAChildOf (product)) return false;

            for (int attempt = 0; attempt < 16; ++attempt)
            {
                const auto candidate = canonicalParent.getChildFile ("anamorph-state-" + juce::Uuid().toString());
                // Even a caller-selected temporary parent must not put the test
                // root inside the actual preset tree, or encompass that tree.
                if (candidate == product || candidate.isAChildOf (product) || product.isAChildOf (candidate))
                    return false;

                bool collision = false;
                if (! createExclusiveDirectory (candidate, collision))
                {
                    if (! collision) return false;
                    continue; // another process already owns that name
                }
                root = candidate;
                rootIdentifier = root.getFileIdentifier();
                if (rootIdentifier == 0) { root.deleteFile(); root = juce::File(); return false; }
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

        bool cleanup()
        {
            if (root == juce::File()) return true;
            // Never follow a replaced root/symlink or remove an unmarked tree.
            // This is an accidental-collision safeguard, not a hostile-user sandbox.
            if (root.isSymbolicLink() || root.getFileIdentifier() != rootIdentifier
                || marker.isSymbolicLink() || ! marker.existsAsFile()
                || marker.loadFileAsString() != token || ! root.deleteRecursively (false))
            {
                std::fprintf (stderr, "State test profile preserved; cleanup refused or failed: %s\n",
                              root.getFullPathName().toRawUTF8());
                return false;
            }
            root = juce::File();
            return true;
        }

        ~Profile() { (void) cleanup(); }

        const juce::File& directory() const { return root; }

    private:
        juce::File root, marker;
        juce::String token;
        juce::uint64 rootIdentifier = 0;
    };
}
