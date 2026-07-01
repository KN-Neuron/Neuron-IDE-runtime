#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include <config/ExperimentConfig.hpp>
#include <istream>
#include <string>

class ConfigParser {
   public:
    ConfigParser() = default;

    static ExperimentConfig parse(const std::string& filePath);
    static ExperimentConfig parseStream(std::istream& stream);
};

#endif  // CONFIGPARSER_HPP
