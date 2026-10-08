#include "scene/components/BlinkComponent.hpp"
#include "scene/SceneObject.hpp"
#include "data_structures/Context.hpp"

#include "neuronide.pb.h"
#include "scene/components/ComponentRegistry.hpp"

#include <cmath>
void BlinkComponent::setFrequency(double freq) { blinkFrequencyHz = freq; }

std::unique_ptr<Component> BlinkComponent::createBlinker(
    const NeuronIDE::Component& protoComp, const std::shared_ptr<SceneObject>& owner) {
    return std::make_unique<BlinkComponent>(owner, protoComp.blinker().blink_frequency_hz());
}

void BlinkComponent::update(const Context& context) {
    // TODO: implement blinking logic based on blinkFrequencyHz and context.timestamp
   auto ownerPtr = owner.lock();
   if (ownerPtr == nullptr) {
        return;
   }
   if (blinkFrequencyHz <= 0.0) {
       ownerPtr->isVisible = true;
       return;
   }
   elapsedTime += context.timestamp;
   ownerPtr->isVisible = std::sin(2.0 * M_PI * blinkFrequencyHz * elapsedTime) >= 0.0;
}


void BlinkComponent::render(SDL_Renderer* renderer) {
    (void)renderer;
    // This component does not render anything itself, it only controls visibility of the owner
    // object.
}

REGISTER_COMPONENT(NeuronIDE::Component::kBlinker, BlinkComponent::createBlinker)