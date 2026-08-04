#include "config/ConfigParser.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
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

bool toUnsigned(std::string_view text, int& out) {
    const bool digitsOnly =
        !text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char character) {
            return std::isdigit(character) != 0;
        });
    if (!digitsOnly) {
        return false;
    }

    const char* const first   = text.data();
    const char* const last    = text.data() + text.size();
    const auto [parsed, code] = std::from_chars(first, last, out);
    return code == std::errc{} && parsed == last;
}

// "MAJOR.MINOR" -> ConfigVersion. Anything else ("1", "v1", "1.2.3", "") is
// rejected: a version that cannot be compared is worse than no version at all.
ConfigVersion parseConfigVersion(const std::string& text) {
    const std::size_t dot = text.find('.');
    ConfigVersion     version;

    if (dot == std::string::npos ||
        !toUnsigned(std::string_view(text).substr(0, dot), version.major) ||
        !toUnsigned(std::string_view(text).substr(dot + 1), version.minor)) {
        throw std::invalid_argument(
            "ConfigParser: 'config_version' must be \"MAJOR.MINOR\", got \"" + text + "\"");
    }
    return version;
}

// Checked before anything else is parsed: on an unsupported schema every later
// error would be a misleading missing/renamed field complaint.
ConfigVersion requireSupportedVersion(const json& root) {
    const ConfigVersion version =
        parseConfigVersion(requireField<std::string>(root, "config_version", "config root"));

    if (version.major != ConfigParser::kSupportedConfigMajor) {
        throw std::invalid_argument("ConfigParser: config_version " + version.toString() +
                                    " is not supported by this runtime (supports " +
                                    std::to_string(ConfigParser::kSupportedConfigMajor) + ".x)");
    }
    return version;
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

LSLConfig buildLSLStream(const json& root) {
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

DeviceConfig ConfigParser::parse(const std::string& filePath) {
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

DeviceConfig ConfigParser::parseStream(std::istream& stream) {
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
        DeviceConfig config;
        config.configVersion   = requireSupportedVersion(root);
        config.deviceName      = requireField<std::string>(root, "device_name", "config root");
        config.montageStandard = requireField<std::string>(root, "montage_standard", "config root");

        requireNonEmpty(config.deviceName, "device_name", "config root");

        config.lsl       = buildLSLStream(root);
        config.reference = buildReference(root);
        config.ground    = buildGround(root);
        config.channels  = buildChannels(root, config.lsl.expectedChannelCount);
        config.impedance = buildImpedance(root);

        return config;
    } catch (const json::type_error& e) {
        throw std::invalid_argument(std::string("ConfigParser: field has wrong type: ") + e.what());
    }
}
