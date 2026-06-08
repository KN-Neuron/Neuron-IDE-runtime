#ifndef DATAWRITER_HPP
#define DATAWRITER_HPP

#include <blockingconcurrentqueue.h>

#include <EEGData.hpp>
#include <Marker.hpp>
#include <atomic>
#include <fstream>
#include <memory>
#include <string>
#include <thread>

class DataWriter {
   public:
    DataWriter() = default;
    ~DataWriter();

    DataWriter(const DataWriter&)            = delete;
    DataWriter& operator=(const DataWriter&) = delete;
    DataWriter(DataWriter&&)                 = delete;
    DataWriter& operator=(DataWriter&&)      = delete;

    void start(const std::string&                                    filePath,
               std::shared_ptr<moodycamel::ConcurrentQueue<EEGData>> eegQueue,
               std::shared_ptr<moodycamel::ConcurrentQueue<Marker>>  markerQueue);
    void stop();

   private:
    void writeLoop();
    void writeEEGData(const EEGData& data);
    void writeMarker(const Marker& marker);

    std::ofstream                                         outputFile;
    std::shared_ptr<moodycamel::ConcurrentQueue<EEGData>> eegQueue;
    std::shared_ptr<moodycamel::ConcurrentQueue<Marker>>  markerQueue;
    std::thread                                           writerThread;

    std::atomic<bool> stopRequested{false};
};

#endif  // DATAWRITER_HPP