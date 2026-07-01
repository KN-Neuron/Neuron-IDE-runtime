#ifndef LSLREADER_HPP
#define LSLREADER_HPP

#include <concurrentqueue.h>

#include <config/LSLConfig.hpp>
#include <memory>
#include <thread>

struct EEGData;

class LSLReader {
   public:
    explicit LSLReader(LSLConfig config);
    ~LSLReader();

    LSLReader(const LSLReader&)            = delete;
    LSLReader& operator=(const LSLReader&) = delete;
    LSLReader(LSLReader&&)                 = delete;
    LSLReader& operator=(LSLReader&&)      = delete;

    void start(std::shared_ptr<moodycamel::ConcurrentQueue<EEGData>> eegQueue);
    void stop();

   private:
    void readLoop(const std::stop_token& stopToken);

    LSLConfig                                             config;
    std::shared_ptr<moodycamel::ConcurrentQueue<EEGData>> eegQueue;
    std::jthread                                          readerThread;
};

#endif  // LSLREADER_HPP
