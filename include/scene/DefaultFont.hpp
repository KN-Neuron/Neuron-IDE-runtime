#ifndef DEFAULTFONT_HPP
#define DEFAULTFONT_HPP

#include <span>

// The TrueType file used by text that names no font, compiled into the executable so it is there
// wherever the launcher starts the runtime from. Built from assets/fonts/DejaVuSans.ttf.
std::span<const unsigned char> defaultFontBytes();

#endif  // DEFAULTFONT_HPP
