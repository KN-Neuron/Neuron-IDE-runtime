#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "data_structures/Context.hpp"
#include "events/MarkerEventLink.hpp"
#include "events/Subscription.hpp"
#include "scene/Scene.hpp"
#include "scene/SceneObject.hpp"
#include "scene/components/BlinkComponent.hpp"
#include "scene/components/Component.hpp"

namespace {
constexpr double kBlinkFrequencyHz = 1.0;
constexpr double kHalfSecondStep   = 0.5;
constexpr double kStep             = 0.05;

// Counts how often it is updated and drawn, and can veto drawing.
class SpyComponent : public Component {
   public:
    SpyComponent(const std::shared_ptr<SceneObject>& owner, bool allowDrawing)
        : Component(owner), allowDrawing(allowDrawing) {}

    void update(const Context& context) override {
        (void)context;
        ++updateCalls;
    }
    void render(SDL_Renderer* renderer) override {
        (void)renderer;
        ++renderCalls;
    }
    bool allowsDrawing() const override { return allowDrawing; }

    [[nodiscard]] int updates() const { return updateCalls; }
    [[nodiscard]] int renders() const { return renderCalls; }

   private:
    bool allowDrawing;
    int  updateCalls = 0;
    int  renderCalls = 0;
};

// Adds a spy to `object` and returns a pointer to it; the object keeps ownership.
SpyComponent* addSpy(const std::shared_ptr<SceneObject>& object, bool allowDrawing) {
    auto  spy = std::make_unique<SpyComponent>(object, allowDrawing);
    auto* raw = spy.get();
    object->addComponent(std::move(spy));
    return raw;
}

using Markers = std::vector<std::string>;

// Turns one of an object's events into the marker "<object> <event>" while it lives.
class EventMarker {
   public:
    EventMarker(SceneObject& object, std::string_view event)
        : link(object.name + " " + std::string(event)),
          subscription(object.getEvent(event)->subscribe(&link)) {}

   private:
    MarkerEventLink link;
    Subscription    subscription;
};

// Sets another object's isActive on every update. The scene keeps both objects alive.
class SwitchComponent : public Component {
   public:
    SwitchComponent(const std::shared_ptr<SceneObject>& owner, SceneObject& target, bool active)
        : Component(owner), target(&target), active(active) {}

    void update(const Context& context) override {
        (void)context;
        target->isActive = active;
    }
    void render(SDL_Renderer* renderer) override { (void)renderer; }

   private:
    SceneObject* target;
    bool         active;
};

// Runs one frame of `scene` and returns its markers.
Markers frame(Scene& scene) {
    Markers markers;
    scene.update(Context{kStep, &markers});
    return markers;
}

// An object called "Obj" whose activate and deactivate events become markers.
class SceneObjectEventsTest : public ::testing::Test {
   protected:
    SceneObject& object() { return obj; }

    // Dispatches the object's change since the last dispatch and returns the events it fired.
    Markers dispatch() {
        Markers markers;
        obj.dispatchActivateStatusChange(Context{kStep, &markers});
        return markers;
    }

   private:
    SceneObject obj{"Obj"};
    EventMarker activated{obj, "activate"};
    EventMarker deactivated{obj, "deactivate"};
};
}  // namespace

TEST(SceneObjectTest, RendersComponentsWhenAllAllowDrawing) {
    auto  object = std::make_shared<SceneObject>("Obj");
    auto* first  = addSpy(object, true);
    auto* second = addSpy(object, true);

    object->render(nullptr);

    EXPECT_EQ(first->renders(), 1);
    EXPECT_EQ(second->renders(), 1);
}

// One component saying "do not draw" is enough to skip the whole object, including components that
// would have allowed it.
TEST(SceneObjectTest, SkipsRenderWhenAnyComponentVetoesDrawing) {
    auto  object   = std::make_shared<SceneObject>("Obj");
    auto* allowing = addSpy(object, true);
    auto* vetoing  = addSpy(object, false);

    object->render(nullptr);

    EXPECT_EQ(allowing->renders(), 0);
    EXPECT_EQ(vetoing->renders(), 0);
}

// A veto only affects drawing: the vetoing component and its siblings keep getting update().
TEST(SceneObjectTest, VetoedObjectIsStillUpdated) {
    auto  object  = std::make_shared<SceneObject>("Obj");
    auto* vetoing = addSpy(object, false);

    object->update(Context{kStep, nullptr});

    EXPECT_EQ(vetoing->updates(), 1);
}

// End to end with a real blinker: the object is drawn in the lit half of the cycle and skipped in
// the unlit half, while its own isActive flag is never touched.
TEST(SceneObjectTest, BlinkerHidesObjectOnlyThroughAllowsDrawing) {
    auto object = std::make_shared<SceneObject>("Obj");
    object->addComponent(std::make_unique<BlinkComponent>(object, kBlinkFrequencyHz));
    auto* spy = addSpy(object, true);

    object->update(Context{kStep, nullptr});
    object->render(nullptr);
    EXPECT_EQ(spy->renders(), 1);

    object->update(Context{kHalfSecondStep, nullptr});
    object->render(nullptr);
    EXPECT_EQ(spy->renders(), 1);

    EXPECT_TRUE(object->isActive);
}

// An inactive object is out of the scene: nothing on it is updated or drawn.
TEST(SceneObjectTest, InactiveObjectSkipsUpdateAndRender) {
    auto  object = std::make_shared<SceneObject>("Obj", false);
    auto* spy    = addSpy(object, true);

    object->update(Context{kStep, nullptr});
    object->render(nullptr);

    EXPECT_EQ(spy->updates(), 0);
    EXPECT_EQ(spy->renders(), 0);
}

// Activating the object again brings its components back.
TEST(SceneObjectTest, ReactivatedObjectUpdatesAndRendersAgain) {
    auto  object = std::make_shared<SceneObject>("Obj", false);
    auto* spy    = addSpy(object, true);

    object->isActive = true;
    object->update(Context{kStep, nullptr});
    object->render(nullptr);

    EXPECT_EQ(spy->updates(), 1);
    EXPECT_EQ(spy->renders(), 1);
}

TEST(SceneObjectTest, HasOnlyTheActivateAndDeactivateEvents) {
    SceneObject object("Obj");

    EXPECT_NE(object.getEvent("activate"), nullptr);
    EXPECT_NE(object.getEvent("deactivate"), nullptr);
    EXPECT_NE(object.getEvent("activate"), object.getEvent("deactivate"));
    EXPECT_EQ(object.getEvent("appear"), nullptr);
}

// Nothing is on screen before the first frame, so an object shown in it has just been activated.
TEST_F(SceneObjectEventsTest, FiresActivateInTheFirstFrameForAnObjectActiveFromTheStart) {
    EXPECT_EQ(dispatch(), Markers{"Obj activate"});
}

TEST_F(SceneObjectEventsTest, FiresNothingInTheFirstFrameForAnObjectInactiveFromTheStart) {
    object().isActive = false;

    EXPECT_TRUE(dispatch().empty());
}

TEST_F(SceneObjectEventsTest, FiresDeactivateWhenTheObjectIsDeactivated) {
    dispatch();
    object().isActive = false;

    EXPECT_EQ(dispatch(), Markers{"Obj deactivate"});
}

TEST_F(SceneObjectEventsTest, FiresActivateWhenTheObjectIsActivatedAgain) {
    dispatch();
    object().isActive = false;
    dispatch();
    object().isActive = true;

    EXPECT_EQ(dispatch(), Markers{"Obj activate"});
}

TEST_F(SceneObjectEventsTest, FiresEachChangeOnce) {
    dispatch();

    EXPECT_TRUE(dispatch().empty());
}

// The screen never showed the object hidden, so there is nothing to mark.
TEST_F(SceneObjectEventsTest, FiresNothingForAChangeUndoneBeforeTheDispatch) {
    dispatch();
    object().isActive = false;
    object().isActive = true;

    EXPECT_TRUE(dispatch().empty());
}

// Events fire after every object is updated, so a change made by an object later in the scene
// still goes with the frame that first shows it.
TEST(SceneUpdateTest, FiresAChangeMadeLaterInTheFrameInThatFrame) {
    const auto scene      = std::make_shared<Scene>();
    const auto stimulus   = std::make_shared<SceneObject>("Stimulus", false);
    const auto experiment = std::make_shared<SceneObject>("Experiment");
    experiment->addComponent(std::make_unique<SwitchComponent>(experiment, *stimulus, true));
    scene->addObject(stimulus);
    scene->addObject(experiment);
    const EventMarker activated(*stimulus, "activate");

    EXPECT_EQ(frame(*scene), Markers{"Stimulus activate"});
}

// An inactive object is not updated, but its deactivation still has to fire.
TEST(SceneUpdateTest, FiresDeactivateForAnObjectNoLongerUpdated) {
    const auto scene    = std::make_shared<Scene>();
    const auto stimulus = std::make_shared<SceneObject>("Stimulus");
    scene->addObject(stimulus);
    const EventMarker deactivated(*stimulus, "deactivate");
    frame(*scene);

    stimulus->isActive = false;

    EXPECT_EQ(frame(*scene), Markers{"Stimulus deactivate"});
}
