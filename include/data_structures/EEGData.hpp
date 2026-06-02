#ifndef EEGDATA_HPP
#define EEGDATA_HPP

#include <vector>

struct EEGData {
    double              timestamp;
    std::vector<double> channelValues;
};

#endif  // EEGDATA_HPP