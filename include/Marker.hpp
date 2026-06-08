#ifndef MARKER_HPP
#define MARKER_HPP

#include <string>
#include <utility>

struct Marker {
    std::string eventName;
    double      timestamp = 0.0;

    Marker() = default;

    Marker(const std::string& name, double markerTimestamp)
        : eventName(name), timestamp(markerTimestamp) {}

    Marker(std::string&& name, double markerTimestamp)
        : eventName(std::move(name)), timestamp(markerTimestamp) {}
};

#endif  // MARKER_HPP