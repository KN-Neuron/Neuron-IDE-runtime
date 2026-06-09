#include <gtest/gtest.h>

#include <atomic>
#include <memory>
#include <stop_token>

#include "data_structures/Context.hpp"
#include "data_structures/Marker.hpp"
#include "renderer/Renderer.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneObject.hpp"
#include "scene/components/Component.hpp"

namespace {
constexpr int      kDummySurfaceWidth  = 10;
constexpr int      kDummySurfaceHeight = 10;
constexpr int      kDummySurfaceDepth  = 32;
constexpr uint32_t kDummySurfaceFlags  = 0;

class CustomComponent : public Component {
   public:
    CustomComponent(std::shared_ptr<SceneObject> owner, std::shared_ptr<std::atomic<int>> updates,
                    std::shared_ptr<std::atomic<int>> renders,
                    std::shared_ptr<std::stop_source> stopSource)
        : Component(std::move(owner)),
          updates(std::move(updates)),
          renders(std::move(renders)),
          stopSource(std::move(stopSource)) {}

    void update(const Context& context) override {
        (void)context;
        (*updates)++;

        stopSource->request_stop();
    }

    void render(SDL_Renderer* renderer) override {
        (void)renderer;
        (*renders)++;
    }

   private:
    std::shared_ptr<std::atomic<int>> updates;
    std::shared_ptr<std::atomic<int>> renders;
    std::shared_ptr<std::stop_source> stopSource;
};

class MarkerComponent : public Component {
   public:
    MarkerComponent(std::shared_ptr<SceneObject>      owner,
                    std::shared_ptr<std::stop_source> stopSource)
        : Component(std::move(owner)), stopSource(std::move(stopSource)) {}

    void update(const Context& context) override {
        context.markers.push_back("test_marker");
        stopSource->request_stop();
    }

    void render(SDL_Renderer* renderer) override { (void)renderer; }

   private:
    std::shared_ptr<std::stop_source> stopSource;
};

}  // namespace

TEST(RendererTest, RenderLoop_WhenComponentAdded_CallsUpdateExactlyOnceBeforeStop) {
    ASSERT_EQ(SDL_Init(SDL_INIT_EVENTS), 0);

    auto scene = std::make_shared<Scene>();
    auto obj   = std::make_shared<SceneObject>("obj");

    auto updates     = std::make_shared<std::atomic<int>>(0);
    auto renders     = std::make_shared<std::atomic<int>>(0);
    auto stop_source = std::make_shared<std::stop_source>();

    obj->addComponent(std::make_unique<CustomComponent>(obj, updates, renders, stop_source));
    scene->addObject(obj);

    SDL_Surface* surface =
        SDL_CreateRGBSurfaceWithFormat(kDummySurfaceFlags, kDummySurfaceWidth, kDummySurfaceHeight,
                                       kDummySurfaceDepth, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* sdlRenderer = SDL_CreateSoftwareRenderer(surface);
    auto sharedRenderer = std::shared_ptr<SDL_Renderer>(sdlRenderer, [surface](SDL_Renderer* r) {
        if (r) {
            SDL_DestroyRenderer(r);
        }
        if (surface) {
            SDL_FreeSurface(surface);
        }
    });

    auto markerQueue = std::make_shared<moodycamel::ConcurrentQueue<Marker>>();

    Renderer renderer(scene, sharedRenderer, markerQueue);
    renderer.render(stop_source->get_token());

    EXPECT_EQ(*updates, 1);
    EXPECT_EQ(*renders, 1);

    SDL_Quit();
}

TEST(RendererTest, RenderLoop_QueuesMarkersFromComponents) {
    ASSERT_EQ(SDL_Init(SDL_INIT_EVENTS), 0);

    auto scene       = std::make_shared<Scene>();
    auto obj         = std::make_shared<SceneObject>("obj");
    auto stop_source = std::make_shared<std::stop_source>();

    obj->addComponent(std::make_unique<MarkerComponent>(obj, stop_source));
    scene->addObject(obj);

    SDL_Surface* surface =
        SDL_CreateRGBSurfaceWithFormat(kDummySurfaceFlags, kDummySurfaceWidth, kDummySurfaceHeight,
                                       kDummySurfaceDepth, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* sdlRenderer = SDL_CreateSoftwareRenderer(surface);
    auto sharedRenderer = std::shared_ptr<SDL_Renderer>(sdlRenderer, [surface](SDL_Renderer* r) {
        if (r) {
            SDL_DestroyRenderer(r);
        }
        if (surface) {
            SDL_FreeSurface(surface);
        }
    });

    auto markerQueue = std::make_shared<moodycamel::ConcurrentQueue<Marker>>();

    Renderer renderer(scene, sharedRenderer, markerQueue);
    renderer.render(stop_source->get_token());

    Marker m;
    bool   dequeued = markerQueue->try_dequeue(m);
    EXPECT_TRUE(dequeued);
    if (dequeued) {
        EXPECT_EQ(m.name, "test_marker");
        EXPECT_FALSE(markerQueue->try_dequeue(m));
    }

    SDL_Quit();
}