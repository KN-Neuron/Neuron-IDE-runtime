#ifndef SDL_TEST_UTILS_HPP
#define SDL_TEST_UTILS_HPP

#include <SDL2/SDL.h>
#include <SDL_image.h>

#include <array>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>

#include "Utf8Path.hpp"

namespace utils {

// One pixel as {r, g, b, a}; ints rather than Uint8 so gtest prints them as numbers.
using Rgba = std::array<int, 4>;

inline constexpr int kSurfaceDepth = 32;

inline std::runtime_error sdlFailure(const std::string& what) {
    return std::runtime_error(what + ": " + SDL_GetError());
}

// A software render target that draws into a CPU surface, so tests can render without a window
// and read the pixels back.
inline std::shared_ptr<SDL_Renderer> makeSoftwareTarget(int width, int height) {
    std::shared_ptr<SDL_Surface> surface(
        SDL_CreateRGBSurfaceWithFormat(0, width, height, kSurfaceDepth, SDL_PIXELFORMAT_RGBA32),
        &SDL_FreeSurface);
    if (!surface) {
        throw sdlFailure("SDL_CreateRGBSurfaceWithFormat");
    }
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(surface.get());
    if (renderer == nullptr) {
        throw sdlFailure("SDL_CreateSoftwareRenderer");
    }
    return {renderer, [surface](SDL_Renderer* target) { SDL_DestroyRenderer(target); }};
}

inline void clear(SDL_Renderer* renderer, const Rgba& color) {
    SDL_SetRenderDrawColor(renderer, static_cast<Uint8>(color[0]), static_cast<Uint8>(color[1]),
                           static_cast<Uint8>(color[2]), static_cast<Uint8>(color[3]));
    SDL_RenderClear(renderer);
}

inline Rgba readPixel(SDL_Renderer* renderer, int column, int row) {
    const SDL_Rect       pixel{column, row, 1, 1};
    std::array<Uint8, 4> bytes{};
    // RGBA32 is r, g, b, a in memory order on any endianness.
    if (SDL_RenderReadPixels(renderer, &pixel, SDL_PIXELFORMAT_RGBA32, bytes.data(),
                             static_cast<int>(bytes.size())) != 0) {
        throw sdlFailure("SDL_RenderReadPixels");
    }
    return {bytes[0], bytes[1], bytes[2], bytes[3]};
}

// Writes a 2x1 PNG whose left pixel is `left` and right pixel is `right`. Stretched over a
// sprite, the two halves show how the image was placed and which way it was turned.
inline void writeTwoPixelPng(const std::filesystem::path& path, const Rgba& left,
                             const Rgba& right) {
    const std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surface(
        SDL_CreateRGBSurfaceWithFormat(0, 2, 1, kSurfaceDepth, SDL_PIXELFORMAT_RGBA32),
        &SDL_FreeSurface);
    if (!surface) {
        throw sdlFailure("SDL_CreateRGBSurfaceWithFormat");
    }

    const auto fill = [&surface](int column, const Rgba& color) {
        const SDL_Rect pixel{column, 0, 1, 1};
        SDL_FillRect(
            surface.get(), &pixel,
            SDL_MapRGBA(surface->format, static_cast<Uint8>(color[0]), static_cast<Uint8>(color[1]),
                        static_cast<Uint8>(color[2]), static_cast<Uint8>(color[3])));
    };
    fill(0, left);
    fill(1, right);

    if (IMG_SavePNG(surface.get(), utf8::toString(path).c_str()) != 0) {
        throw sdlFailure("IMG_SavePNG " + utf8::toString(path));
    }
}

}  // namespace utils

#endif  // SDL_TEST_UTILS_HPP
