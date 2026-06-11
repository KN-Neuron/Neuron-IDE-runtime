#include <EEGData.hpp>
#include <chrono>
#include <datawriter/CSVFormatStrategy.hpp>
#include <datawriter/DataWriter.hpp>
#include <datawriter/IDataFormatStrategy.hpp>
#include <datawriter/Marker.hpp>
#include <stdexcept>
#include <thread>
#include <utility>

constexpr auto kWriteLoopSleep = std::chrono::milliseconds(10);

template <typename QueueT, typename ItemT, typename WriteFn>
bool drainQueue(const std::shared_ptr<QueueT>& queue, WriteFn&& writeFn) {
    bool wroteData = false;

    if (queue) {
        ItemT item;
        while (queue->try_dequeue(item)) {
            writeFn(item);
            wroteData = true;
        }
    }

    return wroteData;
}

DataWriter::DataWriter() : formatStrategy(std::make_unique<CSVFormatStrategy>()) {}

DataWriter::~DataWriter() { stop(); }

void DataWriter::start(const std::string&                                    filePath,
                       std::shared_ptr<moodycamel::ConcurrentQueue<EEGData>> eegQueue,
                       std::shared_ptr<moodycamel::ConcurrentQueue<Marker>>  markerQueue) {
    stop();

    if (!formatStrategy) {
        throw std::runtime_error("No data format strategy set for DataWriter.");
    }

    outputFile.open(filePath, std::ios::out | std::ios::trunc);
    if (!outputFile.is_open()) {
        throw std::runtime_error("Failed to open data writer output file: " + filePath);
    }

    this->eegQueue    = std::move(eegQueue);
    this->markerQueue = std::move(markerQueue);

    formatStrategy->writeHeader(outputFile);
    writerThread = std::jthread([this](const std::stop_token& stopToken) { writeLoop(stopToken); });
}

void DataWriter::stop() {
    if (writerThread.joinable()) {
        writerThread.request_stop();
    }

    if (writerThread.joinable()) {
        writerThread.join();
    }

    if (outputFile.is_open()) {
        outputFile.flush();
        outputFile.close();
    }
}

void DataWriter::writeLoop(const std::stop_token& stopToken) {
    while (!stopToken.stop_requested()) {
        bool wroteData = drainQueue<moodycamel::ConcurrentQueue<EEGData>, EEGData>(
            eegQueue,
            [this](const EEGData& eegData) { formatStrategy->writeEEGData(outputFile, eegData); });

        wroteData |= drainQueue<moodycamel::ConcurrentQueue<Marker>, Marker>(
            markerQueue,
            [this](const Marker& marker) { formatStrategy->writeMarker(outputFile, marker); });

        if (!wroteData) {
            std::this_thread::sleep_for(kWriteLoopSleep);
        }
    }

    drainQueue<moodycamel::ConcurrentQueue<EEGData>, EEGData>(
        eegQueue,
        [this](const EEGData& eegData) { formatStrategy->writeEEGData(outputFile, eegData); });
    drainQueue<moodycamel::ConcurrentQueue<Marker>, Marker>(
        markerQueue,
        [this](const Marker& marker) { formatStrategy->writeMarker(outputFile, marker); });
}