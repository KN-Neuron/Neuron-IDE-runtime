#ifndef MARKER_HPP
#define MARKER_HPP

#include <string>

struct Marker {
    std::string eventName;
    double      timestamp = 0.0;
};

#endif  // MARKER_HPP