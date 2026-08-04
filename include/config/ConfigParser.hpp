#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include <config/DeviceConfig.hpp>
#include <istream>
#include <string>

class ConfigParser {
   public:
    // Schema major version this runtime understands. Configs declaring another
    // major are rejected; any minor of this major is accepted (see README §5).
    static constexpr int kSupportedConfigMajor = 1;

    ConfigParser() = default;

    static DeviceConfig parse(const std::string& filePath);
    static DeviceConfig parseStream(std::istream& stream);
};

#endif  // CONFIGPARSER_HPP
