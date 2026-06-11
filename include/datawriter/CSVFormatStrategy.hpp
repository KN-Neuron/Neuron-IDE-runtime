#ifndef CSVFORMATSTRATEGY_HPP
#define CSVFORMATSTRATEGY_HPP

#include <datawriter/IDataFormatStrategy.hpp>

class CSVFormatStrategy : public IDataFormatStrategy {
   public:
    CSVFormatStrategy()           = default;
    ~CSVFormatStrategy() override = default;

    void writeHeader(std::ofstream& outputFile) const override;
    void writeEEGData(std::ofstream& outputFile, const EEGData& data) const override;
    void writeMarker(std::ofstream& outputFile, const Marker& marker) const override;
};

#endif  // CSVFORMATSTRATEGY_HPP