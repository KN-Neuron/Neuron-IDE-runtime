#ifndef RUNTIME_HPP
#define RUNTIME_HPP

#include <concurrentqueue.h>

#include <config/ExperimentConfig.hpp>
#include <functional>
#include <memory>
#include <stop_token>
#include <string>

class Scene;
class LSLReader;
class DataWriter;
class Renderer;
struct EEGData;
struct Marker;

class SDL_Renderer;

struct RuntimePaths {
    std::string config;           // device config.json
    std::string experiment;       // serialized experiment scene (protobuf)
    std::string outputDir = ".";  // directory the recorded CSV is written to
};

class Runtime {
   public:
    using RenderTargetFactory =
        std::function<std::shared_ptr<SDL_Renderer>(const std::string& windowTitle)>;

    explicit Runtime(const RuntimePaths& paths);
    Runtime(const RuntimePaths& paths, const RenderTargetFactory& renderTargetFactory);
    ~Runtime();

    Runtime(const Runtime&)            = delete;
    Runtime& operator=(const Runtime&) = delete;
    Runtime(Runtime&&)                 = delete;
    Runtime& operator=(Runtime&&)      = delete;

    void run();

    void requestStop();

    const std::string& outputPath() const noexcept { return outputFilePath; }

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

    void        startWorkers(const std::string& outputPath);
    void        shutdown();
    std::string makeOutputPath() const;

    RuntimePaths     paths;
    ExperimentConfig config;

    SdlSession sdlSession;

    std::shared_ptr<Scene>                                scene;
    std::shared_ptr<moodycamel::ConcurrentQueue<EEGData>> eegQueue;
    std::shared_ptr<moodycamel::ConcurrentQueue<Marker>>  markerQueue;

    std::shared_ptr<SDL_Renderer> sdlRenderer;

    std::unique_ptr<LSLReader>  lslReader;
    std::unique_ptr<DataWriter> dataWriter;
    std::unique_ptr<Renderer>   renderer;

    std::string      outputExtension;
    std::string      outputFilePath;
    std::stop_source stopSource;
};

#endif  // RUNTIME_HPP
