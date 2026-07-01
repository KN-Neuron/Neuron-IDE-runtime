#ifndef OUTPUTCONFIG_HPP
#define OUTPUTCONFIG_HPP

#include <string>

// How recorded data should be persisted. The `format` selects the DataWriter
// format strategy (see DataFormatStrategyFactory) and, through it, the output
// file extension.
struct OutputConfig {
    std::string format = "csv";
};

#endif  // OUTPUTCONFIG_HPP
