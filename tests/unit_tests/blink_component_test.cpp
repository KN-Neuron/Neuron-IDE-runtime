#include <gtest/gtest.h>

#include <memory>

#include "data_structures/Context.hpp"
#include "scene/SceneObject.hpp"
#include "scene/components/BlinkComponent.hpp"

namespace {
constexpr double kFrequencyHz        = 1.0;
constexpr double kQuarterPeriod      = 0.25;
constexpr double kThreeQuarterPeriod = 0.75;
constexpr double kStep               = 0.05;
constexpr double kDoubledFrequencyHz = 2.0;
constexpr double kEighthPeriodStep   = 0.125;
constexpr double kQuarterSecondStep  = 0.25;
constexpr double kMultiPeriodStep    = 2.25;
constexpr double kHalfSecondStep     = 0.5;
}  // namespace

// TEST 1
// A frequency of 0 Hz disables blinking: after update() the owner must stay visible
TEST(BlinkComponentTest, StaysVisibleWhenFrequencyIsZero) {
    auto           owner = std::make_shared<SceneObject>("Blinker", true);
    BlinkComponent blink(owner, 0.0);

    Context ctx{kStep, nullptr};
    blink.update(ctx);

    EXPECT_TRUE(owner->isVisible);
}

// TEST 2
// If the owner is destroyed while the component
// is still alive, update() must detect the expired pointer and do nothing instead of crashing.
TEST(BlinkComponentTest, DoesNotCrashWhenOwnerExpired) {
    std::unique_ptr<BlinkComponent> blink;
    {
        auto owner = std::make_shared<SceneObject>("Blinker", true);
        blink      = std::make_unique<BlinkComponent>(owner, kFrequencyHz);
    }

    Context ctx{kStep, nullptr};
    EXPECT_NO_THROW(blink->update(ctx));
}

// TEST 3
TEST(BlinkComponentTest, TogglesVisibilityBasedOnAccumulatedTime) {
    auto           owner = std::make_shared<SceneObject>("Blinker", true);
    BlinkComponent blink(owner, kFrequencyHz);

    double elapsed = 0.0;
    while (elapsed < kQuarterPeriod) {
        Context ctx{kStep, nullptr};
        blink.update(ctx);
        elapsed += kStep;
    }
    EXPECT_TRUE(owner->isVisible);

    while (elapsed < kThreeQuarterPeriod) {
        Context ctx{kStep, nullptr};
        blink.update(ctx);
        elapsed += kStep;
    }
    EXPECT_FALSE(owner->isVisible);
}

// setFrequency() replaces the blink frequency: after switching from 1 Hz to 2 Hz the sine wave has
// to follow the new frequency. At 2 Hz the peak is at 0.125 s and the trough at 0.375 s.
TEST(BlinkComponentTest, SetFrequencyChangesBlinkRate) {
    auto           owner = std::make_shared<SceneObject>("Blinker", true);
    BlinkComponent blink(owner, kFrequencyHz);

    blink.setFrequency(kDoubledFrequencyHz);

    blink.update(Context{kEighthPeriodStep, nullptr});
    EXPECT_TRUE(owner->isVisible);

    blink.update(Context{kQuarterSecondStep, nullptr});
    EXPECT_FALSE(owner->isVisible);
}

// A single frame can be longer than the whole blink period. The visibility must still follow the
// phase of the accumulated time: 2.25 s at 1 Hz is a peak (2 full periods + 0.25), 2.75 s a trough.
TEST(BlinkComponentTest, HandlesTimeStepLongerThanOnePeriod) {
    auto           owner = std::make_shared<SceneObject>("Blinker", true);
    BlinkComponent blink(owner, kFrequencyHz);

    blink.update(Context{kMultiPeriodStep, nullptr});
    EXPECT_TRUE(owner->isVisible);

    blink.update(Context{kHalfSecondStep, nullptr});
    EXPECT_FALSE(owner->isVisible);
}