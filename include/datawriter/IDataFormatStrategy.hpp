#ifndef IDATAFORMATSTRATEGY_HPP
#define IDATAFORMATSTRATEGY_HPP

#include <string>

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

    virtual void open(const std::string& filepath) = 0;
    virtual void close()                           = 0;

    virtual void writeHeader()                     = 0;
    virtual void writeEEGData(const EEGData& data) = 0;
    virtual void writeMarker(const Marker& marker) = 0;
};

#endif  // IDATAFORMATSTRATEGY_HPP
