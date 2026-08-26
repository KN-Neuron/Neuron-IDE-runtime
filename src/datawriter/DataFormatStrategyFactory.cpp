#include <algorithm>
#include <cctype>
#include <datawriter/CSVFormatStrategy.hpp>
#include <datawriter/DataFormatStrategyFactory.hpp>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace {
std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) { return std::tolower(character); });
    return value;
}

using StrategyCreator = std::function<std::unique_ptr<IDataFormatStrategy>()>;

const std::unordered_map<std::string, StrategyCreator>& creators() {
    static const std::unordered_map<std::string, StrategyCreator> registry = {
        {"csv", [] { return std::make_unique<CSVFormatStrategy>(); }},
    };
    return registry;
}
}  // namespace

std::unique_ptr<IDataFormatStrategy> DataFormatStrategyFactory::create(const std::string& format) {
    const auto entry = creators().find(toLower(format));
    if (entry == creators().end()) {
        throw std::invalid_argument("DataFormatStrategyFactory: unknown output format '" + format +
                                    "'");
    }
    return entry->second();
}
