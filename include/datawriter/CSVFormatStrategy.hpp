#ifndef CSVFORMATSTRATEGY_HPP
#define CSVFORMATSTRATEGY_HPP

#include <datawriter/ExclusiveOutputFile.hpp>
#include <datawriter/IDataFormatStrategy.hpp>
#include <string>

class CSVFormatStrategy : public IDataFormatStrategy {
   public:
    CSVFormatStrategy() = default;
    ~CSVFormatStrategy() override;

    CSVFormatStrategy(const CSVFormatStrategy&)            = delete;
    CSVFormatStrategy& operator=(const CSVFormatStrategy&) = delete;
    CSVFormatStrategy(CSVFormatStrategy&&)                 = delete;
    CSVFormatStrategy& operator=(CSVFormatStrategy&&)      = delete;

    std::string fileExtension() const override { return "csv"; }

    void open(const std::string& filepath) override;
    void close() override;

    void writeHeader() override;
    void writeEEGData(const EEGData& data) override;
    void writeMarker(const Marker& marker) override;

   private:
    ExclusiveOutputFile outputFile;
};

#endif  // CSVFORMATSTRATEGY_HPP
