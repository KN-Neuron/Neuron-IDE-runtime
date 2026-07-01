#include <gtest/gtest.h>

#include <chrono>
#include <config/ConfigParser.hpp>
#include <config/DeviceConfig.hpp>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
namespace fs = std::filesystem;

constexpr int    kExpectedChannelCount      = 8;
constexpr double kExpectedSampleRate        = 250.0;
constexpr double kImpedanceThreshold        = 5.0;
constexpr double kDefaultImpedanceThreshold = 0.0;

constexpr const char* kSampleConfig = R"json({
  "config_version": "1.0",
  "device_name": "OpenBCI Cyton 8ch",
  "montage_standard": "10-20",
  "lsl_stream": {
    "name": "obci_eeg1",
    "type": "EEG",
    "source_id": "cyton-a1b2c3",
    "expected_channel_count": 8,
    "expected_sample_rate_hz": 250
  },
  "reference": { "label": "linked_mastoids", "scheme": "physical" },
  "ground": { "label": "Fpz" },
  "channels": [
    { "index": 0, "label": "Fz", "enabled": true,  "unit": "microvolts" },
    { "index": 1, "label": "Cz", "enabled": true,  "unit": "microvolts" },
    { "index": 2, "label": "Pz", "enabled": true,  "unit": "microvolts" },
    { "index": 3, "label": "Oz", "enabled": true,  "unit": "microvolts" },
    { "index": 4, "label": "P3", "enabled": true,  "unit": "microvolts" },
    { "index": 5, "label": "P4", "enabled": true,  "unit": "microvolts" },
    { "index": 6, "label": "O1", "enabled": true,  "unit": "microvolts" },
    { "index": 7, "label": "O2", "enabled": false, "unit": "microvolts" }
  ],
  "impedance_check": { "supported": true, "threshold_kohm": 5.0 }
})json";

constexpr const char* kMinimalConfig = R"json({
  "config_version": "1.0",
  "device_name": "Dev",
  "montage_standard": "10-20",
  "lsl_stream": {
    "name": "s", "type": "EEG", "source_id": "x",
    "expected_channel_count": 1, "expected_sample_rate_hz": 250
  },
  "channels": [ { "index": 0, "label": "Fz", "enabled": true, "unit": "uV" } ]
})json";

DeviceConfig parseString(const std::string& jsonText) {
    std::istringstream stream(jsonText);
    return ConfigParser::parseStream(stream);
}

// A minimal valid config carrying the given raw config_version value.
std::string withVersion(const std::string& version) {
    return R"json({
  "config_version": ")json" +
           version + R"json(",
  "device_name": "Dev",
  "montage_standard": "10-20",
  "lsl_stream": {
    "name": "s", "type": "EEG", "source_id": "x",
    "expected_channel_count": 1, "expected_sample_rate_hz": 250
  },
  "channels": [ { "index": 0, "label": "Fz", "enabled": true, "unit": "uV" } ]
})json";
}

void expectVersionRejected(const std::string& version) {
    EXPECT_THROW(parseString(withVersion(version)), std::invalid_argument)
        << "accepted malformed config_version: \"" << version << "\"";
}

fs::path writeTempConfig(const std::string& content) {
    const auto suffix = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path path = fs::temp_directory_path() / ("neuronide_config_" + suffix + ".json");
    std::ofstream  out(path);
    out << content;
    return path;
}
}  // namespace

TEST(ConfigParserTest, ParsesTopLevelMetadata) {
    const DeviceConfig config = parseString(kSampleConfig);
    EXPECT_EQ(config.configVersion.major, ConfigParser::kSupportedConfigMajor);
    EXPECT_EQ(config.configVersion.minor, 0);
    EXPECT_EQ(config.deviceName, "OpenBCI Cyton 8ch");
    EXPECT_EQ(config.montageStandard, "10-20");
}

TEST(ConfigParserTest, AcceptsNewerMinorOfSupportedMajor) {
    const DeviceConfig config = parseString(withVersion("1.7"));
    EXPECT_EQ(config.configVersion, (ConfigVersion{ConfigParser::kSupportedConfigMajor, 7}));
}

TEST(ConfigParserTest, UnsupportedMajorVersionThrows) {
    EXPECT_THROW(parseString(withVersion("2.0")), std::invalid_argument);
    EXPECT_THROW(parseString(withVersion("0.9")), std::invalid_argument);
}

TEST(ConfigParserTest, MalformedVersionThrows) {
    for (const char* version : {"", "1", "v1", "1.", ".0", "1.2.3", "1.x", "-1.0", " 1.0"}) {
        expectVersionRejected(version);
    }
}

TEST(ConfigParserTest, UnsupportedVersionIsReportedBeforeOtherFieldErrors) {
    // A future schema would fail on every renamed field; the version must be the
    // error the user sees.
    const std::string jsonText = R"json({ "config_version": "2.0" })json";

    try {
        parseString(jsonText);
        FAIL() << "expected an unsupported-version error";
    } catch (const std::invalid_argument& e) {
        const std::string message = e.what();
        EXPECT_NE(message.find("config_version"), std::string::npos) << message;
        EXPECT_NE(message.find("2.0"), std::string::npos) << message;
    }
}

TEST(ConfigParserTest, ParsesLslStreamFields) {
    const DeviceConfig config = parseString(kSampleConfig);
    EXPECT_EQ(config.lsl.name, "obci_eeg1");
    EXPECT_EQ(config.lsl.type, "EEG");
    EXPECT_EQ(config.lsl.sourceId, "cyton-a1b2c3");
    EXPECT_EQ(config.lsl.expectedChannelCount, kExpectedChannelCount);
    EXPECT_DOUBLE_EQ(config.lsl.expectedSampleRateHz, kExpectedSampleRate);
}

TEST(ConfigParserTest, ParsesAllChannelsIncludingDisabled) {
    const DeviceConfig config = parseString(kSampleConfig);
    ASSERT_EQ(config.channels.size(), static_cast<std::size_t>(kExpectedChannelCount));

    const auto& first = config.channels.front();
    EXPECT_EQ(first.index, 0);
    EXPECT_EQ(first.label, "Fz");
    EXPECT_TRUE(first.enabled);
    EXPECT_EQ(first.unit, "microvolts");

    const auto& last = config.channels.back();
    EXPECT_EQ(last.label, "O2");
    EXPECT_FALSE(last.enabled);
}

TEST(ConfigParserTest, ParsesReferenceGroundAndImpedance) {
    const DeviceConfig config = parseString(kSampleConfig);
    EXPECT_EQ(config.reference.label, "linked_mastoids");
    EXPECT_EQ(config.reference.scheme, "physical");
    EXPECT_EQ(config.ground.label, "Fpz");
    EXPECT_TRUE(config.impedance.supported);
    EXPECT_DOUBLE_EQ(config.impedance.thresholdKohm, kImpedanceThreshold);
}

TEST(ConfigParserTest, ParsesOutputFormat) {
    const std::string jsonText = R"json({
      "config_version": "1.0", "device_name": "Dev", "montage_standard": "10-20",
      "lsl_stream": {
        "name": "s", "type": "EEG", "source_id": "x",
        "expected_channel_count": 1, "expected_sample_rate_hz": 250
      },
      "channels": [ { "index": 0, "label": "Fz", "enabled": true, "unit": "uV" } ],
      "output": { "format": "csv" }
    })json";
    EXPECT_EQ(parseString(jsonText).output.format, "csv");
}

TEST(ConfigParserTest, OutputFormatDefaultsToCsvWhenSectionAbsent) {
    EXPECT_EQ(parseString(kMinimalConfig).output.format, "csv");
}

TEST(ConfigParserTest, MissingOutputFormatFieldThrows) {
    const std::string jsonText = R"json({
      "config_version": "1.0", "device_name": "Dev", "montage_standard": "10-20",
      "lsl_stream": {
        "name": "s", "type": "EEG", "source_id": "x",
        "expected_channel_count": 1, "expected_sample_rate_hz": 250
      },
      "channels": [ { "index": 0, "label": "Fz", "enabled": true, "unit": "uV" } ],
      "output": { }
    })json";
    EXPECT_THROW(parseString(jsonText), std::invalid_argument);
}

TEST(ConfigParserTest, MissingLslStreamThrows) {
    const std::string jsonText = R"json({
      "config_version": "1.0",
      "device_name": "Dev",
      "montage_standard": "10-20",
      "channels": []
    })json";
    EXPECT_THROW(parseString(jsonText), std::invalid_argument);
}

TEST(ConfigParserTest, MissingChannelsThrows) {
    const std::string jsonText = R"json({
      "config_version": "1.0", "device_name": "Dev", "montage_standard": "10-20",
      "lsl_stream": {
        "name": "s", "type": "EEG", "source_id": "x",
        "expected_channel_count": 1, "expected_sample_rate_hz": 250
      }
    })json";
    EXPECT_THROW(parseString(jsonText), std::invalid_argument);
}

// The individual rules live in config_validation_test.cpp; this only pins that
// the parser runs them on what it produced.
TEST(ConfigParserTest, SemanticallyInvalidConfigIsRejected) {
    const std::string jsonText = R"json({
      "config_version": "1.0",
      "device_name": "Dev",
      "montage_standard": "10-20",
      "lsl_stream": {
        "name": "", "type": "EEG", "source_id": "x",
        "expected_channel_count": 2, "expected_sample_rate_hz": 250
      },
      "channels": [ { "index": 0, "label": "Fz", "enabled": true, "unit": "uV" } ]
    })json";
    EXPECT_THROW(parseString(jsonText), std::invalid_argument);
}

TEST(ConfigParserTest, MalformedJsonThrows) {
    EXPECT_THROW(parseString("{ this is not json"), std::runtime_error);
}

TEST(ConfigParserTest, NonObjectRootThrows) {
    EXPECT_THROW(parseString("[1, 2, 3]"), std::invalid_argument);
}

TEST(ConfigParserTest, WrongFieldTypeThrows) {
    EXPECT_THROW(parseString(R"json({ "config_version": "1.0", "device_name": 123 })json"),
                 std::invalid_argument);
}

TEST(ConfigParserTest, ChannelsNotArrayThrows) {
    const std::string jsonText = R"json({
      "config_version": "1.0", "device_name": "Dev", "montage_standard": "10-20",
      "lsl_stream": {
        "name": "s", "type": "EEG", "source_id": "x",
        "expected_channel_count": 1, "expected_sample_rate_hz": 250
      },
      "channels": 5
    })json";
    EXPECT_THROW(parseString(jsonText), std::invalid_argument);
}

TEST(ConfigParserTest, MalformedReferenceThrows) {
    const std::string jsonText = R"json({
      "config_version": "1.0", "device_name": "Dev", "montage_standard": "10-20",
      "lsl_stream": {
        "name": "s", "type": "EEG", "source_id": "x",
        "expected_channel_count": 1, "expected_sample_rate_hz": 250
      },
      "channels": [ { "index": 0, "label": "Fz", "enabled": true, "unit": "uV" } ],
      "reference": { "label": 5, "scheme": "physical" }
    })json";
    EXPECT_THROW(parseString(jsonText), std::invalid_argument);
}

TEST(ConfigParserTest, MalformedGroundThrows) {
    const std::string jsonText = R"json({
      "config_version": "1.0", "device_name": "Dev", "montage_standard": "10-20",
      "lsl_stream": {
        "name": "s", "type": "EEG", "source_id": "x",
        "expected_channel_count": 1, "expected_sample_rate_hz": 250
      },
      "channels": [ { "index": 0, "label": "Fz", "enabled": true, "unit": "uV" } ],
      "ground": { "label": 5 }
    })json";
    EXPECT_THROW(parseString(jsonText), std::invalid_argument);
}

TEST(ConfigParserTest, OptionalSectionsDefaultWhenAbsent) {
    const DeviceConfig config = parseString(kMinimalConfig);
    EXPECT_TRUE(config.reference.label.empty());
    EXPECT_TRUE(config.reference.scheme.empty());
    EXPECT_TRUE(config.ground.label.empty());
    EXPECT_FALSE(config.impedance.supported);
    EXPECT_DOUBLE_EQ(config.impedance.thresholdKohm, kDefaultImpedanceThreshold);
}

TEST(ConfigParserTest, ParsesFromFilePath) {
    const fs::path path = writeTempConfig(kSampleConfig);

    const DeviceConfig config = ConfigParser::parse(path.string());
    EXPECT_EQ(config.deviceName, "OpenBCI Cyton 8ch");
    EXPECT_EQ(config.lsl.name, "obci_eeg1");

    fs::remove(path);
}

TEST(ConfigParserTest, MissingFileThrows) {
    EXPECT_THROW(ConfigParser::parse("/no/such/neuronide_config_file.json"), std::runtime_error);
}

TEST(ConfigParserTest, InvalidFileContentThrows) {
    const fs::path path = writeTempConfig("{ this is not json");

    EXPECT_THROW(ConfigParser::parse(path.string()), std::runtime_error);

    fs::remove(path);
}
