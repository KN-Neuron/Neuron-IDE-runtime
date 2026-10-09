#include <cstddef>
#include <data_structures/EEGData.hpp>
#include <data_structures/Marker.hpp>
#include <datawriter/CSVFormatStrategy.hpp>
#include <ostream>

CSVFormatStrategy::~CSVFormatStrategy() { CSVFormatStrategy::close(); }

void CSVFormatStrategy::open(const std::string& filepath) { outputFile.open(filepath); }

void CSVFormatStrategy::close() { outputFile.close(); }

void CSVFormatStrategy::writeHeader() { outputFile.stream() << "type,timestamp,payload\n"; }

void CSVFormatStrategy::writeEEGData(const EEGData& data) {
    std::ostream& out = outputFile.stream();
    out << "eeg," << data.timestamp << ",\"";
    for (std::size_t index = 0; index < data.channels.size(); ++index) {
        if (index > 0) {
            out << ',';
        }
        out << data.channels[index];
    }
    out << '"' << '\n';
}

void CSVFormatStrategy::writeMarker(const Marker& marker) {
    outputFile.stream() << "marker," << marker.timestamp << ",\"" << marker.eventName << '"'
                        << '\n';
}
