#include <concurrentqueue.h>
#include <gtest/gtest.h>

#include <chrono>
#include <config/ChannelConfig.hpp>
#include <config/DeviceConfig.hpp>
#include <cstddef>
#include <data_structures/EEGData.hpp>
#include <iostream>
#include <lslreader/LSLReader.hpp>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <thread>
#include <vector>

#include "lsl_cpp.h"

namespace {
constexpr int    kChannelCount         = 4;
constexpr double kSampleRate           = 250.0;
constexpr int    kSamplesToPush        = 20;
constexpr int    kMismatchedChannels   = kChannelCount + 1;
constexpr double kMismatchedSampleRate = 100.0;
constexpr auto   kSubscribeWait        = std::chrono::seconds(3);
constexpr auto   kRecoveryWait         = std::chrono::seconds(10);
constexpr auto   kPushInterval         = std::chrono::milliseconds(10);
constexpr auto   kDrainWait            = std::chrono::milliseconds(300);
constexpr auto   kSubscribePoll        = std::chrono::milliseconds(20);
constexpr double kRecoveryValue        = 42.0;

constexpr auto kValidationWait = std::chrono::milliseconds(1500);

class ScopedStreamRedirect {
   public:
    ScopedStreamRedirect(std::ostream& stream, std::streambuf* buffer)
        : stream(stream), previous(stream.rdbuf(buffer)) {}
    ~ScopedStreamRedirect() { stream.rdbuf(previous); }

    ScopedStreamRedirect(const ScopedStreamRedirect&)            = delete;
    ScopedStreamRedirect& operator=(const ScopedStreamRedirect&) = delete;
    ScopedStreamRedirect(ScopedStreamRedirect&&)                 = delete;
    ScopedStreamRedirect& operator=(ScopedStreamRedirect&&)      = delete;

   private:
    std::ostream&   stream;
    std::streambuf* previous;
};

// All channels enabled, declared in stream order.
std::vector<ChannelConfig> makeChannels() {
    std::vector<ChannelConfig> channels;
    channels.reserve(kChannelCount);
    for (int i = 0; i < kChannelCount; ++i) {
        ChannelConfig channel;
        channel.index   = i;
        channel.label   = "ch" + std::to_string(i);
        channel.enabled = true;
        channel.unit    = "microvolts";
        channels.push_back(channel);
    }
    return channels;
}

DeviceConfig makeConfig() {
    DeviceConfig config;
    config.configVersion            = ConfigVersion{1, 0};
    config.deviceName               = "NeuronIDE test device";
    config.lsl.name                 = "neuronide_test_stream";
    config.lsl.type                 = "EEG";
    config.lsl.sourceId             = "neuronide-test-src";
    config.lsl.expectedChannelCount = kChannelCount;
    config.lsl.expectedSampleRateHz = kSampleRate;
    config.channels                 = makeChannels();
    return config;
}

std::vector<double> makeSample() {
    std::vector<double> sample(kChannelCount);
    for (int i = 0; i < kChannelCount; ++i) {
        sample[i] = static_cast<double>(i + 1);
    }
    return sample;
}

lsl::stream_outlet makeOutletWithShape(const DeviceConfig& config, int channelCount,
                                       double sampleRate) {
    const lsl::stream_info info(config.lsl.name, config.lsl.type, channelCount, sampleRate,
                                lsl::cf_double64, config.lsl.sourceId);
    return lsl::stream_outlet(info);
}

lsl::stream_outlet makeOutlet(const DeviceConfig& config) {
    return makeOutletWithShape(config, config.lsl.expectedChannelCount,
                               config.lsl.expectedSampleRateHz);
}

// Waits (bounded) for the reader's inlet to subscribe to the outlet.
bool waitForConsumer(lsl::stream_outlet&                 outlet,
                     std::chrono::steady_clock::duration timeout = kSubscribeWait) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!outlet.have_consumers()) {
        if (std::chrono::steady_clock::now() >= deadline) {
            return false;
        }
        std::this_thread::sleep_for(kSubscribePoll);
    }
    return true;
}

void pushSamples(lsl::stream_outlet& outlet, const std::vector<double>& sample, int count) {
    for (int i = 0; i < count; ++i) {
        outlet.push_sample(sample);
        std::this_thread::sleep_for(kPushInterval);
    }
}

std::string runAndCaptureDiagnostics(const DeviceConfig& config, int channelCount,
                                     double sampleRate) {
    lsl::stream_outlet outlet   = makeOutletWithShape(config, channelCount, sampleRate);
    auto               eegQueue = std::make_shared<moodycamel::ConcurrentQueue<EEGData>>();
    LSLReader          reader(config);

    std::ostringstream captured;
    {
        const ScopedStreamRedirect redirect(std::cerr, captured.rdbuf());
        reader.start(eegQueue);
        std::this_thread::sleep_for(kValidationWait);
        reader.stop();
    }

    EEGData received;
    EXPECT_FALSE(eegQueue->try_dequeue(received)) << "a rejected stream must yield no samples";
    return captured.str();
}

// Keeps pushing a marked sample until one of them comes back through the queue,
// so the check does not depend on when exactly the inlet reconnects. Samples
// from before the drop carry other values and are discarded.
bool pushUntilReceived(lsl::stream_outlet& outlet, moodycamel::ConcurrentQueue<EEGData>& eegQueue,
                       double value, std::chrono::steady_clock::duration timeout) {
    const std::vector<double> sample(kChannelCount, value);
    const auto                deadline = std::chrono::steady_clock::now() + timeout;

    while (std::chrono::steady_clock::now() < deadline) {
        outlet.push_sample(sample);
        std::this_thread::sleep_for(kPushInterval);

        EEGData received;
        while (eegQueue.try_dequeue(received)) {
            if (!received.channels.empty() && received.channels.front() == value) {
                return true;
            }
        }
    }
    return false;
}
}  // namespace

TEST(LSLReaderTest, ReadsSamplesFromStreamIntoQueue) {
    const DeviceConfig config = makeConfig();
    lsl::stream_outlet outlet = makeOutlet(config);

    auto eegQueue = std::make_shared<moodycamel::ConcurrentQueue<EEGData>>();

    LSLReader reader(config);
    reader.start(eegQueue);

    ASSERT_TRUE(waitForConsumer(outlet)) << "LSLReader did not subscribe (needs loopback)";

    const std::vector<double> sample = makeSample();
    pushSamples(outlet, sample, kSamplesToPush);

    std::this_thread::sleep_for(kDrainWait);
    reader.stop();

    EEGData received;
    ASSERT_TRUE(eegQueue->try_dequeue(received));
    EXPECT_EQ(received.channels.size(), static_cast<std::size_t>(kChannelCount));
    EXPECT_DOUBLE_EQ(received.channels.front(), sample.front());
    EXPECT_DOUBLE_EQ(received.channels.back(), sample.back());
    EXPECT_NE(received.timestamp, 0.0);
}

TEST(LSLReaderTest, ForwardsOnlyChannelsEnabledInConfig) {
    DeviceConfig config        = makeConfig();
    config.lsl.name            = "neuronide_test_channel_filter";
    config.lsl.sourceId        = "neuronide-test-filter";
    config.channels[1].enabled = false;
    config.channels[2].enabled = false;

    lsl::stream_outlet outlet   = makeOutlet(config);
    auto               eegQueue = std::make_shared<moodycamel::ConcurrentQueue<EEGData>>();

    LSLReader reader(config);
    reader.start(eegQueue);

    ASSERT_TRUE(waitForConsumer(outlet)) << "LSLReader did not subscribe (needs loopback)";

    const std::vector<double> sample = makeSample();  // { 1, 2, 3, 4 }
    pushSamples(outlet, sample, kSamplesToPush);

    std::this_thread::sleep_for(kDrainWait);
    reader.stop();

    EEGData received;
    ASSERT_TRUE(eegQueue->try_dequeue(received));
    ASSERT_EQ(received.channels.size(), 2U) << "disabled channels must not be forwarded";
    EXPECT_DOUBLE_EQ(received.channels[0], sample[0]);
    EXPECT_DOUBLE_EQ(received.channels[1], sample[3]);
}

TEST(LSLReaderTest, ConfigWithoutEnabledChannelsThrows) {
    DeviceConfig config = makeConfig();
    for (ChannelConfig& channel : config.channels) {
        channel.enabled = false;
    }

    EXPECT_THROW({ const LSLReader reader(config); }, std::invalid_argument);
}

TEST(LSLReaderTest, ChannelIndexOutsideExpectedCountThrows) {
    DeviceConfig config          = makeConfig();
    config.channels.back().index = kChannelCount;

    EXPECT_THROW({ const LSLReader reader(config); }, std::invalid_argument);
}

TEST(LSLReaderTest, ReResolvesStreamAfterItIsLost) {
    DeviceConfig config = makeConfig();
    config.lsl.name     = "neuronide_test_lost_stream";
    config.lsl.sourceId = "neuronide-test-lost";

    auto      eegQueue = std::make_shared<moodycamel::ConcurrentQueue<EEGData>>();
    LSLReader reader(config);

    std::ostringstream captured;
    {
        const ScopedStreamRedirect redirect(std::cerr, captured.rdbuf());

        {
            lsl::stream_outlet outlet = makeOutlet(config);
            reader.start(eegQueue);
            ASSERT_TRUE(waitForConsumer(outlet, kRecoveryWait))
                << "LSLReader did not subscribe (needs loopback)";
            pushSamples(outlet, makeSample(), kSamplesToPush);
        }  // outlet gone: the inlet's pull must raise lsl::lost_error

        lsl::stream_outlet revived = makeOutlet(config);
        ASSERT_TRUE(waitForConsumer(revived, kRecoveryWait))
            << "LSLReader did not re-resolve the stream after it was lost";

        EXPECT_TRUE(pushUntilReceived(revived, *eegQueue, kRecoveryValue, kRecoveryWait))
            << "no samples arrived after the stream came back";

        reader.stop();
    }

    const std::string log = captured.str();
    EXPECT_NE(log.find("lost"), std::string::npos)
        << "the drop should surface as lsl::lost_error and be re-resolved, got: " << log;
    EXPECT_EQ(log.find("fatal error"), std::string::npos)
        << "a dropped stream must not stop acquisition, got: " << log;
}

TEST(LSLReaderTest, RejectsStreamWithMismatchedChannelCount) {
    DeviceConfig config = makeConfig();
    config.lsl.name     = "neuronide_test_chan_mismatch";
    config.lsl.sourceId = "neuronide-test-chan";

    const std::string log = runAndCaptureDiagnostics(config, kMismatchedChannels, kSampleRate);
    EXPECT_NE(log.find("channels"), std::string::npos)
        << "expected a channel-count rejection, got: " << log;
}

TEST(LSLReaderTest, RejectsStreamWithMismatchedSampleRate) {
    DeviceConfig config = makeConfig();
    config.lsl.name     = "neuronide_test_rate_mismatch";
    config.lsl.sourceId = "neuronide-test-rate";

    const std::string log = runAndCaptureDiagnostics(config, kChannelCount, kMismatchedSampleRate);
    EXPECT_NE(log.find("Hz"), std::string::npos)
        << "expected a sample-rate rejection, got: " << log;
}

TEST(LSLReaderTest, StopBeforeStreamResolvedExitsCleanly) {
    DeviceConfig config = makeConfig();
    config.lsl.name     = "neuronide_test_absent_stream";
    config.lsl.sourceId = "neuronide-test-absent";

    auto      eegQueue = std::make_shared<moodycamel::ConcurrentQueue<EEGData>>();
    LSLReader reader(config);

    reader.start(eegQueue);
    reader.stop();

    EEGData received;
    EXPECT_FALSE(eegQueue->try_dequeue(received));
}

TEST(LSLReaderTest, DestroyingUnstartedReaderIsSafe) {
    EXPECT_NO_THROW({ const LSLReader reader(makeConfig()); });
}
