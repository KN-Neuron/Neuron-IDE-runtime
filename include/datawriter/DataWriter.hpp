#ifndef DATAWRITER_HPP
#define DATAWRITER_HPP

#include <concurrentqueue.h>

#include <datawriter/IDataFormatStrategy.hpp>

class EEGData;
class Marker;

#include <fstream>
#include <memory>
#include <string>
#include <thread>

class DataWriter {
   public:
    DataWriter();
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
    void writeLoop(const std::stop_token& stopToken);

    std::ofstream                                         outputFile;
    std::unique_ptr<IDataFormatStrategy>                  formatStrategy;
    std::shared_ptr<moodycamel::ConcurrentQueue<EEGData>> eegQueue;
    std::shared_ptr<moodycamel::ConcurrentQueue<Marker>>  markerQueue;
    std::jthread                                          writerThread;
};

#endif  // DATAWRITER_HPP