#include "scene/ImageLibrary.hpp"

#include "scene/Image.hpp"

std::shared_ptr<const Image> ImageLibrary::load(const std::filesystem::path& path) {
    const std::filesystem::path key = path.lexically_normal();
    if (const auto found = images.find(key); found != images.end()) {
        // TODO(quill): debug, "'<path>' is already loaded and is shared".
        return found->second;
    }

    auto image = std::make_shared<Image>(key);
    images.emplace(key, image);
    return image;
}

void ImageLibrary::upload(const std::shared_ptr<SDL_Renderer>& target) {
    for (const auto& [path, image] : images) {
        image->upload(target);
    }
    // TODO(quill): info, how many images were uploaded.
}
