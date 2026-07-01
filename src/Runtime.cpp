#include <SDL2/SDL.h>

#include <Runtime.hpp>
#include <algorithm>
#include <chrono>
#include <config/ConfigParser.hpp>
#include <data_structures/EEGData.hpp>
#include <data_structures/Marker.hpp>
#include <datawriter/DataFormatStrategyFactory.hpp>
#include <datawriter/DataWriter.hpp>
#include <datawriter/IDataFormatStrategy.hpp>
#include <filesystem>
#include <lslreader/LSLReader.hpp>
#include <memory>
#include <parser/Parser.hpp>
#include <renderer/Renderer.hpp>
#include <scene/Scene.hpp>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
constexpr int         kWindowWidth  = 1280;
constexpr int         kWindowHeight = 720;
constexpr const char* kDefaultTitle = "NeuronIDE";
constexpr const char* kFallbackName = "experiment";

std::string sdlError(const char* what) { return std::string(what) + ": " + SDL_GetError(); }
}  // namespace

Runtime::SdlSession::SdlSession() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        throw std::runtime_error(sdlError("Runtime: SDL_Init failed"));
    }
}

Runtime::SdlSession::~SdlSession() { SDL_Quit(); }

Runtime::RenderTargetFactory Runtime::defaultRenderTargetFactory() {
    return [](const std::string& windowTitle) -> std::shared_ptr<SDL_Renderer> {
        const std::string title = windowTitle.empty() ? kDefaultTitle : windowTitle;

        SDL_Window* window =
            SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             kWindowWidth, kWindowHeight, SDL_WINDOW_SHOWN);
        if (window == nullptr) {
            throw std::runtime_error(sdlError("Runtime: SDL_CreateWindow failed"));
        }

        SDL_Renderer* renderer =
            SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (renderer == nullptr) {
            SDL_DestroyWindow(window);
            throw std::runtime_error(sdlError("Runtime: SDL_CreateRenderer failed"));
        }

        return {renderer, [window](SDL_Renderer* target) {
                    if (target != nullptr) {
                        SDL_DestroyRenderer(target);
                    }
                    SDL_DestroyWindow(window);
                }};
    };
}

Runtime::Runtime(const RuntimePaths& paths) : Runtime(paths, defaultRenderTargetFactory()) {}

Runtime::Runtime(const RuntimePaths& paths, const RenderTargetFactory& renderTargetFactory)
    : paths(paths),
      config(ConfigParser::parse(paths.config)),
      scene(Parser::parse(paths.experiment)),
      eegQueue(std::make_shared<moodycamel::ConcurrentQueue<EEGData>>()),
      markerQueue(std::make_shared<moodycamel::ConcurrentQueue<Marker>>()) {
    if (!renderTargetFactory) {
        throw std::invalid_argument("Runtime: render target factory must not be null");
    }

    sdlRenderer = renderTargetFactory(scene->getExperimentName());
    if (!sdlRenderer) {
        throw std::runtime_error("Runtime: render target factory returned no renderer");
    }

    auto formatStrategy = DataFormatStrategyFactory::create(config.output.format);
    outputExtension     = formatStrategy->fileExtension();

    lslReader  = std::make_unique<LSLReader>(config.lsl);
    dataWriter = std::make_unique<DataWriter>(std::move(formatStrategy));
    renderer   = std::make_unique<Renderer>(scene, sdlRenderer, markerQueue);
}

Runtime::~Runtime() { shutdown(); }

std::string Runtime::makeOutputPath() const {
    std::string name = scene->getExperimentName();
    if (name.empty()) {
        name = kFallbackName;
    }
    std::replace(name.begin(), name.end(), ' ', '_');

    const auto now   = std::chrono::system_clock::now().time_since_epoch();
    const auto epoch = std::chrono::duration_cast<std::chrono::seconds>(now).count();

    const std::filesystem::path fileName =
        name + "_" + std::to_string(epoch) + "." + outputExtension;
    return (std::filesystem::path(paths.outputDir) / fileName).string();
}

void Runtime::startWorkers(const std::string& outputPath) {
    dataWriter->start(outputPath, eegQueue, markerQueue);
    lslReader->start(eegQueue);
}

void Runtime::run() {
    outputFilePath = makeOutputPath();
    startWorkers(outputFilePath);

    renderer->render(stopSource.get_token());

    shutdown();
}

void Runtime::requestStop() { stopSource.request_stop(); }

void Runtime::shutdown() {
    stopSource.request_stop();

    if (lslReader) {
        lslReader->stop();
    }
    if (dataWriter) {
        dataWriter->stop();
    }
}
