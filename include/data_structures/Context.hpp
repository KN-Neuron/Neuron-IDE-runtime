#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include <span>
#include <string>
#include <vector>

#include "data_structures/KeyPress.hpp"
#include "data_structures/Marker.hpp"

struct Context {
    double timestamp = 0.0;
    // Marker names; the Renderer stamps them with the flip that presents this frame.
    std::vector<std::string>* markers = nullptr;
    // Markers that already carry their time, such as a response stamped with its key press.
    std::vector<Marker>* timedMarkers = nullptr;
    // Keys pressed since the previous frame, oldest first.
    // The `{}` keeps GCC's -Wmissing-field-initializers quiet about Context{a, b}.
    // NOLINTNEXTLINE(readability-redundant-member-init)
    std::span<const KeyPress> keyPresses{};
};

#endif  // CONTEXT_HPP
