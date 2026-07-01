#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <stop_token>
#include <thread>

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
constexpr auto     kRenderSpinWait     = std::chrono::milliseconds(30);

class CustomComponent : public Component {
   public:
    CustomComponent(const std::shared_ptr<SceneObject>& owner,
                    std::shared_ptr<std::atomic<int>>   updates,
                    std::shared_ptr<std::atomic<int>>   renders,
                    std::shared_ptr<std::stop_source>   stopSource)
        : Component(owner),
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
    MarkerComponent(const std::shared_ptr<SceneObject>& owner,
                    std::shared_ptr<std::stop_source>   stopSource)
        : Component(owner), stopSource(std::move(stopSource)) {}

    void update(const Context& context) override {
        if (context.markers != nullptr) {
            context.markers->push_back("test_marker");
        }
        stopSource->request_stop();
    }

    void render(SDL_Renderer* renderer) override { (void)renderer; }

   private:
    std::shared_ptr<std::stop_source> stopSource;
};

class RendererTest : public ::testing::Test {
   protected:
    void SetUp() override { ASSERT_EQ(SDL_Init(SDL_INIT_EVENTS), 0); }
    void TearDown() override { SDL_Quit(); }

    static std::shared_ptr<SDL_Renderer> makeSoftwareRenderer() {
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
            kDummySurfaceFlags, kDummySurfaceWidth, kDummySurfaceHeight, kDummySurfaceDepth,
            SDL_PIXELFORMAT_RGBA32);
        SDL_Renderer* sdlRenderer = SDL_CreateSoftwareRenderer(surface);
        return std::shared_ptr<SDL_Renderer>(sdlRenderer, [surface](SDL_Renderer* renderer) {
            if (renderer != nullptr) {
                SDL_DestroyRenderer(renderer);
            }
            if (surface != nullptr) {
                SDL_FreeSurface(surface);
            }
        });
    }
};
}  // namespace

TEST_F(RendererTest, RenderLoop_WhenComponentAdded_CallsUpdateExactlyOnceBeforeStop) {
    auto scene = std::make_shared<Scene>();
    auto obj   = std::make_shared<SceneObject>("obj");

    auto updates     = std::make_shared<std::atomic<int>>(0);
    auto renders     = std::make_shared<std::atomic<int>>(0);
    auto stop_source = std::make_shared<std::stop_source>();

    obj->addComponent(std::make_unique<CustomComponent>(obj, updates, renders, stop_source));
    scene->addObject(obj);

    auto markerQueue = std::make_shared<moodycamel::ConcurrentQueue<Marker>>();

    Renderer renderer(scene, makeSoftwareRenderer(), markerQueue);
    renderer.render(stop_source->get_token());

    EXPECT_EQ(*updates, 1);
    EXPECT_EQ(*renders, 1);
}

TEST_F(RendererTest, RenderLoop_QueuesMarkersFromComponents) {
    auto scene       = std::make_shared<Scene>();
    auto obj         = std::make_shared<SceneObject>("obj");
    auto stop_source = std::make_shared<std::stop_source>();

    obj->addComponent(std::make_unique<MarkerComponent>(obj, stop_source));
    scene->addObject(obj);

    auto markerQueue = std::make_shared<moodycamel::ConcurrentQueue<Marker>>();

    Renderer renderer(scene, makeSoftwareRenderer(), markerQueue);
    renderer.render(stop_source->get_token());

    Marker marker;
    bool   dequeued = markerQueue->try_dequeue(marker);
    EXPECT_TRUE(dequeued);
    if (dequeued) {
        EXPECT_EQ(marker.eventName, "test_marker");
        EXPECT_FALSE(markerQueue->try_dequeue(marker));
    }
}

TEST_F(RendererTest, RenderLoop_OnQuitEvent_ReturnsBeforeUpdating) {
    auto scene       = std::make_shared<Scene>();
    auto obj         = std::make_shared<SceneObject>("obj");
    auto updates     = std::make_shared<std::atomic<int>>(0);
    auto renders     = std::make_shared<std::atomic<int>>(0);
    auto stop_source = std::make_shared<std::stop_source>();

    obj->addComponent(std::make_unique<CustomComponent>(obj, updates, renders, stop_source));
    scene->addObject(obj);

    auto markerQueue = std::make_shared<moodycamel::ConcurrentQueue<Marker>>();

    SDL_Event quit;
    quit.type = SDL_QUIT;
    ASSERT_EQ(SDL_PushEvent(&quit), 1);

    Renderer renderer(scene, makeSoftwareRenderer(), markerQueue);
    renderer.render(stop_source->get_token());

    EXPECT_EQ(*updates, 0);
    EXPECT_EQ(*renders, 0);
}

TEST_F(RendererTest, RenderLoop_OnNonQuitEvent_ContinuesUpdating) {
    auto scene       = std::make_shared<Scene>();
    auto obj         = std::make_shared<SceneObject>("obj");
    auto updates     = std::make_shared<std::atomic<int>>(0);
    auto renders     = std::make_shared<std::atomic<int>>(0);
    auto stop_source = std::make_shared<std::stop_source>();

    obj->addComponent(std::make_unique<CustomComponent>(obj, updates, renders, stop_source));
    scene->addObject(obj);

    auto markerQueue = std::make_shared<moodycamel::ConcurrentQueue<Marker>>();

    SDL_Event userEvent;
    userEvent.type = SDL_USEREVENT;
    ASSERT_EQ(SDL_PushEvent(&userEvent), 1);

    Renderer renderer(scene, makeSoftwareRenderer(), markerQueue);
    renderer.render(stop_source->get_token());

    EXPECT_EQ(*updates, 1);
    EXPECT_EQ(*renders, 1);
}

TEST_F(RendererTest, RenderLoop_WhenSceneExpired_KeepsRunningWithoutCrashing) {
    auto     scene       = std::make_shared<Scene>();
    auto     markerQueue = std::make_shared<moodycamel::ConcurrentQueue<Marker>>();
    Renderer renderer(scene, makeSoftwareRenderer(), markerQueue);

    scene.reset();

    std::stop_source stopSource;
    std::thread worker([&renderer, &stopSource]() { renderer.render(stopSource.get_token()); });
    std::this_thread::sleep_for(kRenderSpinWait);
    stopSource.request_stop();
    worker.join();

    Marker marker;
    EXPECT_FALSE(markerQueue->try_dequeue(marker));
}
