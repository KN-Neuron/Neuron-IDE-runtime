#ifndef EXPERIMENTCONFIG_HPP
#define EXPERIMENTCONFIG_HPP

#include <config/LSLConfig.hpp>
#include <config/OutputConfig.hpp>
#include <string>

struct ReferenceConfig {
    std::string label;
    std::string scheme;
};

struct GroundConfig {
    std::string label;
};

struct ImpedanceConfig {
    bool   supported     = false;
    double thresholdKohm = 0.0;
};

struct ExperimentConfig {
    std::string     configVersion;
    std::string     deviceName;
    std::string     montageStandard;
    LSLConfig       lsl;
    ReferenceConfig reference;
    GroundConfig    ground;
    ImpedanceConfig impedance;
    OutputConfig    output;
};

#endif  // EXPERIMENTCONFIG_HPP
