#include "scene/Image.hpp"

#include <SDL2/SDL.h>
#include <SDL_image.h>

#include <stdexcept>
#include <string>
#include <utility>

#include "Utf8Path.hpp"

void Image::SurfaceDeleter::operator()(SDL_Surface* surface) const noexcept {
    SDL_FreeSurface(surface);
}

void Image::TextureDeleter::operator()(SDL_Texture* texture) const noexcept {
    SDL_DestroyTexture(texture);
}

Image::Image(std::filesystem::path path) : filePath(std::move(path)) {
    pixels.reset(IMG_Load(utf8::toString(filePath).c_str()));
    if (!pixels) {
        throw std::runtime_error("cannot load image '" + utf8::toString(filePath) +
                                 "': " + IMG_GetError());
    }
}

void Image::upload(const std::shared_ptr<SDL_Renderer>& target) {
    if (!target) {
        throw std::invalid_argument("Image: render target must not be null");
    }

    SDL_Texture* created = SDL_CreateTextureFromSurface(target.get(), pixels.get());
    if (created == nullptr) {
        throw std::runtime_error("cannot create a texture from '" + utf8::toString(filePath) +
                                 "': " + SDL_GetError());
    }
    gpuTexture = std::unique_ptr<SDL_Texture, TextureDeleter>(created, TextureDeleter{target});
}
