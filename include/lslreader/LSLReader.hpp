#ifndef LSLREADER_HPP
#define LSLREADER_HPP

#include <concurrentqueue.h>

#include <chrono>
#include <condition_variable>
#include <config/DeviceConfig.hpp>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <stop_token>
#include <thread>
#include <vector>

struct EEGData;

enum class AcquisitionState : std::uint8_t {
    Idle,       // not started, or stopped
    Resolving,  // looking for the stream: right after start(), and again after it was lost
    Streaming,  // samples from the resolved stream are being queued
    Failed,     // gave up for good (see LSLReader::failure()); the worker has exited
};

// Acquires the device's LSL stream on its own worker thread and pushes samples
// onto eegQueue. Only the channels the device config marks as enabled are
// forwarded; each pushed EEGData holds those channels in config declaration
// order, so its values line up with the enabled entries of DeviceConfig::channels.
class LSLReader {
   public:
    // Invoked once, on the reader thread, when acquisition enters Failed.
    using FailureCallback = std::function<void()>;

    // Validates the config (see DeviceConfig::validate) and throws
    // std::invalid_argument if it is invalid or enables no channels.
    explicit LSLReader(DeviceConfig deviceConfig);
    ~LSLReader();

    LSLReader(const LSLReader&)            = delete;
    LSLReader& operator=(const LSLReader&) = delete;
    LSLReader(LSLReader&&)                 = delete;
    LSLReader& operator=(LSLReader&&)      = delete;

    // Returns immediately; the stream is resolved on the worker thread. The
    // state is Resolving by the time start() returns.
    void start(std::shared_ptr<moodycamel::ConcurrentQueue<EEGData>> eegQueue,
               FailureCallback                                       onFailure = {});
    // Joins the worker. A Failed state and its failure() survive stop(), so the
    // reason can still be read after the worker is gone; any other state
    // becomes Idle.
    void stop();

    AcquisitionState state() const;

    // Blocks while the reader is Resolving, until `deadline` or until stopToken
    // fires, and returns the state it ended on.
    AcquisitionState waitWhileResolving(std::chrono::steady_clock::time_point deadline,
                                        const std::stop_token&                stopToken) const;

    // Why acquisition failed; null unless state() is Failed.
    std::exception_ptr failure() const;

   private:
    void readLoop(const std::stop_token& stopToken);
    void setState(AcquisitionState next);
    void fail(std::exception_ptr reason);

    DeviceConfig config;
    // Sample offsets to forward, in config declaration order.
    std::vector<std::size_t>                              enabledChannelIndices;
    bool                                                  forwardsWholeSample = false;
    std::shared_ptr<moodycamel::ConcurrentQueue<EEGData>> eegQueue;
    FailureCallback                                       onFailure;

    mutable std::mutex                  stateMutex;
    mutable std::condition_variable_any stateChanged;
    AcquisitionState                    currentState = AcquisitionState::Idle;
    std::exception_ptr                  failureReason;

    // Last, so it is joined before the members the worker uses are destroyed.
    std::jthread readerThread;
};

#endif  // LSLREADER_HPP
