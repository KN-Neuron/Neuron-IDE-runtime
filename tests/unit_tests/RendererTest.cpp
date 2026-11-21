#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

#include "data_structures/Context.hpp"
#include "data_structures/KeyPress.hpp"
#include "data_structures/Marker.hpp"
#include "lsl_cpp.h"
#include "renderer/Renderer.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneObject.hpp"
#include "scene/components/Component.hpp"
#include "utils/SdlTestUtils.hpp"

namespace {
constexpr int  kCanvasSize     = 10;
constexpr auto kRenderSpinWait = std::chrono::milliseconds(30);
// SDL stamps events in whole milliseconds, so a press can read up to one tick early.
constexpr double kSdlTickSeconds = 0.001;

void pushKeyDown(SDL_Scancode key, bool repeat = false) {
    SDL_Event event{};
    event.type                = SDL_KEYDOWN;
    event.key.keysym.scancode = key;
    event.key.repeat          = repeat ? 1 : 0;
    ASSERT_EQ(SDL_PushEvent(&event), 1);
}

std::vector<Marker> drain(moodycamel::ConcurrentQueue<Marker>& queue) {
    std::vector<Marker> markers;
    Marker              marker;
    while (queue.try_dequeue(marker)) {
        markers.push_back(marker);
    }
    return markers;
}

// Records the key presses of each frame it is updated in, then hands the frame to `onFrame`.
class FrameProbe : public Component {
   public:
    FrameProbe(const std::shared_ptr<SceneObject>& owner,
               std::vector<std::vector<KeyPress>>* frames,
               std::function<void(const Context&)> onFrame)
        : Component(owner), frames(frames), onFrame(std::move(onFrame)) {}

    void update(const Context& context) override {
        frames->emplace_back(context.keyPresses.begin(), context.keyPresses.end());
        onFrame(context);
    }
    void render(SDL_Renderer* renderer) override { (void)renderer; }

   private:
    std::vector<std::vector<KeyPress>>* frames;
    std::function<void(const Context&)> onFrame;
};

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

// Fails to draw, like a sprite whose texture SDL cannot copy.
class FailingDrawComponent : public Component {
   public:
    explicit FailingDrawComponent(const std::shared_ptr<SceneObject>& owner) : Component(owner) {}

    void update(const Context& context) override { (void)context; }
    void render(SDL_Renderer* renderer) override {
        (void)renderer;
        throw std::runtime_error("cannot draw");
    }
};

// Records every render target it is handed.
class RenderTargetSpy : public Component {
   public:
    RenderTargetSpy(const std::shared_ptr<SceneObject>&       owner,
                    std::vector<std::weak_ptr<SDL_Renderer>>* received)
        : Component(owner), received(received) {}

    void onRenderTargetReady(const std::shared_ptr<SDL_Renderer>& target) override {
        received->push_back(target);
    }
    void update(const Context& context) override { (void)context; }
    void render(SDL_Renderer* renderer) override { (void)renderer; }

   private:
    std::vector<std::weak_ptr<SDL_Renderer>>* received;
};

class RendererTest : public ::testing::Test {
   protected:
    void SetUp() override { ASSERT_EQ(SDL_Init(SDL_INIT_EVENTS), 0); }
    void TearDown() override { SDL_Quit(); }

    static std::shared_ptr<SDL_Renderer> makeSoftwareRenderer() {
        return utils::makeSoftwareTarget(kCanvasSize, kCanvasSize);
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

TEST_F(RendererTest, Constructor_HandsRenderTargetToComponentsBeforeFirstFrame) {
    auto                                     scene = std::make_shared<Scene>();
    auto                                     obj   = std::make_shared<SceneObject>("obj");
    std::vector<std::weak_ptr<SDL_Renderer>> received;

    obj->addComponent(std::make_unique<RenderTargetSpy>(obj, &received));
    scene->addObject(obj);

    const auto     target = makeSoftwareRenderer();
    const Renderer renderer(scene, target, std::make_shared<moodycamel::ConcurrentQueue<Marker>>());

    ASSERT_EQ(received.size(), 1U);
    EXPECT_EQ(received.front().lock(), target);
}

TEST_F(RendererTest, PollEvents_OnDeviceReset_HandsRenderTargetToComponentsAgain) {
    auto                                     scene = std::make_shared<Scene>();
    auto                                     obj   = std::make_shared<SceneObject>("obj");
    std::vector<std::weak_ptr<SDL_Renderer>> received;

    obj->addComponent(std::make_unique<RenderTargetSpy>(obj, &received));
    scene->addObject(obj);

    const auto target = makeSoftwareRenderer();
    Renderer   renderer(scene, target, std::make_shared<moodycamel::ConcurrentQueue<Marker>>());

    SDL_Event reset{};
    reset.type = SDL_RENDER_DEVICE_RESET;
    ASSERT_EQ(SDL_PushEvent(&reset), 1);

    EXPECT_FALSE(renderer.pollEvents());
    ASSERT_EQ(received.size(), 2U);
    EXPECT_EQ(received.back().lock(), target);
}

TEST_F(RendererTest, RenderLoop_WhenDrawingFails_ThrowsWithoutQueueingTheFrameMarkers) {
    auto scene       = std::make_shared<Scene>();
    auto obj         = std::make_shared<SceneObject>("obj");
    auto stop_source = std::make_shared<std::stop_source>();

    obj->addComponent(std::make_unique<MarkerComponent>(obj, stop_source));
    obj->addComponent(std::make_unique<FailingDrawComponent>(obj));
    scene->addObject(obj);

    auto     markerQueue = std::make_shared<moodycamel::ConcurrentQueue<Marker>>();
    Renderer renderer(scene, makeSoftwareRenderer(), markerQueue);

    EXPECT_THROW(renderer.render(stop_source->get_token()), std::runtime_error);

    Marker marker;
    EXPECT_FALSE(markerQueue->try_dequeue(marker));
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

TEST_F(RendererTest, RenderLoop_PassesKeyPressesToTheSceneStampedOnTheLslClock) {
    auto                               scene = std::make_shared<Scene>();
    auto                               obj   = std::make_shared<SceneObject>("obj");
    std::stop_source                   stopSource;
    std::vector<std::vector<KeyPress>> frames;

    obj->addComponent(std::make_unique<FrameProbe>(
        obj, &frames, [&stopSource](const Context&) { stopSource.request_stop(); }));
    scene->addObject(obj);

    const double before = lsl::local_clock();
    pushKeyDown(SDL_SCANCODE_SPACE);
    pushKeyDown(SDL_SCANCODE_SPACE, /*repeat=*/true);  // holding a key down is not a new press
    pushKeyDown(SDL_SCANCODE_A);

    Renderer renderer(scene, makeSoftwareRenderer(),
                      std::make_shared<moodycamel::ConcurrentQueue<Marker>>());
    renderer.render(stopSource.get_token());

    ASSERT_EQ(frames.size(), 1U);
    const auto& presses = frames.front();
    ASSERT_EQ(presses.size(), 2U);
    EXPECT_EQ(presses[0].key, SDL_SCANCODE_SPACE);
    EXPECT_EQ(presses[1].key, SDL_SCANCODE_A);
    EXPECT_GE(presses[0].timestamp, before - kSdlTickSeconds);
    EXPECT_LE(presses[1].timestamp, lsl::local_clock());
}

TEST_F(RendererTest, RenderLoop_PassesEachKeyPressToOneFrameOnly) {
    auto                               scene = std::make_shared<Scene>();
    auto                               obj   = std::make_shared<SceneObject>("obj");
    std::stop_source                   stopSource;
    std::vector<std::vector<KeyPress>> frames;

    obj->addComponent(
        std::make_unique<FrameProbe>(obj, &frames, [&stopSource, &frames](const Context&) {
            if (frames.size() == 2) {
                stopSource.request_stop();
            }
        }));
    scene->addObject(obj);

    pushKeyDown(SDL_SCANCODE_SPACE);

    Renderer renderer(scene, makeSoftwareRenderer(),
                      std::make_shared<moodycamel::ConcurrentQueue<Marker>>());
    renderer.render(stopSource.get_token());

    ASSERT_EQ(frames.size(), 2U);
    EXPECT_EQ(frames[0].size(), 1U);
    EXPECT_TRUE(frames[1].empty());
}

// The runtime polls events while it waits for the EEG stream, before anything is recorded.
TEST_F(RendererTest, PollEvents_KeyPressedBeforeRender_NeverReachesTheScene) {
    auto                               scene = std::make_shared<Scene>();
    auto                               obj   = std::make_shared<SceneObject>("obj");
    std::stop_source                   stopSource;
    std::vector<std::vector<KeyPress>> frames;

    obj->addComponent(std::make_unique<FrameProbe>(
        obj, &frames, [&stopSource](const Context&) { stopSource.request_stop(); }));
    scene->addObject(obj);

    Renderer renderer(scene, makeSoftwareRenderer(),
                      std::make_shared<moodycamel::ConcurrentQueue<Marker>>());
    pushKeyDown(SDL_SCANCODE_SPACE);
    EXPECT_FALSE(renderer.pollEvents());
    renderer.render(stopSource.get_token());

    ASSERT_EQ(frames.size(), 1U);
    EXPECT_TRUE(frames.front().empty());
}

TEST_F(RendererTest, RenderLoop_QueuesTimedMarkersWithTheirOwnTime) {
    constexpr double                   kKeyPressTime = 42.0;
    auto                               scene         = std::make_shared<Scene>();
    auto                               obj           = std::make_shared<SceneObject>("obj");
    std::stop_source                   stopSource;
    std::vector<std::vector<KeyPress>> frames;

    obj->addComponent(
        std::make_unique<FrameProbe>(obj, &frames, [&stopSource](const Context& context) {
            context.markers->push_back("flip");
            context.timedMarkers->push_back(Marker{"response", kKeyPressTime});
            stopSource.request_stop();
        }));
    scene->addObject(obj);

    auto         markerQueue = std::make_shared<moodycamel::ConcurrentQueue<Marker>>();
    Renderer     renderer(scene, makeSoftwareRenderer(), markerQueue);
    const double before = lsl::local_clock();
    renderer.render(stopSource.get_token());

    const auto markers = drain(*markerQueue);
    ASSERT_EQ(markers.size(), 2U);
    EXPECT_EQ(markers[0].eventName, "flip");
    EXPECT_GE(markers[0].timestamp, before);
    EXPECT_EQ(markers[1].eventName, "response");
    EXPECT_DOUBLE_EQ(markers[1].timestamp, kKeyPressTime);
}
