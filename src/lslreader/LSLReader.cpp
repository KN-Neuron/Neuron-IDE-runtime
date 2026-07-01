#include "lslreader/LSLReader.hpp"

#include <cmath>
#include <data_structures/EEGData.hpp>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "lsl_cpp.h"

namespace {
constexpr double kResolveTimeout      = 1.0;  // seconds per resolve attempt
constexpr double kPullTimeout         = 0.2;  // seconds; bounds stop-token check latency
constexpr int    kInletBufferSeconds  = 360;  // liblsl default inlet buffer length
constexpr double kSampleRateTolerance = 0.5;  // Hz

void validateStream(const lsl::stream_info& info, const LSLConfig& config) {
    if (info.channel_count() != config.expectedChannelCount) {
        throw std::runtime_error("LSLReader: stream '" + config.name + "' exposes " +
                                 std::to_string(info.channel_count()) +
                                 " channels but config expects " +
                                 std::to_string(config.expectedChannelCount));
    }

    const double srate = info.nominal_srate();
    if (std::abs(srate - config.expectedSampleRateHz) > kSampleRateTolerance) {
        throw std::runtime_error("LSLReader: stream '" + config.name + "' reports " +
                                 std::to_string(srate) + " Hz but config expects " +
                                 std::to_string(config.expectedSampleRateHz) + " Hz");
    }
}

std::optional<lsl::stream_info> resolveStream(const LSLConfig&       config,
                                              const std::stop_token& stopToken) {
    while (!stopToken.stop_requested()) {
        std::vector<lsl::stream_info> results =
            lsl::resolve_stream("name", config.name, 1, kResolveTimeout);

        if (!results.empty()) {
            validateStream(results.front(), config);
            return results.front();
        }
    }
    return std::nullopt;
}
}  // namespace

LSLReader::LSLReader(LSLConfig config) : config(std::move(config)) {}

LSLReader::~LSLReader() { stop(); }

void LSLReader::start(std::shared_ptr<moodycamel::ConcurrentQueue<EEGData>> eegQueue) {
    stop();

    this->eegQueue = std::move(eegQueue);
    readerThread   = std::jthread([this](const std::stop_token& stopToken) {
        try {
            readLoop(stopToken);
        } catch (const std::exception& e) {
            std::cerr << "LSLReader: fatal error, stopping acquisition: " << e.what() << "\n";
        }
    });
}

void LSLReader::stop() {
    if (readerThread.joinable()) {
        readerThread.request_stop();
    }
    if (readerThread.joinable()) {
        readerThread.join();
    }
}

void LSLReader::readLoop(const std::stop_token& stopToken) {
    const std::optional<lsl::stream_info> info = resolveStream(config, stopToken);
    if (!info.has_value()) {
        return;
    }

    lsl::stream_inlet inlet(*info, kInletBufferSeconds);
    inlet.set_postprocessing(lsl::post_clocksync | lsl::post_dejitter | lsl::post_monotonize);

    while (!stopToken.stop_requested()) {
        std::vector<double> sample;
        const double        timestamp = inlet.pull_sample(sample, kPullTimeout);
        if (timestamp != 0.0) {
            eegQueue->enqueue(EEGData{timestamp, std::move(sample)});
        }
    }
}
