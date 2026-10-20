#include "renderer/Renderer.hpp"

#include "data_structures/Context.hpp"
#include "data_structures/Marker.hpp"
#include "lsl_cpp.h"
#include "scene/Scene.hpp"

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
    bool      quit = false;
    SDL_Event event{};
    while (SDL_PollEvent(&event) == 1) {
        if (event.type == SDL_QUIT) {
            quit = true;
        } else if (event.type == SDL_RENDER_DEVICE_RESET) {
            // Every texture is gone (e.g. Direct3D after a lock screen or UAC prompt).
            // TODO(quill): warning with lsl::local_clock(): frames around a reset may be dropped,
            // and whoever analyses the recording needs to know when.
            prepareScene();
        }
    }
    return quit;
}

void Renderer::render(const std::stop_token& stoken) {
    std::vector<std::string> currentFrameMarkers;
    auto                     lastTime = std::chrono::high_resolution_clock::now();

    while (!stoken.stop_requested()) {
        auto   currentTime = std::chrono::high_resolution_clock::now();
        double deltaTime   = std::chrono::duration<double>(currentTime - lastTime).count();
        lastTime           = currentTime;

        currentFrameMarkers.clear();
        Context ctx{deltaTime, &currentFrameMarkers};

        if (pollEvents()) {
            return;
        }

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
    }
}
