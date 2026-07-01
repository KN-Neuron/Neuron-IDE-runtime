#ifndef CHANNELCONFIG_HPP
#define CHANNELCONFIG_HPP

#include <string>

// Description of a single EEG channel as declared in the device config file.
struct ChannelConfig {
    int         index = 0;
    std::string label;
    bool        enabled = true;
    std::string unit;
};

#endif  // CHANNELCONFIG_HPP
