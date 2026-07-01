#ifndef DATAFORMATSTRATEGYFACTORY_HPP
#define DATAFORMATSTRATEGYFACTORY_HPP

#include <datawriter/IDataFormatStrategy.hpp>
#include <memory>
#include <string>

// Builds the DataWriter format strategy selected by the config's output format.
// Adding a new output format means registering it here; nothing else in the
// runtime needs to change.
class DataFormatStrategyFactory {
   public:
    // Returns the strategy for the given format (case-insensitive), e.g. "csv".
    // Throws std::invalid_argument if the format is unknown.
    static std::unique_ptr<IDataFormatStrategy> create(const std::string& format);
};

#endif  // DATAFORMATSTRATEGYFACTORY_HPP
