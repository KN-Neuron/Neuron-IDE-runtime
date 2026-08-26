// Semantic rules for the device config types. These are deliberately separate
// from ConfigParser: none of them are about JSON, so they hold for any producer
// of a config, not just the file parser.
//
// Field names in the messages are the JSON keys, since that is what a user edits
// (the structs mirror config.json 1:1 - see README §5).

#include <config/ChannelConfig.hpp>
#include <config/ConfigVersion.hpp>
#include <config/DeviceConfig.hpp>
#include <config/LSLConfig.hpp>
#include <config/OutputConfig.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>

namespace {
void requireNonEmpty(const std::string& value, const char* field, std::string_view owner) {
    if (value.empty()) {
        throw std::invalid_argument(std::string(owner) + ": '" + std::string(field) +
                                    "' must not be empty");
    }
}
}  // namespace

void ConfigVersion::validate() const {
    if (major < 0 || minor < 0) {
        throw std::invalid_argument("ConfigVersion: version components must not be negative, got " +
                                    toString());
    }
}

void ChannelConfig::validate() const {
    if (index < 0) {
        throw std::invalid_argument("ChannelConfig: channel '" + label + "' has negative index " +
                                    std::to_string(index));
    }
}

void LSLConfig::validate() const {
    requireNonEmpty(name, "name", "LSLConfig");
    requireNonEmpty(type, "type", "LSLConfig");
    requireNonEmpty(sourceId, "source_id", "LSLConfig");

    if (expectedChannelCount <= 0) {
        throw std::invalid_argument("LSLConfig: 'expected_channel_count' must be positive, got " +
                                    std::to_string(expectedChannelCount));
    }
    if (expectedSampleRateHz <= 0.0) {
        throw std::invalid_argument("LSLConfig: 'expected_sample_rate_hz' must be positive, got " +
                                    std::to_string(expectedSampleRateHz));
    }
}

void ImpedanceConfig::validate() const {
    if (thresholdKohm < 0.0) {
        throw std::invalid_argument("ImpedanceConfig: 'threshold_kohm' must not be negative, got " +
                                    std::to_string(thresholdKohm));
    }
}

void OutputConfig::validate() const { requireNonEmpty(format, "format", "OutputConfig"); }

void DeviceConfig::validate() const {
    configVersion.validate();
    requireNonEmpty(deviceName, "device_name", "DeviceConfig");
    lsl.validate();
    impedance.validate();
    output.validate();

    if (static_cast<int>(channels.size()) != lsl.expectedChannelCount) {
        throw std::invalid_argument("DeviceConfig: channel count mismatch: 'channels' has " +
                                    std::to_string(channels.size()) +
                                    " entries but 'expected_channel_count' is " +
                                    std::to_string(lsl.expectedChannelCount));
    }

    std::unordered_set<int> seenIndices;
    for (const ChannelConfig& channel : channels) {
        channel.validate();

        if (channel.index >= lsl.expectedChannelCount) {
            throw std::invalid_argument("DeviceConfig: channel index out of range: " +
                                        std::to_string(channel.index));
        }
        if (!seenIndices.insert(channel.index).second) {
            throw std::invalid_argument("DeviceConfig: duplicate channel index: " +
                                        std::to_string(channel.index));
        }
    }
}
