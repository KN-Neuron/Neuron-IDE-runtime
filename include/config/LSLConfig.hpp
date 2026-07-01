#ifndef LSLCONFIG_HPP
#define LSLCONFIG_HPP

#include <config/ChannelConfig.hpp>
#include <string>
#include <vector>

struct LSLConfig {
    std::string                name;      // lsl_stream.name
    std::string                type;      // lsl_stream.type
    std::string                sourceId;  // lsl_stream.source_id
    int                        expectedChannelCount = 0;
    double                     expectedSampleRateHz = 0.0;
    std::vector<ChannelConfig> channels;
};

#endif  // LSLCONFIG_HPP
