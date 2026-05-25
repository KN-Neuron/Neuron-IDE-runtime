#include "scene/components/BlinkComponent.hpp"

#include "neuronide.pb.h"
#include "scene/components/ComponentRegistry.hpp"

void BlinkComponent::setFrequency(double freq) { blinkFrequencyHz = freq; }

std::unique_ptr<Component> BlinkComponent::createBlinker(const NeuronIDE::Component& protoComp) {
    return std::make_unique<BlinkComponent>(protoComp.blinker().blink_frequency_hz());
}

REGISTER_COMPONENT(NeuronIDE::Component::kBlinker, BlinkComponent::createBlinker)