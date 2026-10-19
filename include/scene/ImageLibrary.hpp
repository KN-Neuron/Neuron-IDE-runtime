#ifndef IMAGELIBRARY_HPP
#define IMAGELIBRARY_HPP

#include <cstddef>
#include <filesystem>
#include <map>
#include <memory>

class Image;
class SDL_Renderer;

// The images one scene shows. Each file is decoded once and uploaded once, however many sprites
// show it: an SSVEP grid of 40 objects sharing one checkerboard holds one copy, not 40.
class ImageLibrary {
   public:
    // The image already loaded from `path`, or a newly loaded one. Paths are compared after
    // lexical normalisation, so "stimuli/./face.png" and "stimuli/face.png" are the same file.
    // Throws std::runtime_error when the file cannot be loaded.
    std::shared_ptr<const Image> load(const std::filesystem::path& path);

    // (Re)creates every image's texture on `target`. See Image::upload() for what it throws.
    void upload(const std::shared_ptr<SDL_Renderer>& target);

    [[nodiscard]] std::size_t size() const noexcept { return images.size(); }

   private:
    std::map<std::filesystem::path, std::shared_ptr<Image>> images;
};

#endif  // IMAGELIBRARY_HPP
