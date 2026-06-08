#include <DataWriter.hpp>
#include <chrono>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <utility>

// anonymous namespace here for private utility-style definitions
namespace {
constexpr auto kWriteLoopSleep = std::chrono::milliseconds(10);

std::string formatChannels(const std::vector<float>& channels) {
    std::ostringstream stream;
    for (std::size_t index = 0; index < channels.size(); ++index) {
        if (index > 0) {
            stream << ',';
        }
        stream << channels[index];
    }
    return stream.str();
}
}  // namespace

DataWriter::~DataWriter() { stop(); }

void DataWriter::start(const std::string&                                    filePath,
                       std::shared_ptr<moodycamel::ConcurrentQueue<EEGData>> eegQueue,
                       std::shared_ptr<moodycamel::ConcurrentQueue<Marker>>  markerQueue) {
    stop();

    outputFile.open(filePath, std::ios::out | std::ios::trunc);
    if (!outputFile.is_open()) {
        throw std::runtime_error("Failed to open data writer output file: " + filePath);
    }

    this->eegQueue    = std::move(eegQueue);
    this->markerQueue = std::move(markerQueue);
    stopRequested.store(false);

    outputFile << "type,timestamp,payload\n";
    writerThread = std::thread(&DataWriter::writeLoop, this);
}

void DataWriter::stop() {
    stopRequested.store(true);

    if (writerThread.joinable()) {
        writerThread.join();
    }

    if (outputFile.is_open()) {
        outputFile.flush();
        outputFile.close();
    }
}

void DataWriter::writeLoop() {
    while (!stopRequested.load()) {
        bool wroteData = false;

        if (eegQueue) {
            EEGData eegData;
            while (eegQueue->try_dequeue(eegData)) {
                writeEEGData(eegData);
                wroteData = true;
            }
        }

        if (markerQueue) {
            Marker marker;
            while (markerQueue->try_dequeue(marker)) {
                writeMarker(marker);
                wroteData = true;
            }
        }

        if (!wroteData) {
            std::this_thread::sleep_for(kWriteLoopSleep);
        }
    }

    if (eegQueue) {
        EEGData eegData;
        while (eegQueue->try_dequeue(eegData)) {
            writeEEGData(eegData);
        }
    }

    if (markerQueue) {
        Marker marker;
        while (markerQueue->try_dequeue(marker)) {
            writeMarker(marker);
        }
    }
}

void DataWriter::writeEEGData(const EEGData& data) {
    if (!outputFile.is_open()) {
        return;
    }

    outputFile << "eeg," << data.timestamp << ",\"" << formatChannels(data.channels) << '"' << '\n';
}

void DataWriter::writeMarker(const Marker& marker) {
    if (!outputFile.is_open()) {
        return;
    }

    outputFile << "marker," << marker.timestamp << ",\"" << marker.eventName << '"' << '\n';
}