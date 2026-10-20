#include "scene/ImageLibrary.hpp"

#include <logging/Logging.hpp>

#include "Utf8Path.hpp"
#include "scene/Image.hpp"

namespace {
quill::Logger* logger() { return logging::logger(logging::Subsystem::Scene); }
}  // namespace

std::shared_ptr<const Image> ImageLibrary::load(const std::filesystem::path& path) {
    const std::filesystem::path key = path.lexically_normal();
    if (const auto found = images.find(key); found != images.end()) {
        LOG_DEBUG(logger(), "'{}' is already loaded and is shared", utf8::toString(key));
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
    LOG_INFO(logger(), "Uploaded {} image(s) to the render target", images.size());
}
