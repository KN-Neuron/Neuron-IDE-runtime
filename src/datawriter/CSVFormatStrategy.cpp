#include <EEGData.hpp>
#include <datawriter/CSVFormatStrategy.hpp>
#include <datawriter/Marker.hpp>

void CSVFormatStrategy::writeHeader(std::ofstream& outputFile) const {
    outputFile << "type,timestamp,payload\n";
}

void CSVFormatStrategy::writeEEGData(std::ofstream& outputFile, const EEGData& data) const {
    outputFile << "eeg," << data.timestamp << ",\"";
    for (std::size_t index = 0; index < data.channels.size(); ++index) {
        if (index > 0) {
            outputFile << ',';
        }
        outputFile << data.channels[index];
    }
    outputFile << '"' << '\n';
}

void CSVFormatStrategy::writeMarker(std::ofstream& outputFile, const Marker& marker) const {
    outputFile << "marker," << marker.timestamp << ",\"" << marker.eventName << '"' << '\n';
}