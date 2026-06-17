#include <data_structures/EEGData.hpp>
#include <data_structures/Marker.hpp>
#include <datawriter/CSVFormatStrategy.hpp>
#include <stdexcept>

CSVFormatStrategy::~CSVFormatStrategy() { close(); }

void CSVFormatStrategy::open(const std::string& filepath) {
    outputFile.open(filepath, std::ios::out | std::ios::trunc);
    if (!outputFile.is_open()) {
        throw std::runtime_error("Failed to open CSV output file: " + filepath);
    }
}

void CSVFormatStrategy::close() {
    if (outputFile.is_open()) {
        outputFile.flush();
        outputFile.close();
    }
}

void CSVFormatStrategy::writeHeader() { outputFile << "type,timestamp,payload\n"; }

void CSVFormatStrategy::writeEEGData(const EEGData& data) {
    outputFile << "eeg," << data.timestamp << ",\"";
    for (std::size_t index = 0; index < data.channels.size(); ++index) {
        if (index > 0) {
            outputFile << ',';
        }
        outputFile << data.channels[index];
    }
    outputFile << '"' << '\n';
}

void CSVFormatStrategy::writeMarker(const Marker& marker) {
    outputFile << "marker," << marker.timestamp << ",\"" << marker.eventName << '"' << '\n';
}
