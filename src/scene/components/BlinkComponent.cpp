#include "scene/components/BlinkComponent.hpp"

#include <cmath>
#include <logging/Logging.hpp>
#include <numbers>
#include <string>

#include "data_structures/Context.hpp"
#include "neuronide.pb.h"
#include "scene/SceneObject.hpp"
#include "scene/components/ComponentRegistry.hpp"

namespace {
constexpr double kTwoPi = 2.0 * std::numbers::pi;

quill::Logger* logger() { return logging::logger(logging::Subsystem::Scene); }

// Only a positive, finite frequency makes the component blink. sin() of a NaN or infinite phase
// is NaN, which would keep the component unlit for good.
bool blinks(double frequencyHz) { return std::isfinite(frequencyHz) && frequencyHz > 0.0; }

std::string describe(const std::shared_ptr<SceneObject>& owner) {
    return "BlinkComponent on '" + (owner ? owner->name : std::string()) + "'";
}
}  // namespace

void BlinkComponent::setFrequency(double freq) {
    blinkFrequencyHz = freq;
    elapsedTime      = 0.0;
    isLit            = true;
}

std::unique_ptr<Component> BlinkComponent::createBlinker(const NeuronIDE::Component& protoComp,
                                                         const std::shared_ptr<SceneObject>& owner,
                                                         const ComponentBuildContext& /*context*/) {
    const double frequencyHz = protoComp.blinker().blink_frequency_hz();
    if (blinks(frequencyHz)) {
        LOG_DEBUG(logger(), "{} blinks at {} Hz", describe(owner), frequencyHz);
    } else {
        LOG_WARNING(logger(), "{} has a frequency of {} Hz and does not blink", describe(owner),
                    frequencyHz);
    }
    return std::make_unique<BlinkComponent>(owner, frequencyHz);
}

void BlinkComponent::update(const Context& context) {
    if (!blinks(blinkFrequencyHz)) {
        return;
    }
    elapsedTime += context.timestamp;
    isLit = std::sin(kTwoPi * blinkFrequencyHz * elapsedTime) >= 0.0;
}

void BlinkComponent::render(SDL_Renderer* renderer) {
    (void)renderer;
    // This component does not render anything itself, it only vetoes drawing of the owner through
    // allowsDrawing().
}

REGISTER_COMPONENT(NeuronIDE::Component::kBlinker, BlinkComponent::createBlinker)
