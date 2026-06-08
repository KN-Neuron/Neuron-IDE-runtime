#ifndef EEGDATA_HPP
#define EEGDATA_HPP

#include <utility>
#include <vector>

struct EEGData {
    std::vector<float> channels;
    double             timestamp = 0.0;

    EEGData() = default;

    EEGData(const std::vector<float>& channelValues, double sampleTimestamp)
        : channels(channelValues), timestamp(sampleTimestamp) {}

    EEGData(std::vector<float>&& channelValues, double sampleTimestamp)
        : channels(std::move(channelValues)), timestamp(sampleTimestamp) {}
};

#endif  // EEGDATA_HPP