#ifndef RUNTIME_HPP
#define RUNTIME_HPP

#include <concurrentqueue.h>

#include <chrono>
#include <config/DeviceConfig.hpp>
#include <functional>
#include <memory>
#include <stop_token>
#include <string>
#include <thread>

class Scene;
class LSLReader;
class DataWriter;
class Renderer;
struct EEGData;
struct Marker;

// Tag matches the other forward declarations in include/ (Scene, SceneObject,
// Component). SDL declares it as a struct, so all four are technically
// mismatched; changing one alone trips -Wmismatched-tags.
class SDL_Renderer;

struct RuntimePaths {
    std::string config;           // device config.json
    std::string experiment;       // serialized experiment scene (protobuf)
    std::string outputDir = ".";  // existing directory the recording is written to
};

class Runtime {
   public:
    using RenderTargetFactory = std::function<std::shared_ptr<SDL_Renderer>(
        const std::string& windowTitle, const DisplayConfig& display)>;

    // How long run() waits for the first EEG sample before giving up.
    static constexpr std::chrono::seconds kDefaultAcquisitionTimeout{30};

    // Parses and validates both input files and builds every worker before the
    // render target is created, so a bad configuration fails without opening a
    // window. The constructing thread becomes the only thread allowed to call
    // run().
    explicit Runtime(const RuntimePaths& paths);
    Runtime(const RuntimePaths& paths, const RenderTargetFactory& renderTargetFactory,
            std::chrono::milliseconds acquisitionTimeout = kDefaultAcquisitionTimeout);
    ~Runtime();

    Runtime(const Runtime&)            = delete;
    Runtime& operator=(const Runtime&) = delete;
    Runtime(Runtime&&)                 = delete;
    Runtime& operator=(Runtime&&)      = delete;

    // Runs the experiment on the calling thread, which must be the thread that
    // constructed this Runtime: SDL window events can only be handled there.
    //
    // 1. Starts EEG acquisition and waits for the first sample. The experiment
    //    never starts without EEG: a stream that fails validation, or delivers
    //    nothing within the acquisition timeout, throws std::runtime_error.
    //    Closing the window or requestStop() during the wait returns without
    //    recording anything.
    // 2. Creates a new recording file (an existing file is never overwritten)
    //    and renders until SDL_QUIT or requestStop().
    // 3. Stops the workers. If acquisition failed for good during the
    //    experiment, rendering stops at once, the recording is closed, and
    //    run() throws std::runtime_error.
    //
    // Single-shot: throws std::logic_error when called a second time or from
    // the wrong thread.
    void run();

    // Safe to call from any thread, including while run() is in progress.
    void requestStop();

    // Path of the recording, or empty if run() has not started one (yet, or
    // at all). Not synchronized: read it before run() starts or after it has
    // returned.
    const std::string& outputPath() const noexcept { return outputFilePath; }

    // Opens a window on display.index (fullscreen or display.width x height)
    // with an accelerated renderer, and throws std::runtime_error unless that
    // renderer actually presents in sync with the display refresh. Marker
    // timestamps are only correct when it does.
    static RenderTargetFactory defaultRenderTargetFactory();

   private:
    struct SdlSession {
        SdlSession();
        ~SdlSession();

        SdlSession(const SdlSession&)            = delete;
        SdlSession& operator=(const SdlSession&) = delete;
        SdlSession(SdlSession&&)                 = delete;
        SdlSession& operator=(SdlSession&&)      = delete;
    };

    bool                      awaitAcquisition();
    void                      startRecording();
    void                      shutdown();
    [[noreturn]] void         throwAcquisitionFailure(const std::string& phase) const;
    [[nodiscard]] std::string makeOutputPath() const;

    RuntimePaths              paths;
    DeviceConfig              config;
    std::chrono::milliseconds acquisitionTimeout;
    std::thread::id           ownerThread;
    bool                      hasRun = false;

    SdlSession sdlSession;

    // Declared before `scene` on purpose: members are destroyed in reverse
    // order, so the scene - and any SDL_Texture its components hold - is
    // released while the SDL_Renderer that owns those textures is still alive.
    std::shared_ptr<SDL_Renderer> sdlRenderer;

    std::shared_ptr<Scene>                                scene;
    std::shared_ptr<moodycamel::ConcurrentQueue<EEGData>> eegQueue;
    std::shared_ptr<moodycamel::ConcurrentQueue<Marker>>  markerQueue;

    std::unique_ptr<LSLReader>  lslReader;
    std::unique_ptr<DataWriter> dataWriter;
    std::unique_ptr<Renderer>   renderer;

    std::string      outputExtension;
    std::string      outputFilePath;
    std::stop_source stopSource;
};

#endif  // RUNTIME_HPP
