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

Bitmap::Bitmap(SDL_Surface* pixels, std::string description, ScaleMode scaleMode)
    : pixels(pixels), description(std::move(description)), scaleMode(scaleMode) {}

void Bitmap::upload(const std::shared_ptr<SDL_Renderer>& target) {
    if (!target) {
        throw std::invalid_argument("Bitmap: render target must not be null");
    }

    std::unique_ptr<SDL_Texture, TextureDeleter> created(
        SDL_CreateTextureFromSurface(target.get(), pixels.get()), TextureDeleter{target});
    if (!created) {
        throw std::runtime_error("cannot create a texture from " + description + ": " +
                                 SDL_GetError());
    }
    const SDL_ScaleMode sdlScaleMode =
        scaleMode == ScaleMode::Linear ? SDL_ScaleModeLinear : SDL_ScaleModeNearest;
    if (SDL_SetTextureScaleMode(created.get(), sdlScaleMode) != 0) {
        throw std::runtime_error("cannot set how " + description + " is scaled: " + SDL_GetError());
    }
    gpuTexture = std::move(created);
}

int Bitmap::width() const noexcept { return pixels->w; }

int Bitmap::height() const noexcept { return pixels->h; }
