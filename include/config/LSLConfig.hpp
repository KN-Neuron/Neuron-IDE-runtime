#ifndef LSLCONFIG_HPP
#define LSLCONFIG_HPP

#include <string>

// Identity and expected shape of the device's LSL stream (`lsl_stream` in
// `config.json`). The channel table lives next to this in DeviceConfig,
// mirroring the top-level `channels` key of the JSON file.
struct LSLConfig {
    std::string name;      // lsl_stream.name
    std::string type;      // lsl_stream.type
    std::string sourceId;  // lsl_stream.source_id
    int         expectedChannelCount = 0;
    double      expectedSampleRateHz = 0.0;

    // Throws std::invalid_argument if the stream cannot be resolved or checked
    // against with these values.
    void validate() const;
};

#endif  // LSLCONFIG_HPP
