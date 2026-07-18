#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "data_structures/Context.hpp"
#include "events/Delegate.hpp"
#include "events/MarkerEventLink.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneObject.hpp"
#include "scene/components/Component.hpp"
#include "scene/components/MarkerEmitterComponent.hpp"

// -------------------------------------------------------------------
// Mock event listener for delegate tests
// -------------------------------------------------------------------
class MockListener : public EventListener {
   public:
    void onEventTriggered(const Context& ctx) override {
        (void)ctx;
        callCount++;
    }

    int callCount = 0;
};

// -------------------------------------------------------------------
// Test component that exposes a Delegate via getEvent()
// -------------------------------------------------------------------
class TestEventProvider : public Component {
   public:
    explicit TestEventProvider(const std::shared_ptr<SceneObject>& owner) : Component(owner) {}

    Delegate* getEvent(std::string_view eventName) override {
        if (eventName == "trigger") {
            return &testDelegate;
        }
        return nullptr;
    }

    void update(const Context& /*ctx*/) override {}
    void render(SDL_Renderer* /*renderer*/) override {}

    Delegate testDelegate;
};

// ===================================================================
// Delegate tests
// ===================================================================
TEST(DelegateTest, SubscribeInvokesListener) {
    Delegate     delegate;
    MockListener listener;

    delegate.subscribe(&listener);
    std::vector<std::string> markers;
    Context                  ctx{0.0, &markers};
    delegate.invoke(ctx);

    EXPECT_EQ(listener.callCount, 1);
}

TEST(DelegateTest, UnsubscribeRemovesListener) {
    Delegate     delegate;
    MockListener listener;

    delegate.subscribe(&listener);
    delegate.unsubscribe(&listener);
    std::vector<std::string> markers;
    Context                  ctx{0.0, &markers};
    delegate.invoke(ctx);

    EXPECT_EQ(listener.callCount, 0);
}

TEST(DelegateTest, MultipleListenersAllCalled) {
    Delegate     delegate;
    MockListener listener1;
    MockListener listener2;
    MockListener listener3;

    delegate.subscribe(&listener1);
    delegate.subscribe(&listener2);
    delegate.subscribe(&listener3);
    std::vector<std::string> markers;
    Context                  ctx{0.0, &markers};
    delegate.invoke(ctx);

    EXPECT_EQ(listener1.callCount, 1);
    EXPECT_EQ(listener2.callCount, 1);
    EXPECT_EQ(listener3.callCount, 1);
}

// ===================================================================
// MarkerEventLink tests
// ===================================================================
TEST(MarkerEventLinkTest, PushesMarkerToContext) {
    MarkerEventLink          link("test_event");
    std::vector<std::string> markers;
    Context                  ctx{0.0, &markers};

    link.onEventTriggered(ctx);

    ASSERT_EQ(markers.size(), 1);
    EXPECT_EQ(markers[0], "test_event");
}

TEST(MarkerEventLinkTest, NullMarkersNoCrash) {
    MarkerEventLink link("test_event");
    Context         ctx{0.0, nullptr};

    EXPECT_NO_THROW(link.onEventTriggered(ctx));
}

// ===================================================================
// Scene::resolveEvents tests
// ===================================================================
TEST(ResolveEventsTest, ReturnsDelegateFromComponent) {
    auto scene = std::make_shared<Scene>();
    auto obj   = std::make_shared<SceneObject>("Provider");
    auto comp  = std::make_unique<TestEventProvider>(obj);
    obj->addComponent(std::move(comp));
    scene->addObject(obj);

    auto& delegate = scene->resolveEvents("Provider", "trigger");
    // Should return a valid reference
    EXPECT_NO_THROW(delegate.invoke(Context{}));
}

TEST(ResolveEventsTest, ThrowsOnMissingObject) {
    auto scene = std::make_shared<Scene>();

    EXPECT_THROW(scene->resolveEvents("NoSuchObject", "trigger"), std::runtime_error);
}

TEST(ResolveEventsTest, ThrowsOnMissingEvent) {
    auto scene = std::make_shared<Scene>();
    auto obj   = std::make_shared<SceneObject>("Provider");
    auto comp  = std::make_unique<TestEventProvider>(obj);
    obj->addComponent(std::move(comp));
    scene->addObject(obj);

    EXPECT_THROW(scene->resolveEvents("Provider", "no_such_event"), std::runtime_error);
}

// ===================================================================
// MarkerEmitterComponent integration test
// ===================================================================
TEST(MarkerEmitterIntegration, FullFlow) {
    // Build scene with a provider and an emitter
    auto scene = std::make_shared<Scene>();

    auto  providerObj  = std::make_shared<SceneObject>("Provider");
    auto  providerComp = std::make_unique<TestEventProvider>(providerObj);
    auto* providerPtr  = providerComp.get();
    providerObj->addComponent(std::move(providerComp));
    scene->addObject(providerObj);

    auto emitterObj = std::make_shared<SceneObject>("Emitter");
    std::vector<MarkerEmitterComponent::Binding> bindings = {
        {"Provider", "trigger", "hello_world"}};
    emitterObj->addComponent(
        std::make_unique<MarkerEmitterComponent>(emitterObj, std::move(bindings)));
    scene->addObject(emitterObj);

    // Simulate onSceneReady
    scene->onSceneReady();

    // Fire the delegate
    std::vector<std::string> markers;
    Context                  ctx{0.0, &markers};
    providerPtr->testDelegate.invoke(ctx);

    ASSERT_EQ(markers.size(), 1);
    EXPECT_EQ(markers[0], "hello_world");
}
