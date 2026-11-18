#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>

#include "data_structures/Context.hpp"
#include "neuronide.pb.h"
#include "scene/Scene.hpp"
#include "scene/SceneObject.hpp"
#include "scene/components/LifetimeComponent.hpp"
#include "utils/ParserTestUtils.hpp"

namespace {
constexpr std::uint32_t kDurationMs  = 200;
constexpr double        kStepSeconds = 0.001;

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

// The component exists but does not count down yet: the owner is left alone.
TEST(LifetimeComponentTest, LeavesTheOwnerActiveWhileUpdated) {
    auto              owner = std::make_shared<SceneObject>("Cue");
    LifetimeComponent lifetime(owner, kDurationMs);

    lifetime.update(Context{kStepSeconds, nullptr});
    lifetime.render(nullptr);

    EXPECT_TRUE(owner->isActive);
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
