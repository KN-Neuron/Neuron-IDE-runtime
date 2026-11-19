#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "data_structures/Context.hpp"
#include "events/EventListener.hpp"
#include "neuronide.pb.h"
#include "scene/Scene.hpp"
#include "scene/SceneObject.hpp"
#include "scene/components/BlinkComponent.hpp"
#include "scene/components/LifetimeComponent.hpp"
#include "utils/ParserTestUtils.hpp"

namespace {
constexpr std::uint32_t kDurationMs     = 200;
constexpr std::uint32_t kHalfSecondMs   = 500;
constexpr std::uint32_t kOneSecondMs    = 1000;
constexpr double        kStepSeconds    = 0.001;
constexpr double        kQuarterSecond  = 0.25;
constexpr double        kHalfSecond     = 0.5;
constexpr double        kLongFrame      = 2.0;
constexpr double        kBlinkFrequency = 1.0;

// Builds a component only to see whether the constructor accepts the arguments.
void construct(const std::shared_ptr<SceneObject>& owner, std::uint32_t milliseconds) {
    const LifetimeComponent lifetime(owner, milliseconds);
    (void)lifetime;
}

// What the std::invalid_argument thrown by the constructor says, or an empty string if it does not
// throw.
std::string invalidArgumentMessage(const std::shared_ptr<SceneObject>& owner,
                                   std::uint32_t                       milliseconds) {
    try {
        construct(owner, milliseconds);
    } catch (const std::invalid_argument& error) {
        return error.what();
    }
    return {};
}

// A scene with one object called "Cue" that has a Lifetime component of `durationMs`.
NeuronIDE::Scene sceneWithLifetime(std::uint32_t durationMs) {
    NeuronIDE::Scene scene;
    scene.set_project_name("LifetimeTest");
    auto* object = scene.add_scene_objects();
    object->set_name("Cue");
    object->set_is_visible(true);
    object->add_components()->mutable_lifetime()->set_duration_ms(durationMs);
    return scene;
}

// The scene above plus an object "Markers" that emits "cue_expired" on the cue's "expire" and
// "cue_hidden" on its "deactivate".
NeuronIDE::Scene sceneWithMarkersOnTheCue(std::uint32_t durationMs) {
    NeuronIDE::Scene scene        = sceneWithLifetime(durationMs);
    auto*            markerObject = scene.add_scene_objects();
    markerObject->set_name("Markers");
    markerObject->set_is_visible(true);
    auto* emitter  = markerObject->add_components()->mutable_marker_emitter();
    auto* onExpire = emitter->add_bindings();
    onExpire->set_target_object("Cue");
    onExpire->set_target_event("expire");
    onExpire->set_marker_name("cue_expired");
    auto* onHide = emitter->add_bindings();
    onHide->set_target_object("Cue");
    onHide->set_target_event("deactivate");
    onHide->set_marker_name("cue_hidden");
    return scene;
}

// Counts how often the event it is subscribed to fires.
class CountingListener : public EventListener {
   public:
    void onEventTriggered(const Context& context) override {
        (void)context;
        ++count;
    }

    [[nodiscard]] int triggered() const { return count; }

   private:
    int count = 0;
};
}  // namespace

TEST(LifetimeComponentTest, KeepsTheDuration) {
    auto                    owner = std::make_shared<SceneObject>("Cue");
    const LifetimeComponent lifetime(owner, kDurationMs);

    EXPECT_EQ(lifetime.durationMs(), kDurationMs);
}

TEST(LifetimeComponentTest, AcceptsTheShortestAndTheLongestDuration) {
    auto owner = std::make_shared<SceneObject>("Cue");

    const LifetimeComponent shortest(owner, 1);
    const LifetimeComponent longest(owner, LifetimeComponent::kMaxDurationMs);

    EXPECT_EQ(shortest.durationMs(), 1U);
    EXPECT_EQ(longest.durationMs(), LifetimeComponent::kMaxDurationMs);
}

// A duration of 0 would hide the object before it was ever shown, which looks like a mistake in the
// experiment rather than something asked for.
TEST(LifetimeComponentTest, RejectsADurationOfZeroAndNamesTheObject) {
    auto owner = std::make_shared<SceneObject>("Cue");

    const std::string message = invalidArgumentMessage(owner, 0);

    EXPECT_NE(message.find("'Cue'"), std::string::npos) << message;
    EXPECT_NE(message.find("duration_ms"), std::string::npos) << message;
}

TEST(LifetimeComponentTest, RejectsADurationAboveTheLimit) {
    auto owner = std::make_shared<SceneObject>("Cue");

    EXPECT_THROW(construct(owner, LifetimeComponent::kMaxDurationMs + 1), std::invalid_argument);
}

TEST(LifetimeComponentTest, RejectsANullOwner) {
    EXPECT_THROW(construct(nullptr, kDurationMs), std::invalid_argument);
}

// A frame far shorter than the duration leaves the owner alone. Counts are in binary-exact seconds
// below (0.25, 0.5), so the sums are exact and the tests do not depend on rounding.
TEST(LifetimeComponentTest, LeavesTheOwnerActiveBeforeTheDurationHasPassed) {
    auto              owner = std::make_shared<SceneObject>("Cue");
    LifetimeComponent lifetime(owner, kDurationMs);

    lifetime.update(Context{kStepSeconds, nullptr});
    lifetime.render(nullptr);

    EXPECT_TRUE(owner->isActive);
}

TEST(LifetimeComponentTest, HidesTheOwnerOnceTheDurationHasPassed) {
    auto              owner = std::make_shared<SceneObject>("Cue");
    LifetimeComponent lifetime(owner, kHalfSecondMs);

    lifetime.update(Context{kQuarterSecond, nullptr});
    EXPECT_TRUE(owner->isActive);

    lifetime.update(Context{kQuarterSecond, nullptr});
    EXPECT_FALSE(owner->isActive);
}

// The first update counts too: an object active from the start does not wait a frame.
TEST(LifetimeComponentTest, CountsFromTheFirstFrame) {
    auto              owner = std::make_shared<SceneObject>("Cue");
    LifetimeComponent lifetime(owner, kHalfSecondMs);

    lifetime.update(Context{kHalfSecond, nullptr});

    EXPECT_FALSE(owner->isActive);
}

// One frame can be far longer than the duration, e.g. after a stall.
TEST(LifetimeComponentTest, HidesTheOwnerWhenOneFrameOvershootsTheDuration) {
    auto              owner = std::make_shared<SceneObject>("Cue");
    LifetimeComponent lifetime(owner, kHalfSecondMs);

    lifetime.update(Context{kLongFrame, nullptr});

    EXPECT_FALSE(owner->isActive);
}

// Lifetime only hides: an owner that is already inactive is never turned back on.
TEST(LifetimeComponentTest, NeverActivatesTheOwner) {
    auto              owner = std::make_shared<SceneObject>("Cue", false);
    LifetimeComponent lifetime(owner, kHalfSecondMs);

    lifetime.update(Context{kQuarterSecond, nullptr});
    lifetime.update(Context{kLongFrame, nullptr});

    EXPECT_FALSE(owner->isActive);
}

// After hiding the object the countdown starts over, so an object activated again gets the full
// duration, not what was left over.
TEST(LifetimeComponentTest, GivesTheFullDurationToAnObjectActivatedAgain) {
    auto              owner = std::make_shared<SceneObject>("Cue");
    LifetimeComponent lifetime(owner, kHalfSecondMs);
    lifetime.update(Context{kHalfSecond, nullptr});
    ASSERT_FALSE(owner->isActive);

    owner->isActive = true;
    lifetime.update(Context{kQuarterSecond, nullptr});
    EXPECT_TRUE(owner->isActive);

    lifetime.update(Context{kQuarterSecond, nullptr});
    EXPECT_FALSE(owner->isActive);
}

// A blinker only vetoes drawing; the object stays active and keeps being updated, so Lifetime keeps
// counting while the blinker is unlit.
TEST(LifetimeComponentTest, ABlinkerOnTheSameObjectDoesNotPauseTheCountdown) {
    auto object = std::make_shared<SceneObject>("Cue");
    object->addComponent(std::make_unique<BlinkComponent>(object, kBlinkFrequency));
    object->addComponent(std::make_unique<LifetimeComponent>(object, kOneSecondMs));
    const auto* blinker = object->findComponent<BlinkComponent>();

    // 0.75 s into a 1 Hz blink the sine is at its trough, so the blinker is unlit.
    for (int frame = 0; frame < 3; ++frame) {
        object->update(Context{kQuarterSecond, nullptr});
    }
    EXPECT_FALSE(blinker->allowsDrawing());
    EXPECT_TRUE(object->isActive);

    object->update(Context{kQuarterSecond, nullptr});
    EXPECT_FALSE(object->isActive);
}

TEST(LifetimeComponentTest, DoesNothingWhenItsOwnerIsGone) {
    std::unique_ptr<LifetimeComponent> lifetime;
    {
        auto owner = std::make_shared<SceneObject>("Cue");
        lifetime   = std::make_unique<LifetimeComponent>(owner, kHalfSecondMs);
    }

    EXPECT_NO_THROW(lifetime->update(Context{kLongFrame, nullptr}));
}

TEST(LifetimeComponentParseTest, BuildsTheComponentFromTheExperimentFile) {
    const auto scene = utils::parseProtoScene(sceneWithLifetime(kDurationMs));

    const auto* lifetime = scene->getObjects().at(0)->findComponent<LifetimeComponent>();

    ASSERT_NE(lifetime, nullptr);
    EXPECT_EQ(lifetime->durationMs(), kDurationMs);
}

TEST(LifetimeComponentParseTest, RejectsAnExperimentWithADurationOfZero) {
    EXPECT_THROW(utils::parseProtoScene(sceneWithLifetime(0)), std::invalid_argument);
}

// proto3 gives a field that was never set the value 0, so an empty Lifetime message is rejected
// too.
TEST(LifetimeComponentParseTest, RejectsALifetimeWithoutADuration) {
    NeuronIDE::Scene scene = sceneWithLifetime(kDurationMs);
    scene.mutable_scene_objects(0)->mutable_components(0)->mutable_lifetime()->clear_duration_ms();

    EXPECT_THROW(utils::parseProtoScene(scene), std::invalid_argument);
}

// "expire" is the only event Lifetime has. "deactivate" belongs to the object itself.
TEST(LifetimeComponentEventTest, ExposesOnlyTheExpireEvent) {
    auto              owner = std::make_shared<SceneObject>("Cue");
    LifetimeComponent lifetime(owner, kHalfSecondMs);

    EXPECT_NE(lifetime.getEvent("expire"), nullptr);
    EXPECT_EQ(lifetime.getEvent("deactivate"), nullptr);
    EXPECT_EQ(lifetime.getEvent("anything else"), nullptr);
}

TEST(LifetimeComponentEventTest, FiresExpireInTheFrameItHidesTheObject) {
    auto              owner = std::make_shared<SceneObject>("Cue");
    LifetimeComponent lifetime(owner, kHalfSecondMs);
    CountingListener  listener;
    const auto        subscription = lifetime.getEvent("expire")->subscribe(&listener);

    lifetime.update(Context{kQuarterSecond, nullptr});
    EXPECT_EQ(listener.triggered(), 0);

    lifetime.update(Context{kQuarterSecond, nullptr});
    EXPECT_EQ(listener.triggered(), 1);
}

// Each timeout fires once, and the countdown starts over after it.
TEST(LifetimeComponentEventTest, FiresOncePerTimeout) {
    auto              owner = std::make_shared<SceneObject>("Cue");
    LifetimeComponent lifetime(owner, kHalfSecondMs);
    CountingListener  listener;
    const auto        subscription = lifetime.getEvent("expire")->subscribe(&listener);

    lifetime.update(Context{kHalfSecond, nullptr});
    EXPECT_EQ(listener.triggered(), 1);

    lifetime.update(Context{kQuarterSecond, nullptr});
    EXPECT_EQ(listener.triggered(), 1);

    lifetime.update(Context{kQuarterSecond, nullptr});
    EXPECT_EQ(listener.triggered(), 2);
}

// Through a scene: "expire" and the object's own "deactivate" both land in the markers of the frame
// in which Lifetime hides the object.
TEST(LifetimeComponentEventTest, MarksATimeoutTogetherWithTheDeactivation) {
    const auto scene = utils::parseProtoScene(sceneWithMarkersOnTheCue(kHalfSecondMs));
    std::vector<std::string> markers;

    scene->update(Context{kQuarterSecond, &markers});
    EXPECT_TRUE(markers.empty());

    scene->update(Context{kQuarterSecond, &markers});
    std::ranges::sort(markers);
    EXPECT_EQ(markers, (std::vector<std::string>{"cue_expired", "cue_hidden"}));
}

// If something else hides the object first, only "deactivate" fires: a marker on "expire" tells a
// timeout from an early hide.
TEST(LifetimeComponentEventTest, DoesNotFireExpireWhenSomethingElseHidesTheObjectFirst) {
    const auto scene = utils::parseProtoScene(sceneWithMarkersOnTheCue(kHalfSecondMs));
    std::vector<std::string> markers;

    scene->update(Context{kQuarterSecond, &markers});
    scene->getObjects().at(0)->isActive = false;
    scene->update(Context{kLongFrame, &markers});

    EXPECT_EQ(markers, (std::vector<std::string>{"cue_hidden"}));
}
