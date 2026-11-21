#include "renderer/Renderer.hpp"

#include <logging/Logging.hpp>

#include "data_structures/Context.hpp"
#include "data_structures/Marker.hpp"
#include "lsl_cpp.h"
#include "scene/Scene.hpp"

namespace {
constexpr double kSecondsPerMillisecond = 0.001;

quill::Logger* logger() { return logging::logger(logging::Subsystem::Renderer); }

// SDL stamps an event, in milliseconds on its own clock, when it reads the event from the OS. That
// happens in SDL_PollEvent, once per frame, so a key press is known to within one frame.
double toLslTime(Uint32 eventTicks) {
    const Uint32 ageMs = SDL_GetTicks() - eventTicks;
    return lsl::local_clock() - (static_cast<double>(ageMs) * kSecondsPerMillisecond);
}
}  // namespace

void Renderer::SDLWindowDeleter::operator()(SDL_Window* window) const {
    if (window != nullptr) {
        SDL_DestroyWindow(window);
    }
}

Renderer::Renderer(const std::shared_ptr<Scene>& scene, std::shared_ptr<SDL_Renderer> sdlRenderer,
                   std::shared_ptr<moodycamel::ConcurrentQueue<Marker>> markerQueue)
    : window(nullptr, SDLWindowDeleter{}),
      sdlRenderer(std::move(sdlRenderer)),
      currentScene(scene),
      markerQueue(std::move(markerQueue)) {
    // Resources are created here rather than on the first frame, so uploads never delay the
    // first stimulus frame and the marker stamped with it.
    prepareScene();
}

void Renderer::prepareScene() {
    if (auto scene = currentScene.lock(); scene && sdlRenderer) {
        scene->onRenderTargetReady(sdlRenderer);
    }
}

bool Renderer::pollEvents() {
    keyPresses.clear();
    bool      quit = false;
    SDL_Event event{};
    while (SDL_PollEvent(&event) == 1) {
        if (event.type == SDL_QUIT) {
            quit = true;
        } else if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
            keyPresses.push_back({event.key.keysym.scancode, toLslTime(event.key.timestamp)});
        } else if (event.type == SDL_RENDER_DEVICE_RESET) {
            // Every texture is gone (e.g. Direct3D after a lock screen or UAC prompt). Frames
            // around a reset may be dropped, so the log gives analysis the time on the LSL clock.
            LOG_WARNING(logger(),
                        "Render device reset at LSL time {:.6f} s; frames around it may be "
                        "missing. Uploading the scene again",
                        lsl::local_clock());
            prepareScene();
        }
    }
    return quit;
}

void Renderer::render(const std::stop_token& stoken) {
    std::vector<std::string> currentFrameMarkers;
    std::vector<Marker>      timedMarkers;
    auto                     lastTime = std::chrono::high_resolution_clock::now();

    while (!stoken.stop_requested()) {
        auto   currentTime = std::chrono::high_resolution_clock::now();
        double deltaTime   = std::chrono::duration<double>(currentTime - lastTime).count();
        lastTime           = currentTime;

        currentFrameMarkers.clear();
        timedMarkers.clear();

        if (pollEvents()) {
            return;
        }

        const Context ctx{.timestamp    = deltaTime,
                          .markers      = &currentFrameMarkers,
                          .timedMarkers = &timedMarkers,
                          .keyPresses   = keyPresses};

        if (auto scene = currentScene.lock()) {
            scene->update(ctx);
        }

        SDL_RenderClear(sdlRenderer.get());

        if (auto scene = currentScene.lock()) {
            scene->render(sdlRenderer.get());
        }

        SDL_RenderPresent(sdlRenderer.get());

        double exactTime = lsl::local_clock();

        for (const auto& markerName : currentFrameMarkers) {
            markerQueue->enqueue(Marker{markerName, exactTime});
        }
        for (auto& marker : timedMarkers) {
            markerQueue->enqueue(std::move(marker));
        }
    }
}
