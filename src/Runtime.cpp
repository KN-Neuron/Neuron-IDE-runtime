#include <SDL2/SDL.h>

#include <Runtime.hpp>
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <config/ConfigParser.hpp>
#include <ctime>
#include <data_structures/EEGData.hpp>
#include <data_structures/Marker.hpp>
#include <datawriter/DataFormatStrategyFactory.hpp>
#include <datawriter/DataWriter.hpp>
#include <datawriter/IDataFormatStrategy.hpp>
#include <exception>
#include <filesystem>
#include <iostream>
#include <lslreader/LSLReader.hpp>
#include <memory>
#include <parser/Parser.hpp>
#include <renderer/Renderer.hpp>
#include <scene/Scene.hpp>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace {
namespace fs = std::filesystem;

constexpr const char* kDefaultTitle    = "NeuronIDE";
constexpr const char* kFallbackName    = "experiment";
constexpr const char* kTimestampFormat = "%Y%m%dT%H%M%S";  // e.g. 20261009T143012

constexpr std::size_t kTimestampBufferSize = 32;
// How often the window's events are handled while waiting for the EEG stream.
constexpr auto kEventPollInterval = std::chrono::milliseconds(50);

std::string sdlError(const char* what) { return std::string(what) + ": " + SDL_GetError(); }

// The experiment name comes from an authored protobuf file and ends up in a file
// name, so anything that is not plainly safe becomes '_'. Left unfiltered, a name
// like "block 1/run" would resolve to a missing subdirectory (and "../x" would
// escape the output directory entirely).
std::string sanitizeForFileName(std::string name) {
    if (name.empty()) {
        return kFallbackName;
    }

    std::replace_if(
        name.begin(), name.end(),
        [](unsigned char character) {
            return std::isalnum(character) == 0 && character != '-' && character != '_';
        },
        '_');
    return name;
}

// Local wall-clock time, as a researcher reads it when looking for a session.
std::string localTimestamp() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm           local{};
    localtime_r(&now, &local);

    std::array<char, kTimestampBufferSize> buffer{};
    const std::size_t                      length =
        std::strftime(buffer.data(), buffer.size(), kTimestampFormat, &local);
    return {buffer.data(), length};
}

std::string describe(const std::exception_ptr& error) {
    if (!error) {
        return "acquisition stopped unexpectedly";
    }
    try {
        std::rethrow_exception(error);
    } catch (const std::exception& e) {
        return e.what();
    } catch (...) {
        return "unknown error";
    }
}

// Handles every pending window event, so the window stays responsive while
// nothing is being rendered yet, and reports whether the operator closed it.
bool quitRequested() {
    bool      quit = false;
    SDL_Event event{};
    while (SDL_PollEvent(&event) == 1) {
        if (event.type == SDL_QUIT) {
            quit = true;
        }
    }
    return quit;
}
}  // namespace

Runtime::SdlSession::SdlSession() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        throw std::runtime_error(sdlError("Runtime: SDL_Init failed"));
    }
}

Runtime::SdlSession::~SdlSession() { SDL_Quit(); }

Runtime::RenderTargetFactory Runtime::defaultRenderTargetFactory() {
    return [](const std::string&   windowTitle,
              const DisplayConfig& display) -> std::shared_ptr<SDL_Renderer> {
        const std::string title = windowTitle.empty() ? kDefaultTitle : windowTitle;

        const int displayCount = SDL_GetNumVideoDisplays();
        if (displayCount < 1) {
            throw std::runtime_error(sdlError("Runtime: no display available"));
        }
        if (display.index >= displayCount) {
            throw std::runtime_error("Runtime: display.index is " + std::to_string(display.index) +
                                     " but only " + std::to_string(displayCount) +
                                     " display(s) are connected");
        }

        // Desktop fullscreen keeps the monitor's native mode: no mode switch,
        // and the compositor can hand the window the display directly.
        Uint32 windowFlags = SDL_WINDOW_SHOWN;
        if (display.fullscreen) {
            windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        }
        const int position = static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(display.index));

        std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window(
            SDL_CreateWindow(title.c_str(), position, position, display.width, display.height,
                             windowFlags),
            &SDL_DestroyWindow);
        if (!window) {
            throw std::runtime_error(sdlError("Runtime: SDL_CreateWindow failed"));
        }

        std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer(
            SDL_CreateRenderer(window.get(), -1,
                               SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC),
            &SDL_DestroyRenderer);
        if (!renderer) {
            throw std::runtime_error(sdlError("Runtime: SDL_CreateRenderer failed"));
        }

        // SDL quietly drops PRESENTVSYNC when the driver cannot honour it and
        // still returns a renderer. Markers are stamped right after
        // SDL_RenderPresent, so without vsync they would not match the frame
        // that is actually on screen.
        SDL_RendererInfo info{};
        if (SDL_GetRendererInfo(renderer.get(), &info) != 0) {
            throw std::runtime_error(sdlError("Runtime: SDL_GetRendererInfo failed"));
        }
        if ((info.flags & SDL_RENDERER_PRESENTVSYNC) == 0U) {
            throw std::runtime_error(
                std::string("Runtime: renderer '") + info.name +
                "' cannot present in sync with the display refresh; marker timestamps would "
                "not match the frames on screen");
        }

        return {renderer.release(), [window = window.release()](SDL_Renderer* target) {
                    if (target != nullptr) {
                        SDL_DestroyRenderer(target);
                    }
                    SDL_DestroyWindow(window);
                }};
    };
}

Runtime::Runtime(const RuntimePaths& paths) : Runtime(paths, defaultRenderTargetFactory()) {}

Runtime::Runtime(const RuntimePaths& paths, const RenderTargetFactory& renderTargetFactory,
                 std::chrono::milliseconds acquisitionTimeout)
    : paths(paths),
      config(ConfigParser::parse(paths.config)),
      acquisitionTimeout(acquisitionTimeout),
      ownerThread(std::this_thread::get_id()),
      scene(Parser::parse(paths.experiment)),
      eegQueue(std::make_shared<moodycamel::ConcurrentQueue<EEGData>>()),
      markerQueue(std::make_shared<moodycamel::ConcurrentQueue<Marker>>()) {
    if (!renderTargetFactory) {
        throw std::invalid_argument("Runtime: render target factory must not be null");
    }
    if (acquisitionTimeout <= std::chrono::milliseconds::zero()) {
        throw std::invalid_argument("Runtime: acquisition timeout must be positive");
    }
    if (!fs::is_directory(paths.outputDir)) {
        throw std::invalid_argument("Runtime: output directory does not exist: " + paths.outputDir);
    }

    // Everything that can still reject the configuration runs before the window
    // opens, so a bad config fails without flashing a window at the participant.
    auto formatStrategy = DataFormatStrategyFactory::create(config.output.format);
    outputExtension     = formatStrategy->fileExtension();

    lslReader  = std::make_unique<LSLReader>(config);
    dataWriter = std::make_unique<DataWriter>(std::move(formatStrategy));

    sdlRenderer = renderTargetFactory(scene->getExperimentName(), config.display);
    if (!sdlRenderer) {
        throw std::runtime_error("Runtime: render target factory returned no renderer");
    }

    renderer = std::make_unique<Renderer>(scene, sdlRenderer, markerQueue);
}

Runtime::~Runtime() { shutdown(); }

// <experiment>_<local time>.<ext>, e.g. "ssvep_20261009T143012.csv". A numeric
// suffix keeps a second recording started within the same second from reusing
// the name; the format strategy refuses to overwrite regardless, so losing a
// race on the name fails loudly instead of destroying a recording.
std::string Runtime::makeOutputPath() const {
    const std::string stem =
        sanitizeForFileName(scene->getExperimentName()) + "_" + localTimestamp();
    const fs::path directory(paths.outputDir);

    fs::path candidate = directory / (stem + "." + outputExtension);
    for (int attempt = 2; fs::exists(candidate); ++attempt) {
        candidate = directory / (stem + "_" + std::to_string(attempt) + "." + outputExtension);
    }
    return candidate.string();
}

// True once samples are flowing; false if the window was closed or
// requestStop() was called first.
bool Runtime::awaitAcquisition() {
    std::cerr << "Runtime: waiting for EEG stream '" << config.lsl.name << "'...\n";

    const std::stop_token token    = stopSource.get_token();
    const auto            deadline = std::chrono::steady_clock::now() + acquisitionTimeout;

    while (!token.stop_requested()) {
        if (quitRequested()) {
            return false;
        }

        const auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            throw std::runtime_error(
                "Runtime: EEG stream '" + config.lsl.name + "' delivered no samples within " +
                std::to_string(
                    std::chrono::duration_cast<std::chrono::seconds>(acquisitionTimeout).count()) +
                " s; the experiment was not started");
        }

        switch (
            lslReader->waitWhileResolving(std::min(now + kEventPollInterval, deadline), token)) {
            case AcquisitionState::Streaming:
                return true;
            case AcquisitionState::Resolving:
                break;  // not yet: handle window events and keep waiting
            case AcquisitionState::Idle:
            case AcquisitionState::Failed:
                throwAcquisitionFailure("before the experiment started");
        }
    }
    return false;
}

void Runtime::startRecording() {
    outputFilePath = makeOutputPath();
    dataWriter->start(outputFilePath, eegQueue, markerQueue);
}

void Runtime::run() {
    if (std::this_thread::get_id() != ownerThread) {
        throw std::logic_error(
            "Runtime: run() must be called on the thread that constructed the Runtime");
    }
    if (hasRun) {
        throw std::logic_error("Runtime: run() was already called; a Runtime runs once");
    }
    hasRun = true;

    try {
        // A failure that ends acquisition for good also ends the experiment:
        // stimuli without EEG are useless for analysis.
        lslReader->start(eegQueue, [this] { stopSource.request_stop(); });

        if (awaitAcquisition()) {
            startRecording();
            renderer->render(stopSource.get_token());
        }
    } catch (...) {
        shutdown();
        throw;
    }
    shutdown();

    if (lslReader->state() == AcquisitionState::Failed) {
        throwAcquisitionFailure("during the experiment; the recording was stopped");
    }
}

void Runtime::requestStop() { stopSource.request_stop(); }

void Runtime::throwAcquisitionFailure(const std::string& phase) const {
    throw std::runtime_error("Runtime: EEG acquisition failed " + phase + ": " +
                             describe(lslReader->failure()));
}

void Runtime::shutdown() {
    stopSource.request_stop();

    if (lslReader) {
        lslReader->stop();
    }
    if (dataWriter) {
        dataWriter->stop();
    }
}
