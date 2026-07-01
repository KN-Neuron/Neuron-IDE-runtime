#ifndef OUTPUTCONFIG_HPP
#define OUTPUTCONFIG_HPP

#include <string>

// How recorded data should be persisted. The `format` selects the DataWriter
// format strategy (see DataFormatStrategyFactory) and, through it, the output
// file extension.
struct OutputConfig {
    std::string format = "csv";

    // Throws std::invalid_argument on an empty format. Whether a non-empty
    // format is *known* is DataFormatStrategyFactory's decision, so that check
    // stays out of the config layer.
    void validate() const;
};

#endif  // OUTPUTCONFIG_HPP
