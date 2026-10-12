#ifndef COMPONENTBUILDCONTEXT_HPP
#define COMPONENTBUILDCONTEXT_HPP

#include <filesystem>

// What a component creator may need beyond its own proto message and owner. Parser builds one per
// scene and hands it to every creator.
struct ComponentBuildContext {
    // Folder that relative asset paths in the experiment file resolve against: the folder holding
    // the .neuroz file. Empty for a scene parsed from a bare stream, so relative paths resolve
    // against the working directory.
    std::filesystem::path assetRoot;

    // An absolute path is returned as-is and a relative one is taken relative to assetRoot. An
    // empty path stays empty, so the caller can still reject it.
    [[nodiscard]] std::filesystem::path resolveAsset(const std::filesystem::path& path) const {
        if (path.empty() || path.is_absolute()) {
            return path;
        }
        return assetRoot / path;
    }
};

#endif  // COMPONENTBUILDCONTEXT_HPP
