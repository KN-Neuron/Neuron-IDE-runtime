#ifndef DEVICECONFIG_HPP
#define DEVICECONFIG_HPP

#include <config/ChannelConfig.hpp>
#include <config/ConfigVersion.hpp>
#include <config/LSLConfig.hpp>
#include <config/OutputConfig.hpp>
#include <string>
#include <vector>

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

    // Throws std::invalid_argument on a negative threshold.
    void validate() const;
};

// Describes the acquisition hardware (the cap and its LSL stream), not the
// experiment - experiment content lives in the protobuf file. Mirrors
// `config.json` 1:1: every JSON key maps onto exactly one field below.
struct DeviceConfig {
    ConfigVersion              configVersion;    // config_version
    std::string                deviceName;       // device_name
    std::string                montageStandard;  // montage_standard
    LSLConfig                  lsl;              // lsl_stream
    ReferenceConfig            reference;        // reference
    GroundConfig               ground;           // ground
    std::vector<ChannelConfig> channels;         // channels
    ImpedanceConfig            impedance;        // impedance_check
    OutputConfig               output;           // output

    // Checks every rule a device config must satisfy, including the cross-field
    // ones no single member can check (channel count matching the stream, unique
    // in-range indices), and throws std::invalid_argument on the first failure.
    //
    // A DeviceConfig returned by ConfigParser has already passed this. One
    // assembled by hand has not - call it before handing the config to a
    // consumer such as LSLReader.
    void validate() const;
};

#endif  // DEVICECONFIG_HPP
