#include "config/ConfigParser.hpp"

#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
using nlohmann::json;

const json* requireMember(const json& obj, const char* key, std::string_view ctx) {
    const auto member = obj.find(key);
    if (member == obj.end()) {
        throw std::invalid_argument("ConfigParser: missing '" + std::string(key) + "' in " +
                                    std::string(ctx));
    }
    return &(*member);
}

template <typename T>
T requireField(const json& obj, const char* key, std::string_view ctx) {
    return requireMember(obj, key, ctx)->get<T>();
}

void requireNonEmpty(const std::string& value, const char* field, std::string_view ctx) {
    if (value.empty()) {
        throw std::invalid_argument("ConfigParser: '" + std::string(field) +
                                    "' must not be empty in " + std::string(ctx));
    }
}


std::vector<ChannelConfig> buildChannels(const json& root, int expectedCount) {
    const json& channelsJson = *requireMember(root, "channels", "config root");
    if (!channelsJson.is_array()) {
        throw std::invalid_argument("ConfigParser: 'channels' must be an array");
    }
    if (static_cast<int>(channelsJson.size()) != expectedCount) {
        throw std::invalid_argument("ConfigParser: channel count mismatch: 'channels' has " +
                                    std::to_string(channelsJson.size()) +
                                    " entries but expected_channel_count is " +
                                    std::to_string(expectedCount));
    }

    std::vector<ChannelConfig> channels;
    channels.reserve(channelsJson.size());
    std::unordered_set<int> seenIndices;

    for (const auto& entry : channelsJson) {
        ChannelConfig channel;
        channel.index   = requireField<int>(entry, "index", "channel");
        channel.label   = requireField<std::string>(entry, "label", "channel");
        channel.enabled = requireField<bool>(entry, "enabled", "channel");
        channel.unit    = requireField<std::string>(entry, "unit", "channel");

        if (channel.index < 0 || channel.index >= expectedCount) {
            throw std::invalid_argument("ConfigParser: channel index out of range: " +
                                        std::to_string(channel.index));
        }
        if (!seenIndices.insert(channel.index).second) {
            throw std::invalid_argument("ConfigParser: duplicate channel index: " +
                                        std::to_string(channel.index));
        }

        channels.push_back(std::move(channel));
    }

    return channels;
}

LSLConfig buildLSLConfig(const json& root) {
    const json& streamJson = *requireMember(root, "lsl_stream", "config root");

    LSLConfig lsl;
    lsl.name     = requireField<std::string>(streamJson, "name", "lsl_stream");
    lsl.type     = requireField<std::string>(streamJson, "type", "lsl_stream");
    lsl.sourceId = requireField<std::string>(streamJson, "source_id", "lsl_stream");
    lsl.expectedChannelCount =
        requireField<int>(streamJson, "expected_channel_count", "lsl_stream");
    lsl.expectedSampleRateHz =
        requireField<double>(streamJson, "expected_sample_rate_hz", "lsl_stream");

    requireNonEmpty(lsl.name, "name", "lsl_stream");
    requireNonEmpty(lsl.type, "type", "lsl_stream");
    requireNonEmpty(lsl.sourceId, "source_id", "lsl_stream");
    if (lsl.expectedChannelCount <= 0) {
        throw std::invalid_argument("ConfigParser: 'expected_channel_count' must be positive");
    }
    if (lsl.expectedSampleRateHz <= 0.0) {
        throw std::invalid_argument("ConfigParser: 'expected_sample_rate_hz' must be positive");
    }

    lsl.channels = buildChannels(root, lsl.expectedChannelCount);
    return lsl;
}

ReferenceConfig buildReference(const json& root) {
    ReferenceConfig reference;
    if (root.contains("reference")) {
        const json& ref  = root.at("reference");
        reference.label  = requireField<std::string>(ref, "label", "reference");
        reference.scheme = requireField<std::string>(ref, "scheme", "reference");
    }
    return reference;
}

GroundConfig buildGround(const json& root) {
    GroundConfig ground;
    if (root.contains("ground")) {
        ground.label = requireField<std::string>(root.at("ground"), "label", "ground");
    }
    return ground;
}

ImpedanceConfig buildImpedance(const json& root) {
    ImpedanceConfig impedance;
    if (root.contains("impedance_check")) {
        const json& imp         = root.at("impedance_check");
        impedance.supported     = requireField<bool>(imp, "supported", "impedance_check");
        impedance.thresholdKohm = requireField<double>(imp, "threshold_kohm", "impedance_check");
    }
    return impedance;
}
}  // namespace

ExperimentConfig ConfigParser::parse(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("ConfigParser: cannot open file: " + filePath);
    }

    try {
        return parseStream(file);
    } catch (const std::exception& e) {
        throw std::runtime_error("ConfigParser: failed to parse file " + filePath + " - " +
                                 e.what());
    }
}

ExperimentConfig ConfigParser::parseStream(std::istream& stream) {
    json root;
    try {
        root = json::parse(stream);
    } catch (const json::parse_error& e) {
        throw std::runtime_error(std::string("ConfigParser: invalid JSON: ") + e.what());
    }

    if (!root.is_object()) {
        throw std::invalid_argument("ConfigParser: config root must be a JSON object");
    }

    try {
        ExperimentConfig config;
        config.configVersion   = requireField<std::string>(root, "config_version", "config root");
        config.deviceName      = requireField<std::string>(root, "device_name", "config root");
        config.montageStandard = requireField<std::string>(root, "montage_standard", "config root");

        requireNonEmpty(config.deviceName, "device_name", "config root");

        config.lsl       = buildLSLConfig(root);
        config.reference = buildReference(root);
        config.ground    = buildGround(root);
        config.impedance = buildImpedance(root);

        return config;
    } catch (const json::type_error& e) {
        throw std::invalid_argument(std::string("ConfigParser: field has wrong type: ") + e.what());
    }
}
