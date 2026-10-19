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
    Renderer() = delete;
    Renderer(const std::shared_ptr<Scene>& scene, std::shared_ptr<SDL_Renderer> sdlRenderer,
             std::shared_ptr<moodycamel::ConcurrentQueue<Marker>> markerQueue);
    ~Renderer() = default;

    Renderer(const Renderer&)            = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&)                 = delete;
    Renderer& operator=(Renderer&&)      = delete;

    void render(const std::stop_token& stoken);

    // Handles every pending window event and returns true when the operator asked to close the
    // window. Call it whenever events are polled outside render(), so a device reset is not lost.
    bool pollEvents();

   private:
    void prepareScene();

    struct SDLWindowDeleter {
        void operator()(SDL_Window* window) const;
    };
    std::unique_ptr<SDL_Window, SDLWindowDeleter>        window;
    std::shared_ptr<SDL_Renderer>                        sdlRenderer;
    std::weak_ptr<Scene>                                 currentScene;
    std::shared_ptr<moodycamel::ConcurrentQueue<Marker>> markerQueue;
    std::jthread                                         renderThread;
};

#endif  // RENDERER_HPP