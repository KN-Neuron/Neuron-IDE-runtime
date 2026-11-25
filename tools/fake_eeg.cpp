// Streams made-up EEG on LSL, shaped like the stream a device config.json describes: its name,
// type, source_id, channel count and sample rate, with each channel's label and unit. The runtime
// then runs with that config as if the device were there. Runs until it is killed.
//
//     fake_eeg <config.json>
//
// Every channel carries a 10 Hz rhythm under noise, so a recording looks roughly like EEG.

#include <lsl_cpp.h>

#include <chrono>
#include <cmath>
#include <config/ConfigParser.hpp>
#include <config/DeviceConfig.hpp>
#include <cstddef>
#include <exception>
#include <logging/Logging.hpp>
#include <numbers>
#include <random>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace {
constexpr double kRhythmHz     = 10.0;
constexpr double kRhythmMicroV = 10.0;
constexpr double kNoiseMicroV  = 5.0;

quill::Logger* logger() { return logging::logger(logging::Subsystem::App); }

lsl::stream_info describe(const DeviceConfig& config) {
    const LSLConfig& stream = config.lsl;
    lsl::stream_info info(stream.name, stream.type, stream.expectedChannelCount,
                          stream.expectedSampleRateHz, lsl::cf_float32, stream.sourceId);

    // In stream order, which the config does not have to follow.
    std::vector<const ChannelConfig*> byIndex(config.channels.size());
    for (const ChannelConfig& channel : config.channels) {
        byIndex.at(static_cast<std::size_t>(channel.index)) = &channel;
    }
    info.desc().append_child_value("manufacturer", "NeuronIDE fake_eeg");
    lsl::xml_element channels = info.desc().append_child("channels");
    for (const ChannelConfig* channel : byIndex) {
        channels.append_child("channel")
            .append_child_value("label", channel->label)
            .append_child_value("unit", channel->unit);
    }
    return info;
}

// Pushes one sample per period, timed against a deadline so the rate does not drift.
[[noreturn]] void stream(lsl::stream_outlet& outlet, const LSLConfig& shape) {
    const double                     sampleRateHz = shape.expectedSampleRateHz;
    std::mt19937                     random(std::random_device{}());
    std::normal_distribution<double> noise(0.0, kNoiseMicroV);

    const auto period = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::duration<double>(1.0 / sampleRateHz));
    std::vector<float> sample(static_cast<std::size_t>(shape.expectedChannelCount));
    auto               deadline = std::chrono::steady_clock::now();

    for (long long sampleNumber = 0;; ++sampleNumber) {
        const double phase =
            2.0 * std::numbers::pi * kRhythmHz * static_cast<double>(sampleNumber) / sampleRateHz;
        for (std::size_t channel = 0; channel < sample.size(); ++channel) {
            const double shift = static_cast<double>(channel) * std::numbers::pi / 4.0;
            sample[channel] =
                static_cast<float>((kRhythmMicroV * std::sin(phase + shift)) + noise(random));
        }
        outlet.push_sample(sample);

        deadline += period;
        std::this_thread::sleep_until(deadline);
    }
}

int run(std::span<char* const> args) {
    const logging::Session loggingSession;

    if (args.size() != 2) {
        LOG_ERROR(logger(), "Usage: fake_eeg <config.json>");
        return 1;
    }

    try {
        const DeviceConfig config = ConfigParser::parse(args[1]);
        lsl::stream_outlet outlet(describe(config));
        LOG_INFO(logger(), "Streaming fake EEG as '{}' ({} channels at {} Hz) until killed",
                 config.lsl.name, config.lsl.expectedChannelCount, config.lsl.expectedSampleRateHz);
        stream(outlet, config.lsl);
    } catch (const std::exception& e) {
        LOG_CRITICAL(logger(), "fake_eeg stopped: {}", e.what());
        return 1;
    }
}
}  // namespace

int main(int argc, char* argv[]) {
    try {
        return run(std::span<char* const>(argv, static_cast<std::size_t>(argc)));
    } catch (...) {
        return 1;
    }
}
