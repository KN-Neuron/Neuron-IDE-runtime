#include <EEGData.hpp>
#include <chrono>
#include <datawriter/DataWriter.hpp>
#include <datawriter/IDataFormatStrategy.hpp>
#include <datawriter/Marker.hpp>
#include <stdexcept>
#include <thread>
#include <utility>

constexpr auto kWriteLoopSleep = std::chrono::milliseconds(10);

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
    writerThread = std::jthread(&DataWriter::writeLoop, this);
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
        bool wroteData = false;

        if (eegQueue) {
            EEGData eegData;
            while (eegQueue->try_dequeue(eegData)) {
                formatStrategy->writeEEGData(outputFile, eegData);
                wroteData = true;
            }
        }

        if (markerQueue) {
            Marker marker;
            while (markerQueue->try_dequeue(marker)) {
                formatStrategy->writeMarker(outputFile, marker);
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
            formatStrategy->writeEEGData(outputFile, eegData);
        }
    }

    if (markerQueue) {
        Marker marker;
        while (markerQueue->try_dequeue(marker)) {
            formatStrategy->writeMarker(outputFile, marker);
        }
    }
}