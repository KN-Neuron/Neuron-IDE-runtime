#ifndef FONT_HPP
#define FONT_HPP

#include <SDL_ttf.h>

#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>

#include "scene/Bitmap.hpp"

// A TrueType font opened at one size, which turns text into pixels.
class Font {
   public:
    // `pixelSize` is the em size in pixels and must be positive. An empty `path` selects the
    // built-in font. Throws std::runtime_error when the font cannot be loaded.
    Font(const std::filesystem::path& path, int pixelSize);

    // UTF-8 `text`, white on a transparent background, sampled smoothly when drawn rotated. Lines
    // break at line endings and between words wherever one would grow past `wrapWidth` pixels.
    // They are centred against each other by what is visible: spaces at either end of a line and
    // blank lines at either end of the text are left out. A word wider than `wrapWidth` is split
    // between characters, and a single character wider than it runs past it. Other control
    // characters are drawn as spaces. Empty when the text has nothing to draw. Throws
    // std::runtime_error when SDL_ttf cannot measure or render it.
    [[nodiscard]] std::optional<Bitmap> render(std::string_view text, int wrapWidth) const;

    // In pixels. A word wider than the wrap width has to be split across lines.
    [[nodiscard]] int widestWord(std::string_view text) const;

   private:
    // SDL_ttf counts TTF_Init calls, so every Font holds one.
    struct Session {
        Session();
        ~Session();
        Session(const Session&)            = delete;
        Session(Session&&)                 = delete;
        Session& operator=(const Session&) = delete;
        Session& operator=(Session&&)      = delete;
    };
    struct FontCloser {
        void operator()(TTF_Font* font) const noexcept;
    };

    // In this order: the font must be closed while the session is still open.
    Session                               session;
    std::unique_ptr<TTF_Font, FontCloser> handle;
};

#endif  // FONT_HPP
