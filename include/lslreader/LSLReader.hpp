#ifndef LSLREADER_HPP
#define LSLREADER_HPP

#include <concurrentqueue.h>

#include <config/DeviceConfig.hpp>
#include <cstddef>
#include <memory>
#include <thread>
#include <vector>

struct EEGData;

// Acquires the device's LSL stream on its own worker thread and pushes samples
// onto eegQueue. Only the channels the device config marks as enabled are
// forwarded; each pushed EEGData holds those channels in config declaration
// order, so its values line up with the enabled entries of DeviceConfig::channels.
class LSLReader {
   public:
    // Validates the config (see DeviceConfig::validate) and throws
    // std::invalid_argument if it is invalid or enables no channels.
    explicit LSLReader(DeviceConfig deviceConfig);
    ~LSLReader();

    LSLReader(const LSLReader&)            = delete;
    LSLReader& operator=(const LSLReader&) = delete;
    LSLReader(LSLReader&&)                 = delete;
    LSLReader& operator=(LSLReader&&)      = delete;

    void start(std::shared_ptr<moodycamel::ConcurrentQueue<EEGData>> eegQueue);
    void stop();

   private:
    void readLoop(const std::stop_token& stopToken);

    DeviceConfig config;
    // Sample offsets to forward, in config declaration order.
    std::vector<std::size_t>                              enabledChannelIndices;
    bool                                                  forwardsWholeSample = false;
    std::shared_ptr<moodycamel::ConcurrentQueue<EEGData>> eegQueue;
    std::jthread                                          readerThread;
};

#endif  // LSLREADER_HPP
