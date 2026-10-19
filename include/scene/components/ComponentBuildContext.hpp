#ifndef COMPONENTBUILDCONTEXT_HPP
#define COMPONENTBUILDCONTEXT_HPP

#include <filesystem>
#include <memory>
#include <utility>

#include "scene/ImageLibrary.hpp"

class Image;

// What a component creator may need beyond its own proto message and owner. Parser builds one per
// scene and hands it to every creator.
class ComponentBuildContext {
   public:
    // `assetRoot` is the folder that relative asset paths in the experiment file resolve against:
    // the folder holding the .neuroz file, or empty for a scene parsed from a bare stream, so
    // relative paths resolve against the working directory. `images` is the scene's library and
    // must outlive the context.
    ComponentBuildContext(std::filesystem::path assetRoot, ImageLibrary& images)
        : assetRoot(std::move(assetRoot)), images(&images) {}

    // An absolute path is returned as-is and a relative one is taken relative to assetRoot. An
    // empty path stays empty.
    [[nodiscard]] std::filesystem::path resolveAsset(const std::filesystem::path& path) const {
        if (path.empty() || path.is_absolute()) {
            return path;
        }
        return assetRoot / path;
    }

    // The image at `path`, resolved as above, shared with every other component of the scene that
    // shows the same file. Throws std::runtime_error when the file cannot be loaded.
    [[nodiscard]] std::shared_ptr<const Image> loadImage(const std::filesystem::path& path) const {
        return images->load(resolveAsset(path));
    }

   private:
    std::filesystem::path assetRoot;
    ImageLibrary*         images;
};

#endif  // COMPONENTBUILDCONTEXT_HPP
