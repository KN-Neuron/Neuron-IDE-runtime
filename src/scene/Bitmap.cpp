#include "scene/Bitmap.hpp"

#include <SDL2/SDL.h>

#include <stdexcept>
#include <utility>

void Bitmap::SurfaceDeleter::operator()(SDL_Surface* surface) const noexcept {
    SDL_FreeSurface(surface);
}

void Bitmap::TextureDeleter::operator()(SDL_Texture* texture) const noexcept {
    SDL_DestroyTexture(texture);
}

Bitmap::Bitmap(SDL_Surface* pixels, std::string description)
    : pixels(pixels), description(std::move(description)) {}

void Bitmap::upload(const std::shared_ptr<SDL_Renderer>& target) {
    if (!target) {
        throw std::invalid_argument("Bitmap: render target must not be null");
    }

    SDL_Texture* created = SDL_CreateTextureFromSurface(target.get(), pixels.get());
    if (created == nullptr) {
        throw std::runtime_error("cannot create a texture from " + description + ": " +
                                 SDL_GetError());
    }
    gpuTexture = std::unique_ptr<SDL_Texture, TextureDeleter>(created, TextureDeleter{target});
}

int Bitmap::width() const noexcept { return pixels->w; }

int Bitmap::height() const noexcept { return pixels->h; }
