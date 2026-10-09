#include <cerrno>
#include <cstdio>
#include <data_structures/EEGData.hpp>
#include <data_structures/Marker.hpp>
#include <datawriter/CSVFormatStrategy.hpp>
#include <stdexcept>
#include <system_error>

CSVFormatStrategy::~CSVFormatStrategy() { CSVFormatStrategy::close(); }

void CSVFormatStrategy::open(const std::string& filepath) {
    // "x" (C11) creates the file, or fails if it already exists, in one atomic
    // step: an existing recording is never truncated, not even by a race on
    // the file name. C++20 streams cannot express this (std::ios::noreplace is
    // C++23), hence the short-lived C handle.
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    std::FILE* created = std::fopen(filepath.c_str(), "wx");
    if (created == nullptr) {
        const int error = errno;
        throw std::system_error(error, std::generic_category(),
                                "Failed to create CSV output file " + filepath);
    }
    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory)
    std::fclose(created);

    outputFile.open(filepath, std::ios::out);
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
