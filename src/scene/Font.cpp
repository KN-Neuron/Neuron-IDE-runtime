#include "scene/Font.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Utf8Path.hpp"
#include "scene/DefaultFont.hpp"

namespace {
constexpr SDL_Color     kWhite{255, 255, 255, 255};
constexpr unsigned char kFirstPrintable   = 0x20;
constexpr unsigned char kDelete           = 0x7F;
constexpr unsigned char kContinuationMask = 0xC0;
constexpr unsigned char kContinuationByte = 0x80;
// With no wrap width, SDL_ttf breaks lines only at '\n'.
constexpr int kNoWrap = 0;

// SDL_ttf has no glyph for control characters and draws each as a box, so line endings become '\n'
// and the rest spaces.
std::string withoutControls(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (std::size_t index = 0; index < text.size(); ++index) {
        const auto byte = static_cast<unsigned char>(text[index]);
        if (byte == '\r') {
            if (index + 1 == text.size() || text[index + 1] != '\n') {
                result.push_back('\n');
            }
        } else if (byte == '\n') {
            result.push_back('\n');
        } else if (byte < kFirstPrintable || byte == kDelete) {
            result.push_back(' ');
        } else {
            result.push_back(text[index]);
        }
    }
    return result;
}

int widthOf(TTF_Font* font, const std::string& text) {
    int width = 0;
    if (TTF_SizeUTF8(font, text.c_str(), &width, nullptr) != 0) {
        throw std::runtime_error("cannot measure text '" + text + "': " + TTF_GetError());
    }
    return width;
}

// Where the UTF-8 character that starts at `start` ends.
std::size_t endOfCharacter(std::string_view text, std::size_t start) {
    std::size_t end = start + 1;
    while (end < text.size() &&
           (static_cast<unsigned char>(text[end]) & kContinuationMask) == kContinuationByte) {
        ++end;
    }
    return end;
}

// SDL_ttf's own wrapping leaves the spaces at a break on the line and counts them when centring
// it, which moves the visible text off centre. So lines are broken here, without those spaces.
class LineBreaker {
   public:
    LineBreaker(TTF_Font* font, int wrapWidth) : font(font), wrapWidth(wrapWidth) {}

    std::vector<std::string> breakText(std::string_view text) {
        const std::string printable = withoutControls(text);
        std::string_view  rest      = printable;
        for (std::size_t end = rest.find('\n'); end != std::string_view::npos;
             end             = rest.find('\n')) {
            addParagraph(rest.substr(0, end));
            rest.remove_prefix(end + 1);
        }
        addParagraph(rest);

        const auto blank = [](const std::string& line) { return line.empty(); };
        lines.erase(lines.begin(), std::find_if_not(lines.begin(), lines.end(), blank));
        lines.erase(std::find_if_not(lines.rbegin(), lines.rend(), blank).base(), lines.end());
        return std::move(lines);
    }

   private:
    void addParagraph(std::string_view paragraph) {
        std::string line;
        std::size_t wordEnd = 0;
        for (std::size_t start = paragraph.find_first_not_of(' '); start != std::string_view::npos;
             start             = paragraph.find_first_not_of(' ', wordEnd)) {
            const std::size_t gapStart  = wordEnd;
            wordEnd                     = std::min(paragraph.find(' ', start), paragraph.size());
            const std::string_view word = paragraph.substr(start, wordEnd - start);

            if (line.empty()) {
                line = startLine(word);
                continue;
            }
            std::string longer = line;
            longer.append(paragraph.substr(gapStart, start - gapStart)).append(word);
            if (widthOf(font, longer) <= wrapWidth) {
                line = std::move(longer);
            } else {
                lines.push_back(std::move(line));
                line = startLine(word);
            }
        }
        lines.push_back(std::move(line));
    }

    // Splits a word wider than the box between characters and returns its last piece. A single
    // character wider than the box stays whole.
    std::string startLine(std::string_view word) {
        while (widthOf(font, std::string(word)) > wrapWidth) {
            const std::size_t piece = fittingPiece(word);
            if (piece == word.size()) {
                break;
            }
            lines.emplace_back(word.substr(0, piece));
            word.remove_prefix(piece);
        }
        return std::string(word);
    }

    // In bytes: the longest start of `word` that fits the box, but at least one character.
    [[nodiscard]] std::size_t fittingPiece(std::string_view word) const {
        std::size_t fits = endOfCharacter(word, 0);
        while (fits < word.size()) {
            const std::size_t longer = endOfCharacter(word, fits);
            if (widthOf(font, std::string(word.substr(0, longer))) > wrapWidth) {
                break;
            }
            fits = longer;
        }
        return fits;
    }

    TTF_Font*                font;
    int                      wrapWidth;
    std::vector<std::string> lines;
};
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
    const std::vector<std::string> lines = LineBreaker(handle.get(), wrapWidth).breakText(text);

    std::string joined;
    int         widest = 0;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (index > 0) {
            joined.push_back('\n');
        }
        joined.append(lines[index]);
        if (!lines[index].empty()) {
            widest = std::max(widest, widthOf(handle.get(), lines[index]));
        }
    }
    if (widest == 0) {
        return std::nullopt;
    }

    SDL_Surface* pixels =
        TTF_RenderUTF8_Blended_Wrapped(handle.get(), joined.c_str(), kWhite, kNoWrap);
    if (pixels == nullptr) {
        throw std::runtime_error("cannot render text '" + joined + "': " + TTF_GetError());
    }
    return std::optional<Bitmap>(std::in_place, pixels, "text '" + joined + "'",
                                 Bitmap::ScaleMode::Linear);
}

int Font::widestWord(std::string_view text) const {
    int                widest = 0;
    std::istringstream words{withoutControls(text)};
    for (std::string word; words >> word;) {
        widest = std::max(widest, widthOf(handle.get(), word));
    }
    return widest;
}
