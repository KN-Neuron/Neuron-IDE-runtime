#ifndef KEYPRESS_HPP
#define KEYPRESS_HPP

#include <SDL2/SDL_scancode.h>

// A key going down. `key` is the physical key, so a response key stays in the same place whatever
// keyboard layout the lab PC uses. `timestamp` is on the LSL clock, like a Marker's.
struct KeyPress {
    SDL_Scancode key       = SDL_SCANCODE_UNKNOWN;
    double       timestamp = 0.0;
};

#endif  // KEYPRESS_HPP
