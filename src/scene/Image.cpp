#include "scene/Image.hpp"

#include <SDL2/SDL.h>
#include <SDL_image.h>

#include <logging/Logging.hpp>
#include <stdexcept>
#include <string>
#include <utility>

#include "Utf8Path.hpp"

namespace {
quill::Logger* logger() { return logging::logger(logging::Subsystem::Scene); }

SDL_Surface* decode(const std::filesystem::path& path) {
    SDL_Surface* pixels = IMG_Load(utf8::toString(path).c_str());
    if (pixels == nullptr) {
        throw std::runtime_error("cannot load image '" + utf8::toString(path) +
                                 "': " + IMG_GetError());
    }
    return pixels;
}
}  // namespace

Image::Image(std::filesystem::path path)
    : filePath(std::move(path)), bitmap(decode(filePath), "'" + utf8::toString(filePath) + "'") {
    LOG_DEBUG(logger(), "Decoded '{}' ({}x{} px)", utf8::toString(filePath), bitmap.width(),
              bitmap.height());
}

void Image::upload(const std::shared_ptr<SDL_Renderer>& target) { bitmap.upload(target); }

SDL_Texture* Image::texture() const noexcept { return bitmap.texture(); }
