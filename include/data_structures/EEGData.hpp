#ifndef EEGDATA_HPP
#define EEGDATA_HPP

#include <vector>

struct EEGData {
    double              timestamp = 0.0;
    std::vector<double> channels;
};

#endif  // EEGDATA_HPP