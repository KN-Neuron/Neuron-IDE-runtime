#ifndef UTF8PATH_HPP
#define UTF8PATH_HPP

#include <filesystem>
#include <string>
#include <string_view>

// On Windows, std::filesystem converts plain std::string with the system code page (e.g. cp1250),
// while protobuf strings and SDL file names are UTF-8, so "twarz_ż.png" would arrive garbled.
// These convert through UTF-8 explicitly. On Linux both are a plain byte copy.
namespace utf8 {

inline std::filesystem::path toPath(std::string_view text) {
    return {std::u8string(text.begin(), text.end())};
}

inline std::string toString(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return {text.begin(), text.end()};
}

}  // namespace utf8

#endif  // UTF8PATH_HPP
