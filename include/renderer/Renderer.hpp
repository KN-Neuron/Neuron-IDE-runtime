#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <SDL2/SDL.h>
#include <concurrentqueue.h>

#include <memory>
#include <thread>

class Scene;
struct Marker;

class Renderer {
   public:
    Renderer()  = default;
    ~Renderer() = default;

    void render(const std::stop_token& stoken);

   private:
    std::unique_ptr<SDL_Window>                          window;
    std::shared_ptr<SDL_Renderer>                        sdlRenderer;
    std::weak_ptr<Scene>                                 currentScene;
    std::shared_ptr<moodycamel::ConcurrentQueue<Marker>> markerQueue;
    std::jthread                                         renderThread;
};

#endif  // RENDERER_HPP