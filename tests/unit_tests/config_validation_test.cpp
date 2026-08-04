// Rules that hold for any device config, whatever produced it. These build the
// structs directly - no JSON - so they test the rule and not the parser.

#include <gtest/gtest.h>

#include <config/ChannelConfig.hpp>
#include <config/DeviceConfig.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr int    kChannelCount = 2;
constexpr double kSampleRate   = 250.0;

ChannelConfig makeChannel(int index, const std::string& label) {
    ChannelConfig channel;
    channel.index   = index;
    channel.label   = label;
    channel.enabled = true;
    channel.unit    = "microvolts";
    return channel;
}

DeviceConfig makeValidConfig() {
    DeviceConfig config;
    config.configVersion            = ConfigVersion{1, 0};
    config.deviceName               = "OpenBCI Cyton 8ch";
    config.montageStandard          = "10-20";
    config.lsl.name                 = "obci_eeg1";
    config.lsl.type                 = "EEG";
    config.lsl.sourceId             = "cyton-a1b2c3";
    config.lsl.expectedChannelCount = kChannelCount;
    config.lsl.expectedSampleRateHz = kSampleRate;
    config.channels                 = {makeChannel(0, "Fz"), makeChannel(1, "Cz")};
    return config;
}
}  // namespace

TEST(ConfigValidationTest, AcceptsAValidConfig) { EXPECT_NO_THROW(makeValidConfig().validate()); }

TEST(ConfigValidationTest, AcceptsConfigWithDisabledChannels) {
    DeviceConfig config        = makeValidConfig();
    config.channels[1].enabled = false;

    EXPECT_NO_THROW(config.validate());
}

TEST(ConfigValidationTest, EmptyDeviceNameThrows) {
    DeviceConfig config = makeValidConfig();
    config.deviceName.clear();

    EXPECT_THROW(config.validate(), std::invalid_argument);
}

TEST(ConfigValidationTest, EmptyStreamNameThrows) {
    DeviceConfig config = makeValidConfig();
    config.lsl.name.clear();

    EXPECT_THROW(config.validate(), std::invalid_argument);
}

TEST(ConfigValidationTest, EmptyStreamTypeThrows) {
    DeviceConfig config = makeValidConfig();
    config.lsl.type.clear();

    EXPECT_THROW(config.validate(), std::invalid_argument);
}

TEST(ConfigValidationTest, EmptySourceIdThrows) {
    DeviceConfig config = makeValidConfig();
    config.lsl.sourceId.clear();

    EXPECT_THROW(config.validate(), std::invalid_argument);
}

TEST(ConfigValidationTest, NonPositiveChannelCountThrows) {
    DeviceConfig config             = makeValidConfig();
    config.lsl.expectedChannelCount = 0;
    config.channels.clear();

    EXPECT_THROW(config.validate(), std::invalid_argument);
}

TEST(ConfigValidationTest, NonPositiveSampleRateThrows) {
    DeviceConfig config             = makeValidConfig();
    config.lsl.expectedSampleRateHz = 0.0;

    EXPECT_THROW(config.validate(), std::invalid_argument);
}

TEST(ConfigValidationTest, ChannelCountMismatchThrows) {
    DeviceConfig config = makeValidConfig();
    config.channels.pop_back();

    EXPECT_THROW(config.validate(), std::invalid_argument);
}

TEST(ConfigValidationTest, NegativeChannelIndexThrows) {
    DeviceConfig config      = makeValidConfig();
    config.channels[0].index = -1;

    EXPECT_THROW(config.validate(), std::invalid_argument);
}

TEST(ConfigValidationTest, ChannelIndexOutOfRangeThrows) {
    DeviceConfig config      = makeValidConfig();
    config.channels[1].index = kChannelCount;

    EXPECT_THROW(config.validate(), std::invalid_argument);
}

TEST(ConfigValidationTest, DuplicateChannelIndexThrows) {
    DeviceConfig config      = makeValidConfig();
    config.channels[1].index = config.channels[0].index;

    EXPECT_THROW(config.validate(), std::invalid_argument);
}

TEST(ConfigValidationTest, NegativeImpedanceThresholdThrows) {
    DeviceConfig config            = makeValidConfig();
    config.impedance.supported     = true;
    config.impedance.thresholdKohm = -1.0;

    EXPECT_THROW(config.validate(), std::invalid_argument);
}

TEST(ConfigValidationTest, EmptyOutputFormatThrows) {
    DeviceConfig config = makeValidConfig();
    config.output.format.clear();

    EXPECT_THROW(config.validate(), std::invalid_argument);
}

TEST(ConfigValidationTest, UnknownOutputFormatIsNotAConfigRule) {
    // Which formats exist is DataFormatStrategyFactory's knowledge, not the
    // config layer's; an unknown-but-present format passes validation and is
    // rejected when the strategy is built.
    DeviceConfig config  = makeValidConfig();
    config.output.format = "parquet";

    EXPECT_NO_THROW(config.validate());
}

TEST(ConfigValidationTest, NegativeVersionComponentThrows) {
    DeviceConfig config        = makeValidConfig();
    config.configVersion.minor = -1;

    EXPECT_THROW(config.validate(), std::invalid_argument);
}

TEST(ConfigValidationTest, ChannelValidatesItsOwnInvariantsOnly) {
    // An index beyond the stream's channel count is not something a channel can
    // judge alone - only DeviceConfig knows the expected count.
    const ChannelConfig channel = makeChannel(999, "Fz");

    EXPECT_NO_THROW(channel.validate());
}
