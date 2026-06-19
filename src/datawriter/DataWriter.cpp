#include <chrono>
#include <data_structures/EEGData.hpp>
#include <data_structures/Marker.hpp>
#include <datawriter/DataWriter.hpp>
#include <stdexcept>
#include <thread>
#include <utility>

constexpr auto kWriteLoopSleep = std::chrono::milliseconds(10);

template <typename QueueT, typename ItemT, typename WriteFn>
bool drainQueue(const std::shared_ptr<QueueT>& queue, WriteFn writeFn) {
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

DataWriter::DataWriter(std::unique_ptr<IDataFormatStrategy> strategy)
    : formatStrategy(std::move(strategy)) {}

DataWriter::~DataWriter() { stop(); }

void DataWriter::start(const std::string&                                    filePath,
                       std::shared_ptr<moodycamel::ConcurrentQueue<EEGData>> eegQueue,
                       std::shared_ptr<moodycamel::ConcurrentQueue<Marker>>  markerQueue) {
    stop();

    if (!formatStrategy) {
        throw std::runtime_error("No data format strategy set for DataWriter.");
    }

    this->eegQueue    = std::move(eegQueue);
    this->markerQueue = std::move(markerQueue);

    formatStrategy->open(filePath);
    formatStrategy->writeHeader();
    writerThread = std::jthread([this](const std::stop_token& stopToken) { writeLoop(stopToken); });
}

void DataWriter::stop() {
    if (writerThread.joinable()) {
        writerThread.request_stop();
    }

    if (writerThread.joinable()) {
        writerThread.join();
    }

    if (formatStrategy) {
        formatStrategy->close();
    }
}

void DataWriter::writeLoop(const std::stop_token& stopToken) {
    while (!stopToken.stop_requested()) {
        bool wroteData = drainQueue<moodycamel::ConcurrentQueue<EEGData>, EEGData>(
            eegQueue, [this](const EEGData& eegData) { formatStrategy->writeEEGData(eegData); });

        wroteData |= drainQueue<moodycamel::ConcurrentQueue<Marker>, Marker>(
            markerQueue, [this](const Marker& marker) { formatStrategy->writeMarker(marker); });

        if (!wroteData) {
            std::this_thread::sleep_for(kWriteLoopSleep);
        }
    }

    drainQueue<moodycamel::ConcurrentQueue<EEGData>, EEGData>(
        eegQueue, [this](const EEGData& eegData) { formatStrategy->writeEEGData(eegData); });
    drainQueue<moodycamel::ConcurrentQueue<Marker>, Marker>(
        markerQueue, [this](const Marker& marker) { formatStrategy->writeMarker(marker); });
}
