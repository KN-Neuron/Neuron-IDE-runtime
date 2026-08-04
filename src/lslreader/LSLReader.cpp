#include "lslreader/LSLReader.hpp"

#include <cmath>
#include <cstddef>
#include <data_structures/EEGData.hpp>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "lsl_cpp.h"

namespace {
constexpr double kResolveTimeout     = 1.0;  // seconds per resolve attempt
constexpr double kPullTimeout        = 0.2;  // seconds; bounds stop-token check latency
constexpr int    kInletBufferSeconds = 360;  // liblsl default inlet buffer length
constexpr int    kSenderChunkLength  = 0;    // 0: the sender decides chunk granularity
// Disables liblsl's silent recovery so a dropped stream surfaces as lsl::lost_error
// and is re-resolved (and re-validated) here instead.
constexpr bool   kRecoverSilently     = false;
constexpr double kSampleRateTolerance = 0.5;  // Hz

// Offsets of the enabled channels within a pulled sample, in the order the
// config declares them. Validates the config first: the reader indexes into raw
// samples with these offsets, so it cannot take the config's invariants on
// trust, and a hand-built DeviceConfig has not been through ConfigParser.
std::vector<std::size_t> validatedChannelOffsets(const DeviceConfig& config) {
    config.validate();

    std::vector<std::size_t> indices;
    indices.reserve(config.channels.size());

    for (const ChannelConfig& channel : config.channels) {
        if (channel.enabled) {
            indices.push_back(static_cast<std::size_t>(channel.index));
        }
    }

    if (indices.empty()) {
        throw std::invalid_argument("LSLReader: config for stream '" + config.lsl.name +
                                    "' enables no channels");
    }
    return indices;
}

// True when the enabled channels are the whole sample in stream order, so it can
// be forwarded without copying.
bool coversWholeSample(const std::vector<std::size_t>& indices, int expectedChannelCount) {
    if (indices.size() != static_cast<std::size_t>(expectedChannelCount)) {
        return false;
    }
    for (std::size_t position = 0; position < indices.size(); ++position) {
        if (indices[position] != position) {
            return false;
        }
    }
    return true;
}

std::vector<double> pickChannels(const std::vector<double>&      sample,
                                 const std::vector<std::size_t>& indices) {
    std::vector<double> selected;
    selected.reserve(indices.size());
    for (const std::size_t index : indices) {
        selected.push_back(sample[index]);
    }
    return selected;
}

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

LSLReader::LSLReader(DeviceConfig deviceConfig)
    : config(std::move(deviceConfig)),
      enabledChannelIndices(validatedChannelOffsets(config)),
      forwardsWholeSample(
          coversWholeSample(enabledChannelIndices, config.lsl.expectedChannelCount)) {}

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
    while (!stopToken.stop_requested()) {
        const std::optional<lsl::stream_info> info = resolveStream(config.lsl, stopToken);
        if (!info.has_value()) {
            return;  // stop requested while waiting for the cap
        }

        try {
            lsl::stream_inlet inlet(*info, kInletBufferSeconds, kSenderChunkLength,
                                    kRecoverSilently);
            inlet.set_postprocessing(lsl::post_clocksync | lsl::post_dejitter |
                                     lsl::post_monotonize);

            std::vector<double> sample;
            while (!stopToken.stop_requested()) {
                const double timestamp = inlet.pull_sample(sample, kPullTimeout);
                if (timestamp == 0.0) {
                    continue;
                }

                if (forwardsWholeSample) {
                    eegQueue->enqueue(EEGData{timestamp, std::move(sample)});
                } else {
                    eegQueue->enqueue(
                        EEGData{timestamp, pickChannels(sample, enabledChannelIndices)});
                }
            }
        } catch (const lsl::lost_error& e) {
            std::cerr << "LSLReader: stream '" << config.lsl.name << "' lost (" << e.what()
                      << "), re-resolving\n";
        }
    }
}
