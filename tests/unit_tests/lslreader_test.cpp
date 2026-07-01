#include <concurrentqueue.h>
#include <gtest/gtest.h>

#include <chrono>
#include <config/LSLConfig.hpp>
#include <cstddef>
#include <data_structures/EEGData.hpp>
#include <iostream>
#include <lslreader/LSLReader.hpp>
#include <memory>
#include <sstream>
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
constexpr auto   kPushInterval         = std::chrono::milliseconds(10);
constexpr auto   kDrainWait            = std::chrono::milliseconds(300);
constexpr auto   kSubscribePoll        = std::chrono::milliseconds(20);

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

LSLConfig makeConfig() {
    LSLConfig config;
    config.name                 = "neuronide_test_stream";
    config.type                 = "EEG";
    config.sourceId             = "neuronide-test-src";
    config.expectedChannelCount = kChannelCount;
    config.expectedSampleRateHz = kSampleRate;
    return config;
}

std::vector<double> makeSample() {
    std::vector<double> sample(kChannelCount);
    for (int i = 0; i < kChannelCount; ++i) {
        sample[i] = static_cast<double>(i + 1);
    }
    return sample;
}

lsl::stream_outlet makeOutletWithShape(const LSLConfig& config, int channelCount,
                                       double sampleRate) {
    const lsl::stream_info info(config.name, config.type, channelCount, sampleRate,
                                lsl::cf_double64, config.sourceId);
    return lsl::stream_outlet(info);
}

lsl::stream_outlet makeOutlet(const LSLConfig& config) {
    return makeOutletWithShape(config, config.expectedChannelCount, config.expectedSampleRateHz);
}

// Waits (bounded) for the reader's inlet to subscribe to the outlet.
bool waitForConsumer(lsl::stream_outlet& outlet) {
    const auto deadline = std::chrono::steady_clock::now() + kSubscribeWait;
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

std::string runAndCaptureDiagnostics(const LSLConfig& config, int channelCount, double sampleRate) {
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
}  // namespace

TEST(LSLReaderTest, ReadsSamplesFromStreamIntoQueue) {
    const LSLConfig    config = makeConfig();
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

TEST(LSLReaderTest, RejectsStreamWithMismatchedChannelCount) {
    LSLConfig config = makeConfig();
    config.name      = "neuronide_test_chan_mismatch";
    config.sourceId  = "neuronide-test-chan";

    const std::string log = runAndCaptureDiagnostics(config, kMismatchedChannels, kSampleRate);
    EXPECT_NE(log.find("channels"), std::string::npos)
        << "expected a channel-count rejection, got: " << log;
}

TEST(LSLReaderTest, RejectsStreamWithMismatchedSampleRate) {
    LSLConfig config = makeConfig();
    config.name      = "neuronide_test_rate_mismatch";
    config.sourceId  = "neuronide-test-rate";

    const std::string log = runAndCaptureDiagnostics(config, kChannelCount, kMismatchedSampleRate);
    EXPECT_NE(log.find("Hz"), std::string::npos)
        << "expected a sample-rate rejection, got: " << log;
}

TEST(LSLReaderTest, StopBeforeStreamResolvedExitsCleanly) {
    LSLConfig config = makeConfig();
    config.name      = "neuronide_test_absent_stream";
    config.sourceId  = "neuronide-test-absent";

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
