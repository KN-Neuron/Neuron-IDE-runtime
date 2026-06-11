#ifndef IDATAFORMATSTRATEGY_HPP
#define IDATAFORMATSTRATEGY_HPP

#include <fstream>

class EEGData;
struct Marker;

class IDataFormatStrategy {
   public:
    IDataFormatStrategy()          = default;
    virtual ~IDataFormatStrategy() = default;

    IDataFormatStrategy(const IDataFormatStrategy&)            = delete;
    IDataFormatStrategy& operator=(const IDataFormatStrategy&) = delete;
    IDataFormatStrategy(IDataFormatStrategy&&)                 = delete;
    IDataFormatStrategy& operator=(IDataFormatStrategy&&)      = delete;

    virtual void writeHeader(std::ofstream& outputFile) const                       = 0;
    virtual void writeEEGData(std::ofstream& outputFile, const EEGData& data) const = 0;
    virtual void writeMarker(std::ofstream& outputFile, const Marker& marker) const = 0;
};

#endif  // IDATAFORMATSTRATEGY_HPP