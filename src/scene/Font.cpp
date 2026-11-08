#include "scene/Font.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>

#include "Utf8Path.hpp"
#include "scene/DefaultFont.hpp"

namespace {
constexpr SDL_Color kWhite{255, 255, 255, 255};

// SDL_ttf draws '\r', '\t' and a '\n' at the very end of the text as visible missing-glyph boxes.
std::string withoutStrayControls(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const char character : text) {
        if (character == '\t') {
            result.push_back(' ');
        } else if (character != '\r') {
            result.push_back(character);
        }
    }
    result.erase(result.find_last_not_of(" \n") + 1);
    return result;
}
}  // namespace

Font::Session::Session() {
    if (TTF_Init() != 0) {
        throw std::runtime_error(std::string("TTF_Init failed: ") + TTF_GetError());
    }
}

Font::Session::~Session() { TTF_Quit(); }

void Font::FontCloser::operator()(TTF_Font* font) const noexcept { TTF_CloseFont(font); }

Font::Font(const std::filesystem::path& path, int pixelSize) {
    if (path.empty()) {
        const auto bytes = defaultFontBytes();
        handle.reset(
            TTF_OpenFontRW(SDL_RWFromConstMem(bytes.data(), static_cast<int>(bytes.size())),
                           /*freesrc=*/1, pixelSize));
    } else {
        handle.reset(TTF_OpenFont(utf8::toString(path).c_str(), pixelSize));
    }
    if (!handle) {
        throw std::runtime_error("cannot load font '" +
                                 (path.empty() ? "built-in" : utf8::toString(path)) +
                                 "': " + TTF_GetError());
    }
    TTF_SetFontWrappedAlign(handle.get(), TTF_WRAPPED_ALIGN_CENTER);
}

std::optional<Bitmap> Font::render(std::string_view text, int wrapWidth) const {
    const std::string printable = withoutStrayControls(text);

    int width = 0;
    if (TTF_SizeUTF8(handle.get(), printable.c_str(), &width, nullptr) == 0 && width == 0) {
        return std::nullopt;
    }

    SDL_Surface* pixels =
        TTF_RenderUTF8_Blended_Wrapped(handle.get(), printable.c_str(), kWhite, wrapWidth);
    if (pixels == nullptr) {
        throw std::runtime_error("cannot render text '" + printable + "': " + TTF_GetError());
    }
    return std::optional<Bitmap>(std::in_place, pixels, "text '" + printable + "'");
}

int Font::widestWord(std::string_view text) const {
    int                widest = 0;
    std::istringstream words{std::string(text)};
    for (std::string word; words >> word;) {
        int width = 0;
        if (TTF_SizeUTF8(handle.get(), word.c_str(), &width, nullptr) == 0) {
            widest = std::max(widest, width);
        }
    }
    return widest;
}
