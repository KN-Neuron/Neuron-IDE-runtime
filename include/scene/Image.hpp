#ifndef IMAGE_HPP
#define IMAGE_HPP

#include <filesystem>
#include <memory>

#include "scene/Bitmap.hpp"

// An image file decoded into memory, plus its texture once uploaded to a render target.
class Image {
   public:
    // Throws std::runtime_error when the file cannot be opened or decoded.
    explicit Image(std::filesystem::path path);

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return filePath; }

    // See Bitmap::upload() for what it throws.
    void upload(const std::shared_ptr<SDL_Renderer>& target);

    // Null until upload() has succeeded.
    [[nodiscard]] SDL_Texture* texture() const noexcept;

   private:
    std::filesystem::path filePath;
    Bitmap                bitmap;
};

#endif  // IMAGE_HPP
