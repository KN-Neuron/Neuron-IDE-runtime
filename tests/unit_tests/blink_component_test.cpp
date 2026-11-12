#include <gtest/gtest.h>

#include <limits>
#include <memory>

#include "data_structures/Context.hpp"
#include "scene/SceneObject.hpp"
#include "scene/components/BlinkComponent.hpp"

namespace {
constexpr double kFrequencyHz        = 1.0;
constexpr double kDoubledFrequencyHz = 2.0;
constexpr double kQuarterPeriod      = 0.25;
constexpr double kThreeQuarterPeriod = 0.75;
constexpr double kStep               = 0.05;
constexpr double kEighthPeriodStep   = 0.125;
constexpr double kQuarterSecondStep  = 0.25;
constexpr double kHalfSecondStep     = 0.5;
constexpr double kMultiPeriodStep    = 2.25;
}  // namespace

// A new blinker starts lit, so the object is drawn from the very first frame.
TEST(BlinkComponentTest, StartsLit) {
    auto           owner = std::make_shared<SceneObject>("Blinker");
    BlinkComponent blink(owner, kFrequencyHz);

    EXPECT_TRUE(blink.allowsDrawing());
}

// A frequency of 0 Hz (or less) disables blinking: update() leaves the component lit.
TEST(BlinkComponentTest, StaysLitWhenFrequencyIsZero) {
    auto           owner = std::make_shared<SceneObject>("Blinker");
    BlinkComponent blink(owner, 0.0);

    blink.update(Context{kStep, nullptr});

    EXPECT_TRUE(blink.allowsDrawing());
}

// A frequency that is not a number or is infinite cannot be turned into a phase. It must not leave
// the object hidden for good: such a blinker does not blink and stays lit, like one at 0 Hz.
TEST(BlinkComponentTest, StaysLitWhenFrequencyIsNotFinite) {
    auto owner = std::make_shared<SceneObject>("Blinker");

    for (const double frequency :
         {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
        BlinkComponent blink(owner, frequency);

        blink.update(Context{kThreeQuarterPeriod, nullptr});

        EXPECT_TRUE(blink.allowsDrawing());
    }
}

// Switching blinking off while the component is unlit must light it again, so the object is not
// left hidden by a blinker that no longer blinks.
TEST(BlinkComponentTest, LightsUpWhenFrequencySetToZero) {
    auto           owner = std::make_shared<SceneObject>("Blinker");
    BlinkComponent blink(owner, kFrequencyHz);

    blink.update(Context{kThreeQuarterPeriod, nullptr});
    ASSERT_FALSE(blink.allowsDrawing());

    blink.setFrequency(0.0);
    EXPECT_TRUE(blink.allowsDrawing());

    blink.update(Context{kStep, nullptr});
    EXPECT_TRUE(blink.allowsDrawing());
}

// Context::timestamp is the per-frame delta, so the component has to accumulate it. At 1 Hz, after
// 0.25 s (sine peak) it is lit, after 0.75 s (sine trough) it is unlit. Fails if update() uses the
// raw delta instead of the accumulated time.
TEST(BlinkComponentTest, TogglesBasedOnAccumulatedTime) {
    auto           owner = std::make_shared<SceneObject>("Blinker");
    BlinkComponent blink(owner, kFrequencyHz);

    double elapsed = 0.0;
    while (elapsed < kQuarterPeriod) {
        blink.update(Context{kStep, nullptr});
        elapsed += kStep;
    }
    EXPECT_TRUE(blink.allowsDrawing());

    while (elapsed < kThreeQuarterPeriod) {
        blink.update(Context{kStep, nullptr});
        elapsed += kStep;
    }
    EXPECT_FALSE(blink.allowsDrawing());
}

// setFrequency() replaces the blink frequency: after switching from 1 Hz to 2 Hz the sine wave has
// to follow the new frequency. At 2 Hz the peak is at 0.125 s and the trough at 0.375 s.
TEST(BlinkComponentTest, SetFrequencyChangesBlinkRate) {
    auto           owner = std::make_shared<SceneObject>("Blinker");
    BlinkComponent blink(owner, kFrequencyHz);

    blink.setFrequency(kDoubledFrequencyHz);

    blink.update(Context{kEighthPeriodStep, nullptr});
    EXPECT_TRUE(blink.allowsDrawing());

    blink.update(Context{kQuarterSecondStep, nullptr});
    EXPECT_FALSE(blink.allowsDrawing());
}

// setFrequency() restarts the blink cycle. After running to the trough of the previous cycle
// (0.75 s at 1 Hz, unlit), changing the frequency starts from phase 0 again, so the blinker is lit
// right away and stays lit after a small step instead of continuing from the old phase.
TEST(BlinkComponentTest, SetFrequencyRestartsBlinkCycle) {
    auto           owner = std::make_shared<SceneObject>("Blinker");
    BlinkComponent blink(owner, kFrequencyHz);

    blink.update(Context{kThreeQuarterPeriod, nullptr});
    ASSERT_FALSE(blink.allowsDrawing());

    blink.setFrequency(kFrequencyHz);
    EXPECT_TRUE(blink.allowsDrawing());

    blink.update(Context{kEighthPeriodStep, nullptr});
    EXPECT_TRUE(blink.allowsDrawing());
}

// A single frame can be longer than the whole blink period. The state must still follow the phase
// of the accumulated time: 2.25 s at 1 Hz is a peak (2 full periods + 0.25), 2.75 s a trough.
TEST(BlinkComponentTest, HandlesTimeStepLongerThanOnePeriod) {
    auto           owner = std::make_shared<SceneObject>("Blinker");
    BlinkComponent blink(owner, kFrequencyHz);

    blink.update(Context{kMultiPeriodStep, nullptr});
    EXPECT_TRUE(blink.allowsDrawing());

    blink.update(Context{kHalfSecondStep, nullptr});
    EXPECT_FALSE(blink.allowsDrawing());
}

// The blinker keeps its state to itself: flickering must not write to the owner, otherwise the
// owner's own flag would change on every blink.
TEST(BlinkComponentTest, DoesNotTouchTheOwner) {
    auto           owner = std::make_shared<SceneObject>("Blinker");
    BlinkComponent blink(owner, kFrequencyHz);

    blink.update(Context{kThreeQuarterPeriod, nullptr});
    ASSERT_FALSE(blink.allowsDrawing());

    EXPECT_TRUE(owner->isActive);
}
