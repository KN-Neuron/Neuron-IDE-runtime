#include "renderer/Renderer.hpp"

#include "data_structures/Context.hpp"
#include "data_structures/Marker.hpp"
#include "lsl_cpp.h"
#include "scene/Scene.hpp"

void Renderer::render(const std::stop_token& stoken) {
    std::vector<std::string> currentFrameMarkers;
    auto                     lastTime = std::chrono::high_resolution_clock::now();

    while (!stoken.stop_requested()) {
        auto   currentTime = std::chrono::high_resolution_clock::now();
        double deltaTime   = std::chrono::duration<double>(currentTime - lastTime).count();
        lastTime           = currentTime;

        currentFrameMarkers.clear();
        Context ctx{deltaTime, currentFrameMarkers};

        SDL_Event event;
        while (SDL_PollEvent(&event) == 1) {
            if (event.type == SDL_QUIT) {
                return;
            }
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
