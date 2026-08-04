#ifndef CONFIGVERSION_HPP
#define CONFIGVERSION_HPP

#include <string>

// Schema version of a device config file, written as "MAJOR.MINOR".
// MAJOR changes are breaking (fields moved, renamed or removed) and are rejected
// by a runtime built for another major; MINOR changes are additive and
// backward-compatible, so any minor of a supported major is accepted.
struct ConfigVersion {
    int major = 0;
    int minor = 0;

    bool operator==(const ConfigVersion&) const = default;

    // Throws std::invalid_argument on a version that cannot be compared.
    // Whether a valid version is *supported* is ConfigParser's decision.
    void validate() const;

    [[nodiscard]] std::string toString() const {
        return std::to_string(major) + "." + std::to_string(minor);
    }
};

#endif  // CONFIGVERSION_HPP
